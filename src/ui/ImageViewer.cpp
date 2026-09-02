#include "ImageViewer.h"
#include "MeasurementTool.h"
#include "Theme.h"

#include <QGraphicsPixmapItem>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <cmath>

ImageViewer::ImageViewer(QWidget* parent)
    : QGraphicsView(parent)
{
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    pixmapItem_ = scene_->addPixmap(QPixmap());
    pixmapItem_->setTransformationMode(Qt::SmoothTransformation);

    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::NoDrag);
    setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QBrush(QColor(Theme::bgColor())));
    setCursorForTool();
}

// ---------------------------------------------------------------------------
// 数据绑定
// ---------------------------------------------------------------------------
void ImageViewer::setSequence(std::shared_ptr<DsaSequence> dsa)
{
    dsa_ = std::move(dsa);
    frame_ = 0;
    clearMeasurements();
    rebuildScene();
    if (isActive())
        fitToWindow();
}

void ImageViewer::setRenderOptions(const RenderOptions& opt)
{
    options_ = opt;
    rebuildScene();
}

void ImageViewer::setFrame(int frame)
{
    if (!dsa_)
        return;
    frame = qBound(0, frame, dsa_->frameCount() - 1);
    if (frame == frame_)
        return;
    frame_ = frame;
    rebuildScene();
    emit frameChanged(frame_);
}

void ImageViewer::setTool(Tool tool)
{
    tool_ = tool;
    setCursorForTool();
}

void ImageViewer::clearMeasurements()
{
    pendingDistance_ = nullptr;
    pendingAngle_ = nullptr;
    angleClickCount_ = 0;
    const auto items = scene_->items();
    for (auto* it : items) {
        if (qgraphicsitem_cast<DistanceItem*>(it) || qgraphicsitem_cast<AngleItem*>(it))
            scene_->removeItem(it);
    }
    emit measurementsChanged();
}

// ---------------------------------------------------------------------------
// 渲染
// ---------------------------------------------------------------------------
void ImageViewer::rebuildScene()
{
    if (!dsa_) {
        pixmapItem_->setPixmap(QPixmap());
        scene_->setSceneRect(QRectF());
        return;
    }

    // 解析自动窗
    effectiveOptions_ = options_;
    if (effectiveOptions_.windowWidth <= 0.0) {
        const auto aw = dsa_->autoWindow(frame_);
        effectiveOptions_.windowWidth = aw.first;
        effectiveOptions_.windowCenter = aw.second;
    }

    const QImage img = dsa_->renderedImage(frame_, effectiveOptions_);
    if (img.isNull())
        return;

    pixmapItem_->setPixmap(QPixmap::fromImage(img));
    if (scene_->sceneRect().isEmpty())
        scene_->setSceneRect(pixmapItem_->boundingRect());
}

QImage ImageViewer::currentImage() const
{
    return dsa_ ? dsa_->renderedImage(frame_, effectiveOptions_) : QImage();
}

double ImageViewer::zoomFactor() const
{
    return transform().m11();
}

// ---------------------------------------------------------------------------
// 缩放
// ---------------------------------------------------------------------------
void ImageViewer::fitToWindow()
{
    if (!pixmapItem_ || pixmapItem_->pixmap().isNull())
        return;
    fitInView(pixmapItem_, Qt::KeepAspectRatio);
    emit zoomChanged(zoomFactor());
}

void ImageViewer::zoomIn()
{
    scale(1.25, 1.25);
    emit zoomChanged(zoomFactor());
}

void ImageViewer::zoomOut()
{
    scale(1.0 / 1.25, 1.0 / 1.25);
    emit zoomChanged(zoomFactor());
}

void ImageViewer::resetZoom()
{
    setTransform(QTransform());
    emit zoomChanged(1.0);
}

void ImageViewer::wheelEvent(QWheelEvent* event)
{
    const double f = event->angleDelta().y() > 0 ? 1.1 : (1.0 / 1.1);
    scale(f, f);
    emit zoomChanged(zoomFactor());
    event->accept();
}

// ---------------------------------------------------------------------------
// 鼠标交互：左键=工具/窗位，中/右键=平移
// ---------------------------------------------------------------------------
void ImageViewer::mousePressEvent(QMouseEvent* event)
{
    const QPointF scenePos = mapToScene(event->pos());

    // 已有测量项的手柄拖拽优先
    if (event->button() == Qt::LeftButton) {
        for (auto* it : scene_->items(scenePos)) {
            if (auto* d = qgraphicsitem_cast<DistanceItem*>(it)) {
                const int h = d->hitHandle(scenePos);
                if (h >= 0) {
                    drag_ = DragMode::MeasureHandle;
                    dragHandleItem_ = d;
                    dragHandleIndex_ = h;
                    event->accept();
                    return;
                }
            } else if (auto* a = qgraphicsitem_cast<AngleItem*>(it)) {
                const int h = a->hitHandle(scenePos);
                if (h >= 0) {
                    drag_ = DragMode::MeasureHandle;
                    dragHandleItem_ = a;
                    dragHandleIndex_ = h;
                    event->accept();
                    return;
                }
            }
        }

        // 新测量
        if (tool_ == Tool::Distance) {
            startMeasure(scenePos);
            event->accept();
            return;
        }
        if (tool_ == Tool::Angle) {
            startMeasure(scenePos);
            event->accept();
            return;
        }

        // 默认左键：窗宽窗位拖拽
        drag_ = DragMode::WindowLevel;
        lastPos_ = event->pos();
        dragStartWw_ = effectiveOptions_.windowWidth;
        dragStartWc_ = effectiveOptions_.windowCenter;
        event->accept();
        return;
    }

    // 中键/右键：平移
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        drag_ = DragMode::Pan;
        lastPos_ = event->pos();
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent* event)
{
    switch (drag_) {
    case DragMode::Pan: {
        const QPoint delta = event->pos() - lastPos_;
        lastPos_ = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    case DragMode::WindowLevel:
        applyDragWindowLevel(event->pos());
        event->accept();
        return;
    case DragMode::MeasureHandle:
        updateMeasure(mapToScene(event->pos()));
        event->accept();
        return;
    default:
        break;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent* event)
{
    if (drag_ == DragMode::MeasureHandle)
        emit measurementsChanged();
    drag_ = DragMode::None;
    dragHandleItem_ = nullptr;
    dragHandleIndex_ = -1;
    QGraphicsView::mouseReleaseEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ImageViewer::enterEvent(QEnterEvent* event)
#else
void ImageViewer::enterEvent(QEvent* event)
#endif
{
    setFocus();
    QGraphicsView::enterEvent(event);
}

void ImageViewer::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
}

// ---------------------------------------------------------------------------
// 窗宽窗位拖拽（对应 Python WL 拖拽：水平=窗宽，垂直=窗位）
// ---------------------------------------------------------------------------
void ImageViewer::applyDragWindowLevel(const QPoint& pos)
{
    const int dx = pos.x() - lastPos_.x();
    const int dy = pos.y() - lastPos_.y();
    lastPos_ = pos;

    double ww = dragStartWw_ + dx * kWlSensitivity;
    double wc = dragStartWc_ + dy * kWlSensitivity;
    ww = qMax(1.0, ww);

    options_.windowWidth = ww;
    options_.windowCenter = wc;
    effectiveOptions_.windowWidth = ww;
    effectiveOptions_.windowCenter = wc;
    rebuildScene();
    emit windowLevelChanged(ww, wc);
}

// ---------------------------------------------------------------------------
// 测量工具
// ---------------------------------------------------------------------------
void ImageViewer::startMeasure(const QPointF& scenePos)
{
    if (tool_ == Tool::Distance) {
        if (pendingDistance_ == nullptr) {
            pendingDistance_ = new DistanceItem();
            scene_->addItem(pendingDistance_);
            pendingDistance_->setPoints(scenePos, scenePos);
            drag_ = DragMode::MeasureHandle;
            dragHandleItem_ = pendingDistance_;
            dragHandleIndex_ = 1;
        }
        emit measurementsChanged();
    } else if (tool_ == Tool::Angle) {
        anglePts_[angleClickCount_++] = scenePos;
        if (angleClickCount_ == 1) {
            // 预览：顶点与端点暂时重合
            if (!pendingAngle_) {
                pendingAngle_ = new AngleItem();
                scene_->addItem(pendingAngle_);
            }
            pendingAngle_->setPoints(scenePos, scenePos, scenePos);
        } else if (angleClickCount_ == 2) {
            pendingAngle_->setPoints(anglePts_[0], anglePts_[0], scenePos);
            drag_ = DragMode::MeasureHandle;
            dragHandleItem_ = pendingAngle_;
            dragHandleIndex_ = 2;
        } else {
            // 三点采集完成
            pendingAngle_->setPoints(anglePts_[0], anglePts_[1], anglePts_[2]);
            pendingAngle_ = nullptr;
            angleClickCount_ = 0;
            emit measurementsChanged();
        }
    }
}

void ImageViewer::updateMeasure(const QPointF& scenePos)
{
    if (dragHandleItem_ == nullptr)
        return;
    if (auto* d = qgraphicsitem_cast<DistanceItem*>(dragHandleItem_)) {
        d->moveHandle(dragHandleIndex_, scenePos);
    } else if (auto* a = qgraphicsitem_cast<AngleItem*>(dragHandleItem_)) {
        a->moveHandle(dragHandleIndex_, scenePos);
    }
}

void ImageViewer::setCursorForTool()
{
    switch (tool_) {
    case Tool::Distance:
    case Tool::Angle:
        setCursor(Qt::CrossCursor);
        break;
    default:
        setCursor(Qt::ArrowCursor);
    }
}
