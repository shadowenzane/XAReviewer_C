#include "Exporter.h"

#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

// ---------------------------------------------------------------------------
// ImageExporter
// ---------------------------------------------------------------------------
bool ImageExporter::saveImage(const QImage& image, const QString& path)
{
    if (image.isNull())
        return false;

    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("jpg") || ext == QStringLiteral("jpeg")) {
        // JPEG：RGB888
        QImage rgb = image.convertToFormat(QImage::Format_RGB888);
        if (rgb.isNull())
            return false;
        cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
                    const_cast<uchar*>(rgb.constBits()),
                    static_cast<size_t>(rgb.bytesPerLine()));
        cv::Mat bgr;
        cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
        return cv::imwrite(path.toStdString(), bgr,
                           {cv::IMWRITE_JPEG_QUALITY, 95});
    }

    // 默认 PNG（保留灰度）
    QImage img = image;
    cv::Mat mat;
    if (img.format() == QImage::Format_Grayscale8) {
        mat = cv::Mat(img.height(), img.width(), CV_8UC1,
                      const_cast<uchar*>(img.constBits()),
                      static_cast<size_t>(img.bytesPerLine()))
                  .clone();
    } else {
        QImage rgb = img.convertToFormat(QImage::Format_RGB888);
        mat = cv::Mat(rgb.height(), rgb.width(), CV_8UC3,
                      const_cast<uchar*>(rgb.constBits()),
                      static_cast<size_t>(rgb.bytesPerLine()))
                  .clone();
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    }
    return cv::imwrite(path.toStdString(), mat);
}

// ---------------------------------------------------------------------------
// VideoExporter：FourCC 回退链 mp4v -> avc1 -> XVID -> MJPG
// ---------------------------------------------------------------------------
VideoExporter::VideoExporter(std::shared_ptr<DsaSequence> dsa, RenderOptions options,
                             int outWidth, int outHeight, QObject* parent)
    : QObject(parent), dsa_(std::move(dsa)), options_(std::move(options)),
      outWidth_(outWidth), outHeight_(outHeight)
{
}

bool VideoExporter::exportVideo(const QString& path, int fromFrame, int toFrame, int fps)
{
    if (!dsa_ || dsa_->frameCount() == 0) {
        emit finished(false, QStringLiteral("无序列可导出"));
        return false;
    }
    if (fps <= 0)
        fps = 30;

    fromFrame = qMax(0, fromFrame);
    toFrame = qMin(dsa_->frameCount() - 1, toFrame);
    if (fromFrame > toFrame)
        std::swap(fromFrame, toFrame);
    const int total = toFrame - fromFrame + 1;

    // 首帧确定分辨率
    QImage first = dsa_->renderedImage(fromFrame, options_);
    if (first.isNull()) {
        emit finished(false, QStringLiteral("首帧渲染失败"));
        return false;
    }

    int w = outWidth_ > 0 ? outWidth_ : first.width();
    int h = outHeight_ > 0 ? outHeight_ : first.height();
    // 偶数尺寸（部分编码器要求）
    w -= (w & 1);
    h -= (h & 1);

    // FourCC 回退链（对应 Python 的编码回退）
    const std::vector<std::string> fourccs = {"mp4v", "avc1", "XVID", "MJPG"};

    cv::VideoWriter writer;
    bool opened = false;
    for (const auto& cc : fourccs) {
        writer.open(path.toStdString(), cv::VideoWriter::fourcc(cc[0], cc[1], cc[2], cc[3]),
                    fps, cv::Size(w, h), true);
        if (writer.isOpened()) {
            opened = true;
            break;
        }
    }
    if (!opened) {
        emit finished(false, QStringLiteral("无法创建视频文件（已尝试 mp4v/avc1/XVID/MJPG）"));
        return false;
    }

    for (int f = fromFrame; f <= toFrame; ++f) {
        QImage img = dsa_->renderedImage(f, options_);
        if (img.isNull()) {
            emit finished(false, QStringLiteral("第 %1 帧渲染失败").arg(f + 1));
            return false;
        }
        // 统一转 BGR 并缩放到输出尺寸
        QImage rgb = img.convertToFormat(QImage::Format_RGB888)
                         .scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        cv::Mat frame(rgb.height(), rgb.width(), CV_8UC3,
                      const_cast<uchar*>(rgb.constBits()),
                      static_cast<size_t>(rgb.bytesPerLine()));
        cv::Mat bgr;
        cv::cvtColor(frame, bgr, cv::COLOR_RGB2BGR);
        writer.write(bgr);
        emit frameExported(f - fromFrame + 1, total);
    }
    writer.release();
    emit finished(true, QStringLiteral("已导出 %1 帧").arg(total));
    return true;
}
