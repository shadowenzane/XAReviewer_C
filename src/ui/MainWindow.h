#pragma once

#include <QMainWindow>

#include <memory>

#include "../core/DicomSeries.h"
#include "../core/DsaSequence.h"
#include "ImageViewer.h"

class ControlPanel;
class QGridLayout;
class QTimer;
class QProgressBar;
class QLabel;

// 主窗口：菜单 / 工具栏 / 视图网格 / 控制面板
// 对应 Python XAReviewer 主界面
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // 单视图 / 2x2
    enum class LayoutMode { Single, Grid2x2 };
    void setLayoutMode(LayoutMode mode);

public slots:
    // 打开目录（后台扫描，进度条显示）
    void loadDirectory(const QString& dirPath);
    void openDirectoryDialog();
    void queryPacs();

    // 播放控制
    void togglePlay(bool playing);

    // 导出
    void exportImage();
    void exportVideo();

    // 增强
    void enhanceUltrasound();

    // 视图切换
    void setSingleLayout();
    void setGridLayout();

signals:
    void directoryLoaded(const QString& dirPath, bool ok);

private:
    void buildMenus();
    void buildToolBar();
    void setupCentral();
    ImageViewer* addViewer();
    void connectViewer(ImageViewer* viewer);
    void applyRenderOptionsToAll();
    void showStatus(const QString& msg);

    QWidget* gridHost_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QList<ImageViewer*> viewers_;
    ImageViewer* active_ = nullptr;

    ControlPanel* panel_ = nullptr;
    std::shared_ptr<DicomSeries> series_;
    std::shared_ptr<DsaSequence> dsa_;

    QTimer* playTimer_ = nullptr;
    bool playing_ = false;

    QAction* singleAction_ = nullptr;
    QAction* gridAction_ = nullptr;
};
