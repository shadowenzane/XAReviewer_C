#pragma once

#include <QGraphicsScene>
#include <QGraphicsView>

#include <memory>

#include "../core/DsaSequence.h"

class DistanceItem;
class AngleItem;

// 图像视图：缩放/平移/窗宽窗位拖拽/测量工具
// 对应 Python ImageViewer（QGraphicsView 封装）
class ImageViewer : public QGraphicsView
{
    Q_OBJECT

public:
    enum class Tool { None, Distance, Angle };

    explicit ImageViewer(QWidget* parent = nullptr);

    void setSequence(std::shared_ptr<DsaSequence> dsa);
    void setRenderOptions(const RenderOptions& opt);

    void setFrame(int frame);
    int frame() const { return frame_; }

    void setTool(Tool tool);
    Tool tool() const { return tool_; }

    bool isActive() const { return dsa_ != nullptr; }

    // 当前显示图像（含测量叠加则另存当前帧原图）
    QImage currentImage() const;

    double zoomFactor() const;

    void clearMeasurements();

    // 是否参与多视图同步（同步帧号/窗宽窗位）
    bool synced() const { return synced_; }
    void setSynced(bool on) { synced_ = on; }

    // 实际生效的渲染选项（自动窗已解析）
    RenderOptions effectiveRenderOptions() const { return effectiveOptions_; }

signals:
    void frameChanged(int frame);
    void zoomChanged(double factor);
    void windowLevelChanged(double ww, double wc);
    void measurementsChanged();

public slots:
    void fitToWindow();
    void zoomIn();
    void zoomOut();
    void resetZoom();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent* event) override;
#else
    void enterEvent(QEvent* event) override;
#endif

private:
    void rebuildScene();
    void startMeasure(const QPointF& scenePos);
    void updateMeasure(const QPointF& scenePos);
    void finishMeasure();
    void applyDragWindowLevel(const QPoint& pos);
    void setCursorForTool();

    QGraphicsScene* scene_ = nullptr;
    QGraphicsPixmapItem* pixmapItem_ = nullptr;
    std::shared_ptr<DsaSequence> dsa_;
    RenderOptions options_;       // 外部设置的原始选项（ww<=0 触发自动窗）
    RenderOptions effectiveOptions_; // 自动窗解析后
    int frame_ = 0;

    // 测量
    Tool tool_ = Tool::None;
    DistanceItem* pendingDistance_ = nullptr;
    AngleItem* pendingAngle_ = nullptr;
    int angleClickCount_ = 0;   // 角度三点采集计数
    QPointF anglePts_[3];

    // 拖拽状态
    enum class DragMode { None, Pan, WindowLevel, MeasureHandle };
    DragMode drag_ = DragMode::None;
    QPoint lastPos_;
    double dragStartWw_ = 0.0, dragStartWc_ = 0.0;
    QGraphicsItem* dragHandleItem_ = nullptr;
    int dragHandleIndex_ = -1;

    // 窗宽窗位灵敏度（像素->窗值比例）
    static constexpr double kWlSensitivity = 2.0;

    bool synced_ = true;
};
