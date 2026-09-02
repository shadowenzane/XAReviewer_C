#include "DsaSequence.h"
#include "ImagePipeline.h"

#include <opencv2/imgproc.hpp>

#include <cstring>

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------
DsaSequence::DsaSequence(std::shared_ptr<DicomSeries> series)
    : series_(std::move(series))
{
}

int DsaSequence::frameCount() const
{
    return series_ ? series_->frameCount() : 0;
}

void DsaSequence::clearCache()
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    lruMap_.clear();
    lruList_.clear();
}

// ---------------------------------------------------------------------------
// 自动窗（对应 Python set_default_window_by_modality L1657-1664）
// ww = (max-min)*1.2，wc = (min+max)/2
// ---------------------------------------------------------------------------
std::pair<double, double> DsaSequence::autoWindow(int frame)
{
    if (!series_)
        return {500.0, 50.0};

    // CT 类兜底 800/100（对应 Python CT 预设）
    if (series_->patientInfo().isCtLike)
        return {800.0, 100.0};

    cv::Mat gray = series_->loadFrameGray32(frame);
    if (gray.empty())
        return {500.0, 50.0};

    double mn, mx;
    cv::minMaxLoc(gray, &mn, &mx);
    if (mx <= mn)
        return {500.0, 50.0};

    const double ww = (mx - mn) * 1.2;
    const double wc = (mn + mx) / 2.0;
    return {ww, wc};
}

// ---------------------------------------------------------------------------
// 窗宽窗位公式（对应 Python apply_window_level L2067-2102）
// clip[wc-ww/2, wc+ww/2] -> 线性 0-255 -> gamma0.5 -> min-max 重拉伸 -> 0-255
// ---------------------------------------------------------------------------
cv::Mat DsaSequence::applyWindowLevel(const cv::Mat& gray32f, double windowWidth,
                                      double windowCenter)
{
    if (gray32f.empty())
        return {};

    if (windowWidth <= 0.0)
        windowWidth = 1.0;

    // clip[wc-ww/2, wc+ww/2] -> 线性 0-255
    const double minVal = windowCenter - windowWidth / 2.0;
    const double scale = 255.0 / windowWidth;
    cv::Mat clipped;
    cv::max(gray32f, cv::Scalar(minVal), clipped);
    cv::min(clipped, cv::Scalar(minVal + windowWidth), clipped);
    cv::Mat norm;
    clipped.convertTo(norm, CV_32FC1, scale, -minVal * scale);

    // gamma 0.5（提高亮度；常数因子被后续 min-max 拉伸归一，与 Python 255*(x/255)^0.5 等价）
    cv::Mat gamma;
    cv::sqrt(norm, gamma);

    // min-max 重拉伸到 0-255（对齐 L2091-2097）
    double mappedMin = 0.0, mappedMax = 0.0;
    cv::minMaxLoc(gamma, &mappedMin, &mappedMax);
    cv::Mat out;
    if (mappedMax > mappedMin) {
        gamma.convertTo(out, CV_8UC1, 255.0 / (mappedMax - mappedMin),
                        -mappedMin * 255.0 / (mappedMax - mappedMin));
    } else {
        out = cv::Mat(gamma.size(), CV_8UC1, cv::Scalar(128));
    }
    return out;
}

// ---------------------------------------------------------------------------
// 减影原图（对应 get_subtracted_image L2104-2140）
// ---------------------------------------------------------------------------
cv::Mat DsaSequence::subtractedOriginal(int maskIndex, int contrastIndex, double maskSigma)
{
    if (!series_)
        return {};

    cv::Mat mask = series_->loadFrameGray32(maskIndex);
    cv::Mat contrast = series_->loadFrameGray32(contrastIndex);
    if (mask.empty() || contrast.empty())
        return {};

    // 尺寸不一致时以掩模为准缩放（对齐 L2123-2124）
    if (mask.size() != contrast.size())
        cv::resize(contrast, contrast, mask.size(), 0, 0, cv::INTER_LINEAR);

    // 掩模平滑（可选，减少噪声）
    if (maskSigma > 0.0)
        mask = ImagePipeline::itkGaussianSmooth(mask, maskSigma);

    return contrast - mask;
}

// ---------------------------------------------------------------------------
// 渲染（带 LRU 缓存）
// ---------------------------------------------------------------------------
QImage DsaSequence::renderedImage(int frame, const RenderOptions& options)
{
    if (!series_)
        return {};

    // 解析自动窗
    RenderOptions opt = options;
    if (opt.windowWidth <= 0.0) {
        const auto aw = autoWindow(frame);
        opt.windowWidth = aw.first;
        opt.windowCenter = aw.second;
    }

    // 缓存键（double 转位模式，避免浮点比较）
    CacheKey key{frame,
                 *reinterpret_cast<const qint64*>(&opt.windowWidth),
                 *reinterpret_cast<const qint64*>(&opt.windowCenter),
                 opt.pseudoColor,
                 opt.subtraction,
                 opt.subtraction ? opt.maskFrame : 0,
                 opt.pseudoColor ? opt.colorMap : QString(),
                 *reinterpret_cast<const qint64*>(&opt.maskSmoothSigma)};

    // 命中缓存
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = lruMap_.find(key);
        if (it != lruMap_.end()) {
            lruList_.splice(lruList_.begin(), lruList_, it->second); // 提到前端
            return it->second->second;
        }
    }

    // 未命中：渲染
    QImage img = renderUncached(frame, opt);
    if (img.isNull())
        return {};

    // 写入缓存
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        lruList_.emplace_front(key, img);
        lruMap_[key] = lruList_.begin();
        if (lruMap_.size() > kMaxCacheEntries) {
            lruMap_.erase(lruList_.back().first);
            lruList_.pop_back();
        }
    }
    return img;
}

QImage DsaSequence::renderUncached(int frame, const RenderOptions& opt)
{
    cv::Mat gray8;

    if (opt.subtraction) {
        // 减影：差值图 -> 窗位
        cv::Mat diff = subtractedOriginal(opt.maskFrame, frame, opt.maskSmoothSigma);
        if (diff.empty())
            return {};
        gray8 = applyWindowLevel(diff, opt.windowWidth, opt.windowCenter);
    } else {
        cv::Mat gray32 = series_->loadFrameGray32(frame);
        if (gray32.empty())
            return {};
        gray8 = applyWindowLevel(gray32, opt.windowWidth, opt.windowCenter);
    }

    if (gray8.empty())
        return {};

    // 伪彩（10 种 LUT）
    cv::Mat out = gray8;
    if (opt.pseudoColor)
        out = ImagePipeline::applyPseudoColor(gray8, opt.colorMap);

    // cv::Mat(BGR/Gray) -> QImage
    QImage::Format fmt = (out.channels() == 3) ? QImage::Format_RGB888
                                               : QImage::Format_Grayscale8;
    if (out.channels() == 3)
        cv::cvtColor(out, out, cv::COLOR_BGR2RGB);
    return QImage(out.data, out.cols, out.rows,
                  static_cast<qsizetype>(out.step), fmt)
        .copy(); // 深拷贝脱离 Mat 生命周期
}
