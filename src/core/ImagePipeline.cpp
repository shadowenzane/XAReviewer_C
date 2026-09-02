#include "ImagePipeline.h"

#include <QStringList>

#include <itkImportImageFilter.h>
#include <itkImage.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// 10 种伪彩 LUT（256 项，BGR 顺序），与 Python 版伪彩表逐一对齐
// ---------------------------------------------------------------------------
namespace {

struct Rgb8 {
    uchar b, g, r;
};

using Lut = std::vector<Rgb8>;

// 由若干 (pos, r, g, b) 停靠点线性插值生成 256 项 LUT
Lut buildLut(std::vector<std::array<float, 4>> stops)
{
    Lut lut(256);
    std::sort(stops.begin(), stops.end(),
              [](const auto& a, const auto& b) { return a[0] < b[0]; });
    for (int i = 0; i < 256; ++i) {
        const float t = i / 255.0f;
        // 找到停靠区间
        size_t k = 0;
        while (k + 1 < stops.size() && stops[k + 1][0] < t)
            ++k;
        float r, g, b;
        if (k + 1 >= stops.size()) {
            r = stops.back()[1]; g = stops.back()[2]; b = stops.back()[3];
        } else {
            const auto& a = stops[k];
            const auto& c = stops[k + 1];
            const float span = c[0] - a[0];
            const float f = span > 0.0f ? (t - a[0]) / span : 0.0f;
            r = a[1] + (c[1] - a[1]) * f;
            g = a[2] + (c[2] - a[2]) * f;
            b = a[3] + (c[3] - a[3]) * f;
        }
        lut[i] = {static_cast<uchar>(cv::saturate_cast<uchar>(b)),
                  static_cast<uchar>(cv::saturate_cast<uchar>(g)),
                  static_cast<uchar>(cv::saturate_cast<uchar>(r))};
    }
    return lut;
}

// JET：经典 jet 色带
Lut jetLut()
{
    Lut lut(256);
    for (int i = 0; i < 256; ++i) {
        const float t = i / 255.0f * 4.0f;
        float r = std::min(std::max(t - 1.5f, 0.0f), 1.0f);
        float g = std::min(std::max(1.5f - std::fabs(t - 2.0f), 0.0f), 1.0f);
        float b = std::min(std::max(1.5f - std::fabs(t - 1.0f), 0.0f), 1.0f);
        lut[i] = {static_cast<uchar>(b * 255), static_cast<uchar>(g * 255),
                  static_cast<uchar>(r * 255)};
    }
    return lut;
}

// 热金属（HOT）：黑 -> 红 -> 黄 -> 白
Lut hotLut()
{
    return buildLut({{{0.0f, 0, 0, 0}, {0.33f, 255, 0, 0}, {0.66f, 255, 255, 0}, {1.0f, 255, 255, 255}}});
}

// 骨（BONE）：深蓝灰 -> 浅灰白
Lut boneLut()
{
    return buildLut({{{0.0f, 0, 0, 20}, {0.35f, 60, 70, 90}, {0.7f, 170, 175, 185}, {1.0f, 255, 255, 255}}});
}

// 彩虹
Lut rainbowLut()
{
    // HSL 彩虹：紫->蓝->青->绿->黄->红
    Lut lut(256);
    for (int i = 0; i < 256; ++i) {
        const float t = i / 255.0f;
        // 简化 HSV：h 从 280 度降到 0 度
        const float h = (1.0f - t) * 280.0f / 60.0f; // [0, 4.67]
        const float s = 1.0f, v = 1.0f;
        const int hi = static_cast<int>(h) % 6;
        const float f = h - std::floor(h);
        const float p = v * (1 - s);
        const float q = v * (1 - f * s);
        const float t2 = v * (1 - (1 - f) * s);
        float r, g, b;
        switch (hi) {
        case 0: r = v; g = t2; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t2; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t2; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
        }
        lut[i] = {static_cast<uchar>(b * 255), static_cast<uchar>(g * 255),
                  static_cast<uchar>(r * 255)};
    }
    return lut;
}

// 彩色（简单三段）
Lut colorLut()
{
    return buildLut({{{0.0f, 0, 0, 60}, {0.5f, 255, 60, 0}, {1.0f, 255, 255, 80}}});
}

// 熔岩
Lut lavaLut()
{
    return buildLut({{{0.0f, 0, 0, 0}, {0.25f, 80, 0, 20}, {0.55f, 220, 40, 0}, {0.8f, 255, 160, 20}, {1.0f, 255, 255, 200}}});
}

// 冰（COOL）
Lut coolLut()
{
    Lut lut(256);
    for (int i = 0; i < 256; ++i) {
        lut[i] = {static_cast<uchar>(i), static_cast<uchar>(i),
                  static_cast<uchar>(255 - i)}; // B=G=t, R=1-t（青->洋红）
        // 交换为更常见的 cool：cyan -> magenta
    }
    return lut;
}

// 铜（COPPER）
Lut copperLut()
{
    Lut lut(256);
    for (int i = 0; i < 256; ++i) {
        const float t = i / 255.0f;
        lut[i] = {static_cast<uchar>(t * 120), static_cast<uchar>(t * 180),
                  static_cast<uchar>(t * 255)};
    }
    return lut;
}

// 春（SPRING）
Lut springLut()
{
    Lut lut(256);
    for (int i = 0; i < 256; ++i) {
        const float t = i / 255.0f;
        lut[i] = {static_cast<uchar>((1 - t) * 255), static_cast<uchar>(t * 255), 0};
    }
    return lut;
}

// HSV
Lut hsvLut()
{
    Lut lut(256);
    for (int i = 0; i < 256; ++i) {
        const float h = i / 255.0f * 360.0f;
        const float s = 1.0f, v = 1.0f;
        const float c = v * s;
        const float x = c * (1 - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        const float m = v - c;
        float r = 0, g = 0, b = 0;
        if (h < 60)       { r = c; g = x; }
        else if (h < 120) { r = x; g = c; }
        else if (h < 180) { g = c; b = x; }
        else if (h < 240) { g = x; b = c; }
        else if (h < 300) { r = x; b = c; }
        else              { r = c; b = x; }
        lut[i] = {static_cast<uchar>((b + m) * 255), static_cast<uchar>((g + m) * 255),
                  static_cast<uchar>((r + m) * 255)};
    }
    return lut;
}

const Lut& lutFor(const QString& name)
{
    static const std::map<QString, Lut> luts = {
        {QStringLiteral("JET"), jetLut()},
        {QStringLiteral("HOT"), hotLut()},
        {QStringLiteral("BONE"), boneLut()},
        {QStringLiteral("RAINBOW"), rainbowLut()},
        {QStringLiteral("COLOR"), colorLut()},
        {QStringLiteral("LAVA"), lavaLut()},
        {QStringLiteral("COOL"), coolLut()},
        {QStringLiteral("COPPER"), copperLut()},
        {QStringLiteral("SPRING"), springLut()},
        {QStringLiteral("HSV"), hsvLut()},
    };
    static const Lut gray3ch = [] {
        Lut l(256);
        for (int i = 0; i < 256; ++i)
            l[i] = {static_cast<uchar>(i), static_cast<uchar>(i),
                    static_cast<uchar>(i)};
        return l;
    }();
    const auto it = luts.find(name.toUpper());
    return it != luts.end() ? it->second : gray3ch;
}

} // namespace

QStringList ImagePipeline::availableColorMaps()
{
    return {QStringLiteral("JET"), QStringLiteral("HOT"), QStringLiteral("BONE"),
            QStringLiteral("RAINBOW"), QStringLiteral("COLOR"), QStringLiteral("LAVA"),
            QStringLiteral("COOL"), QStringLiteral("COPPER"), QStringLiteral("SPRING"),
            QStringLiteral("HSV")};
}

cv::Mat ImagePipeline::applyPseudoColor(const cv::Mat& gray8, const QString& colorMap)
{
    if (gray8.empty())
        return {};

    cv::Mat gray;
    if (gray8.type() != CV_8UC1)
        gray8.convertTo(gray, CV_8UC1);
    else
        gray = gray8;

    const Lut& lut = lutFor(colorMap);

    cv::Mat out(gray.size(), CV_8UC3);
    const int rows = gray.rows, cols = gray.cols;
    for (int y = 0; y < rows; ++y) {
        const uchar* src = gray.ptr<uchar>(y);
        Rgb8* dst = out.ptr<Rgb8>(y);
        for (int x = 0; x < cols; ++x)
            dst[x] = lut[src[x]];
    }
    return out;
}

cv::Mat ImagePipeline::enhanceUltrasound(const cv::Mat& gray8)
{
    if (gray8.empty())
        return {};

    cv::Mat gray;
    if (gray8.type() != CV_8UC1)
        gray8.convertTo(gray, CV_8UC1);
    else
        gray = gray8;

    // CLAHE：clipLimit=3.0，tile 8x8（对齐 Python enhance_ultrasound）
    auto clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
    cv::Mat out;
    clahe->apply(gray, out);
    return out;
}

cv::Mat ImagePipeline::itkGaussianSmooth(const cv::Mat& gray32, double sigma)
{
    if (gray32.empty() || gray32.type() != CV_32FC1 || sigma <= 0.0)
        return gray32.clone();

    using ImageType = itk::Image<float, 2>;

    const int w = gray32.cols, h = gray32.rows;

    // cv::Mat -> ITK（拷贝导入，保证内存连续且生命周期独立）
    auto import = itk::ImportImageFilter<float, 2>::New();
    ImageType::SizeType size;
    size[0] = w;
    size[1] = h;
    ImageType::IndexType start;
    start.Fill(0);
    ImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);
    import->SetRegion(region);
    ImageType::SpacingType spacing;
    spacing.Fill(1.0);
    import->SetSpacing(spacing);
    ImageType::PointType origin;
    origin.Fill(0.0);
    import->SetOrigin(origin);
    // ITK 行主序与 cv::Mat 一致（逐行复制，忽略可能的行 padding）
    std::vector<float> buffer(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        std::memcpy(buffer.data() + static_cast<size_t>(y) * w, gray32.ptr<float>(y),
                    static_cast<size_t>(w) * sizeof(float));
    const bool importImageFilterWillOwnBuffer = false;
    import->SetImportPointer(buffer.data(), static_cast<unsigned long>(buffer.size()),
                             importImageFilterWillOwnBuffer);

    auto filter = itk::SmoothingRecursiveGaussianImageFilter<ImageType, ImageType>::New();
    filter->SetInput(import->GetOutput());
    filter->SetSigma(sigma);
    filter->Update();

    // ITK -> cv::Mat
    const ImageType* outImg = filter->GetOutput();
    cv::Mat out(h, w, CV_32FC1);
    for (int y = 0; y < h; ++y)
        std::memcpy(out.ptr<float>(y),
                    outImg->GetBufferPointer() + static_cast<size_t>(y) * w,
                    static_cast<size_t>(w) * sizeof(float));
    return out;
}
