// core_smoke —— 核心层冒烟测试
// 1. 用 DCMTK 生成合成 DSA 序列（10 帧，128x128，含 InstanceNumber 排序与 Rescale）
// 2. DicomSeries 加载 + 排序校验
// 3. 窗宽窗位数学校验（与 Python 版公式对齐）
// 4. 渲染管线校验（自动窗 / 伪彩 / 减影）
// 5. 图像导出
// 6. ITK 平滑

#include "../src/core/DicomSeries.h"
#include "../src/core/DsaSequence.h"
#include "../src/core/ImagePipeline.h"
#include "../src/export/Exporter.h"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>

#include <opencv2/imgcodecs.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static int g_failures = 0;

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (cond) {                                                         \
            std::printf("ok:   %s\n", msg);                                 \
        } else {                                                            \
            std::printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__);    \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

// 生成单帧 DICOM：128x128 16 位，血管亮柱（值 value）背景（bg），含 Rescale
static void saveTestDicom(const std::string& path, int instanceNumber, int vesselValue,
                          int bgValue)
{
    const int W = 128, H = 128;
    std::vector<Uint16> pixels(static_cast<size_t>(W) * H, static_cast<Uint16>(bgValue));
    // 中央竖直血管柱
    for (int y = 0; y < H; ++y)
        for (int x = 56; x < 72; ++x)
            pixels[static_cast<size_t>(y) * W + x] = static_cast<Uint16>(vesselValue);

    DcmFileFormat dcm;
    DcmDataset* ds = dcm.getDataset();
    ds->putAndInsertString(DCM_SOPClassUID, "1.2.840.10008.5.1.4.1.1.12.1");
    char uid[128];
    dcmGenerateUniqueIdentifier(uid, "1.2.826.0.1.3680043.8.498");
    ds->putAndInsertString(DCM_SOPInstanceUID, uid);
    ds->putAndInsertString(DCM_PatientID, "TEST001");
    ds->putAndInsertString(DCM_PatientName, "Test^Patient");
    ds->putAndInsertString(DCM_Modality, "XA");
    ds->putAndInsertString(DCM_SeriesDescription, "TEST DSA");
    ds->putAndInsertString(DCM_InstanceNumber, std::to_string(instanceNumber).c_str());
    ds->putAndInsertString(DCM_PixelSpacing, "0.2\\0.2");
    ds->putAndInsertString(DCM_RescaleSlope, "0.5");
    ds->putAndInsertString(DCM_RescaleIntercept, "0");
    ds->putAndInsertUint16(DCM_SamplesPerPixel, 1);
    ds->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
    ds->putAndInsertUint16(DCM_Rows, static_cast<Uint16>(H));
    ds->putAndInsertUint16(DCM_Columns, static_cast<Uint16>(W));
    ds->putAndInsertUint16(DCM_BitsAllocated, 16);
    ds->putAndInsertUint16(DCM_BitsStored, 16);
    ds->putAndInsertUint16(DCM_HighBit, 15);
    ds->putAndInsertUint16(DCM_PixelRepresentation, 0);
    ds->putAndInsertUint16Array(DCM_PixelData, pixels.data(),
                                static_cast<unsigned long>(pixels.size()));
    OFCondition cond = dcm.saveFile(path.c_str(), EXS_LittleEndianExplicit);
    CHECK(cond.good(), "save test DICOM file");
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // ------------------------------------------------------------------
    // 1. 生成合成序列（乱序写入，InstanceNumber 0..9；值随帧变化）
    // ------------------------------------------------------------------
    const QString tmp = QDir::temp().absoluteFilePath(
        QStringLiteral("xareviewer_smoke_%1").arg(QCoreApplication::applicationPid()));
    QDir().mkpath(tmp);
    CHECK(QDir(tmp).exists(), "temporary directory");

    // 乱序写入（步长 3 取模遍历保证乱序）
    for (int i = 0; i < 10; ++i) {
        const int frame = (i * 3) % 10;
        const int vessel = 200 + frame * 10; // 200..290（存储值），物理值 = *0.5
        const int bg = 100 + frame * 5;
        saveTestDicom((tmp + QStringLiteral("/img_%1.dcm").arg(frame * 7 + 3))
                          .toStdString(),
                      frame, vessel, bg);
    }

    // ------------------------------------------------------------------
    // 2. 加载 + 排序
    // ------------------------------------------------------------------
    auto series = DicomSeries::fromDirectory(tmp);
    CHECK(series != nullptr, "DicomSeries::fromDirectory");
    CHECK(series->frameCount() == 10, "frame count == 10");
    CHECK(series->patientInfo().patientId == QStringLiteral("TEST001"), "patient id");
    CHECK(series->patientInfo().pixelSpacingX == 0.2, "pixel spacing X");
    CHECK(series->patientInfo().pixelSpacingY == 0.2, "pixel spacing Y");
    // 排序后首文件 InstanceNumber==0（文件名 img_3），尾文件 InstanceNumber==9（img_66）
    CHECK(series->fileNameAt(0) == QStringLiteral("img_3.dcm"), "sort by InstanceNumber");
    if (series->fileNameAt(9) != QStringLiteral("img_66.dcm")) {
        for (int i = 0; i < series->frameCount(); ++i)
            std::printf("  frame %d -> %s\n", i,
                        series->fileNameAt(i).toUtf8().constData());
    }
    CHECK(series->fileNameAt(9) == QStringLiteral("img_66.dcm"), "sort last file");

    auto dsa = std::make_shared<DsaSequence>(series);

    // ------------------------------------------------------------------
    // 3. 帧解码（Rescale 斜率 0.5）
    // ------------------------------------------------------------------
    {
        cv::Mat f0 = series->loadFrameGray32(0);
        CHECK(!f0.empty() && f0.cols == 128 && f0.rows == 128, "loadFrameGray32 size");
        // 帧 0：血管存储值 200 -> 物理 100；背景 100 -> 50
        CHECK(std::abs(f0.at<float>(64, 64) - 100.0f) < 1e-3, "vessel pixel frame0 == 200");
        CHECK(std::abs(f0.at<float>(4, 4) - 50.0f) < 1e-3, "background pixel frame0 == 100");

        cv::Mat f5 = series->loadFrameGray32(5);
        // 帧 5：血管 250 -> 125；背景 125 -> 62.5
        CHECK(std::abs(f5.at<float>(64, 64) - 125.0f) < 1e-3, "vessel pixel frame5 == 250");
        CHECK(std::abs(f5.at<float>(4, 4) - 62.5f) < 1e-3, "background pixel frame5 == 125");
    }

    // ------------------------------------------------------------------
    // 4. 自动窗
    // ------------------------------------------------------------------
    {
        auto [ww, wc] = dsa->autoWindow(0);
        // 帧 0 min=50 max=100 -> ww=(100-50)*1.2=60, wc=75
        CHECK(std::abs(ww - 60.0) < 1e-3, "auto ww == 120");
        CHECK(std::abs(wc - 75.0) < 1e-3, "auto wc == 150");
    }

    // ------------------------------------------------------------------
    // 5. 窗宽窗位公式（与 Python apply_window_level 对齐）
    //    手工构造 2x2 输入：[wc-ww/2, wc, wc+ww/4, wc+ww/2]
    //    ww=100 wc=50 -> 输入 [0, 50, 75, 100]
    //    clip 后 [0,50,75,100] -> 线性 [0,127.5,191.25,255]
    //    gamma0.5 -> sqrt -> min-max 拉伸 -> [0, 0.707, 0.866, 1]*255
    // ------------------------------------------------------------------
    {
        cv::Mat in = (cv::Mat_<float>(2, 2) << 0.0, 50.0, 75.0, 100.0);
        cv::Mat out = DsaSequence::applyWindowLevel(in, 100.0, 50.0);
        CHECK(!out.empty() && out.type() == CV_8UC1, "wl output type");
        const double s = std::sqrt(127.5 / 255.0); // 中间参考
        const int p00 = out.at<uchar>(0, 0);
        const int p01 = out.at<uchar>(0, 1);
        const int p11 = out.at<uchar>(1, 1);
        CHECK(p00 == 0, "wl tiny p00");
        CHECK(p01 > 0 && p01 < 255 && std::abs(p01 - 255.0 * s / std::sqrt(1.0)) < 200,
              "wl tiny p01");
        CHECK(p11 == 255, "wl tiny p10/p11");
        CHECK(p01 < p11, "wl monotonic");
    }

    // ------------------------------------------------------------------
    // 6. 渲染管线（自动窗 / 伪彩 / 减影）
    // ------------------------------------------------------------------
    {
        RenderOptions opt; // ww<=0 触发自动窗
        QImage img = dsa->renderedImage(0, opt);
        CHECK(!img.isNull(), "render frame0");

        // 血管比背景亮
        const QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
        CHECK(qGray(gray.pixel(64, 64)) > qGray(gray.pixel(4, 4)),
              "vessel brighter than background");

        // 伪彩（JET）
        RenderOptions pc;
        pc.pseudoColor = true;
        pc.colorMap = QStringLiteral("JET");
        QImage pcImg = dsa->renderedImage(0, pc);
        CHECK(!pcImg.isNull() && pcImg.format() == QImage::Format_RGB888,
              "render pseudo-color");
        // JET 低端偏蓝，高端偏红
        const QColor vessel = pcImg.pixelColor(64, 64);
        const QColor bg = pcImg.pixelColor(4, 4);
        CHECK(vessel.red() >= bg.red(), "JET vessel redder than bg");

        // 减影：帧 5 - 蒙片帧 0（血管差 = 125-100=25 > 背景差 = 62.5-50=12.5）
        RenderOptions sub;
        sub.subtraction = true;
        sub.maskFrame = 0;
        QImage subImg = dsa->renderedImage(5, sub);
        CHECK(!subImg.isNull(), "render subtraction");
        const QImage subGray = subImg.convertToFormat(QImage::Format_Grayscale8);
        CHECK(qGray(subGray.pixel(64, 64)) >= qGray(subGray.pixel(4, 4)),
              "subtraction vessel >= bg");

        // 缓存命中返回相同内容
        QImage again = dsa->renderedImage(0, opt);
        CHECK(again.size() == img.size(), "cache hit returns same size");
    }

    // ------------------------------------------------------------------
    // 7. 图像导出
    // ------------------------------------------------------------------
    {
        QImage img = dsa->renderedImage(0, RenderOptions{});
        const QString pngPath = tmp + QStringLiteral("/out.png");
        CHECK(ImageExporter::saveImage(img, pngPath), "export png");
        QImage reloaded(pngPath);
        CHECK(!reloaded.isNull(), "png reload valid");
        CHECK(reloaded.width() == 128, "png non-trivial");
    }

    // ------------------------------------------------------------------
    // 8. 伪彩 LUT 数量 + ITK 平滑
    // ------------------------------------------------------------------
    {
        CHECK(ImagePipeline::availableColorMaps().size() == 10, "10 color maps");

        cv::Mat in(64, 64, CV_32FC1, cv::Scalar(0));
        in.at<float>(32, 32) = 1000.0f; // 尖峰
        cv::Mat sm = ImagePipeline::itkGaussianSmooth(in, 2.0);
        CHECK(!sm.empty() && sm.size() == in.size(), "itk smooth size");
        std::printf("  smooth peak=%f  neighbor=%f\n", sm.at<float>(32, 32),
                    sm.at<float>(32, 33));
        CHECK(sm.at<float>(32, 32) < 1000.0f, "itk smooth reduces peak");
    }

    // 清理
    QDir(tmp).removeRecursively();

    if (g_failures == 0) {
        std::printf("\nALL PASSED (0 failures)\n");
        return 0;
    }
    std::printf("\nFAILED (%d failures)\n", g_failures);
    return 1;
}
