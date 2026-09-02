#pragma once

#include <QImage>
#include <QObject>
#include <QString>

#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "DicomSeries.h"

// 渲染选项（对应 Python RenderOptions / set_render_options）
struct RenderOptions {
    double windowWidth = 0.0;    // <=0 表示未设置（触发自动窗）
    double windowCenter = 0.0;
    bool pseudoColor = false;
    bool subtraction = false;    // 减影模式：渲染 frame 与 maskFrame 之差
    int maskFrame = 0;
    QString colorMap = QStringLiteral("JET");
    double maskSmoothSigma = 0.0;
};

// DSA 序列渲染：窗宽窗位 / 减影 / 伪彩，带 LRU 缓存（上限 100）
class DsaSequence : public QObject
{
    Q_OBJECT

public:
    explicit DsaSequence(std::shared_ptr<DicomSeries> series);

    int frameCount() const;

    // 自动窗：ww=(max-min)*1.2, wc=(min+max)/2；CT 类兜底 800/100
    std::pair<double, double> autoWindow(int frame);

    // 窗宽窗位公式：clip -> 线性 0-255 -> gamma0.5 -> min-max 重拉伸
    static cv::Mat applyWindowLevel(const cv::Mat& gray32f, double windowWidth,
                                    double windowCenter);

    // 减影差值图（contrast - mask，可对 mask 做 ITK 高斯平滑）
    cv::Mat subtractedOriginal(int maskIndex, int contrastIndex, double maskSigma);

    // 渲染（LRU 缓存，key：帧号/窗宽窗位/伪彩/减影/蒙片）
    QImage renderedImage(int frame, const RenderOptions& options);

    void clearCache();

private:
    struct CacheKey {
        int frame;
        qint64 ww;
        qint64 wc;
        bool pseudoColor;
        bool subtraction;
        int maskFrame;
        QString colorMap;
        qint64 maskSigmaBits;

        bool operator==(const CacheKey& o) const
        {
            return frame == o.frame && ww == o.ww && wc == o.wc &&
                   pseudoColor == o.pseudoColor && subtraction == o.subtraction &&
                   maskFrame == o.maskFrame && colorMap == o.colorMap &&
                   maskSigmaBits == o.maskSigmaBits;
        }
    };
    struct CacheKeyHash {
        size_t operator()(const CacheKey& k) const
        {
            size_t h = std::hash<int>()(k.frame);
            h ^= std::hash<qint64>()(k.ww) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<qint64>()(k.wc) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(static_cast<int>(k.pseudoColor)) + 0x9e3779b9;
            h ^= std::hash<int>()(static_cast<int>(k.subtraction)) + 0x9e3779b9;
            h ^= std::hash<int>()(k.maskFrame) + 0x9e3779b9;
            h ^= qHash(k.colorMap) + 0x9e3779b9;
            h ^= std::hash<qint64>()(k.maskSigmaBits) + 0x9e3779b9;
            return h;
        }
    };

    QImage renderUncached(int frame, const RenderOptions& opt);

    std::shared_ptr<DicomSeries> series_;

    mutable std::mutex cacheMutex_;
    using ListIter = std::list<std::pair<CacheKey, QImage>>::iterator;
    std::list<std::pair<CacheKey, QImage>> lruList_; // front = 最近使用
    std::unordered_map<CacheKey, ListIter, CacheKeyHash> lruMap_;
    static constexpr size_t kMaxCacheEntries = 100;
};
