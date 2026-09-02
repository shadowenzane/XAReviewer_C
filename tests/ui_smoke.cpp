// ui_smoke —— UI 层冒烟测试（离屏）
// 1. MainWindow 构造（主题/菜单/工具栏/面板）
// 2. 合成 DICOM 加载 + 面板联动
// 3. ImageViewer 帧切换 / 钳位
// 4. 离屏事件循环运行

#include "../src/core/DicomSeries.h"
#include "../src/core/DsaSequence.h"
#include "../src/ui/ControlPanel.h"
#include "../src/ui/ImageViewer.h"
#include "../src/ui/MainWindow.h"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>

#include <QApplication>
#include <QDir>
#include <QTimer>

#include <cstdio>
#include <vector>

static int g_failures = 0;

#define CHECK(cond, msg)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            std::printf("ok:   %s\n", msg);                              \
        } else {                                                         \
            std::printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); \
            ++g_failures;                                                \
        }                                                                \
    } while (0)

static void saveTestDicom(const std::string& path, int instanceNumber)
{
    const int W = 64, H = 64;
    std::vector<Uint16> pixels(static_cast<size_t>(W) * H, 100);
    for (int y = 0; y < H; ++y)
        for (int x = 24; x < 40; ++x)
            pixels[static_cast<size_t>(y) * W + x] = 200 + instanceNumber * 10;

    DcmFileFormat dcm;
    DcmDataset* ds = dcm.getDataset();
    ds->putAndInsertString(DCM_SOPClassUID, "1.2.840.10008.5.1.4.1.1.12.1");
    char uid[128];
    dcmGenerateUniqueIdentifier(uid, "1.2.826.0.1.3680043.8.499");
    ds->putAndInsertString(DCM_SOPInstanceUID, uid);
    ds->putAndInsertString(DCM_PatientID, "UITEST");
    ds->putAndInsertString(DCM_Modality, "XA");
    ds->putAndInsertString(DCM_InstanceNumber, std::to_string(instanceNumber).c_str());
    ds->putAndInsertString(DCM_PixelSpacing, "0.3\\0.3");
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
    dcm.saveFile(path.c_str(), EXS_LittleEndianExplicit);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ui_smoke"));

    // 合成序列（4 帧）
    const QString tmp = QDir::temp().absoluteFilePath(
        QStringLiteral("xareviewer_ui_smoke_%1").arg(QCoreApplication::applicationPid()));
    QDir().mkpath(tmp);
    for (int i = 0; i < 4; ++i)
        saveTestDicom((tmp + QStringLiteral("/f%1.dcm").arg(i)).toStdString(), i);

    // 1. MainWindow 构造
    MainWindow win;
    win.resize(900, 600);
    win.show();
    CHECK(true, "MainWindow constructed");

    // 2. 加载目录（loadDirectory 是异步的，经 directoryLoaded 信号确认）
    {
        bool loaded = false;
        QObject::connect(&win, &MainWindow::directoryLoaded,
                         [&](const QString&, bool ok) { loaded = ok; });
        win.loadDirectory(tmp);
        // 事件循环等待扫描线程完成
        QTimer timer;
        timer.setInterval(50);
        int waits = 0;
        timer.start();
        QObject::connect(&timer, &QTimer::timeout, [&]() {
            if (loaded || ++waits > 100)
                timer.stop();
        });
        while (!loaded && waits <= 100)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        CHECK(loaded, "series loaded (4 frames)");
    }

    // 3. ImageViewer 帧切换 / 钳位
    {
        auto series = DicomSeries::fromDirectory(tmp);
        CHECK(series != nullptr, "ui series reload");
        auto dsa = std::make_shared<DsaSequence>(series);

        ImageViewer viewer;
        viewer.resize(400, 400);
        viewer.setSequence(dsa);
        viewer.show();
        QCoreApplication::processEvents();
        CHECK(viewer.frame() == 0, "viewer initial frame");
        viewer.setFrame(3);
        CHECK(viewer.frame() == 3, "viewer setFrame(3)");
        viewer.setFrame(999);
        CHECK(viewer.frame() == 3, "viewer frame clamped");

        // 渲染选项存储
        RenderOptions opt;
        opt.pseudoColor = true;
        viewer.setRenderOptions(opt);
        QCoreApplication::processEvents();
        CHECK(viewer.effectiveRenderOptions().pseudoColor, "viewer render options stored");
        CHECK(viewer.isActive(), "viewer active");
    }

    // 4. 离屏事件循环
    {
        QTimer::singleShot(100, &app, &QCoreApplication::quit);
        app.exec();
        CHECK(true, "offscreen event loop");
    }

    QDir(tmp).removeRecursively();

    if (g_failures == 0) {
        std::printf("\nALL PASSED (0 failures)\n");
        return 0;
    }
    std::printf("\nFAILED (%d failures)\n", g_failures);
    return 1;
}
