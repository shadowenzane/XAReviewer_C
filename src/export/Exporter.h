#pragma once

#include <QImage>
#include <QObject>
#include <QString>

#include <memory>

#include "../core/DsaSequence.h"

// 静态图导出（PNG 保留灰度 / JPEG 高质量）
class ImageExporter
{
public:
    // 依据扩展名选择编码器；png 保留 Grayscale8，jpg 转 RGB888（质量 95）
    static bool saveImage(const QImage& image, const QString& path);
};

// 视频导出（OpenCV VideoWriter，FourCC 回退链）
// 在调用线程内同步逐帧导出；进度经 frameExported 信号报告
class VideoExporter : public QObject
{
    Q_OBJECT

public:
    // outWidth/outHeight <= 0 表示使用原始分辨率
    VideoExporter(std::shared_ptr<DsaSequence> dsa, RenderOptions options,
                  int outWidth, int outHeight, QObject* parent = nullptr);

    // 导出 [fromFrame, toFrame]（含端点）
    bool exportVideo(const QString& path, int fromFrame, int toFrame, int fps);

signals:
    void frameExported(int frame, int total);
    void finished(bool ok, const QString& message);

private:
    std::shared_ptr<DsaSequence> dsa_;
    RenderOptions options_;
    int outWidth_ = 0;
    int outHeight_ = 0;
};
