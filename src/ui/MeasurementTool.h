#pragma once

#include <QGraphicsItem>

// 测量图元基类：距离 / 角度（对应 Python 测量工具）
class MeasurementTool
{
public:
    enum class Type { Distance, Angle };

    static const double kHandleRadiusPx;

    // 两点间像素距离（px）
    static double pixelDistance(const QPointF& a, const QPointF& b);

    // 物理距离（mm）：像素距离 × spacing（mm/px）
    static double physicalDistance(const QPointF& a, const QPointF& b,
                                   double spacingXmm, double spacingYmm);

    // 三点角度（degrees）：angle at vertex
    static double angleDegrees(const QPointF& a, const QPointF& vertex, const QPointF& b);

    // 距离/角度格式化（mm 保留 2 位，角度 1 位）
    static QString formatDistance(double mm);
    static QString formatAngle(double deg);
};

// 距离测量图元（两点线段 + 标签）
class DistanceItem : public QGraphicsObject
{
    Q_OBJECT

public:
    DistanceItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    void setPoints(const QPointF& p1, const QPointF& p2);
    void setSpacing(double spacingXmm, double spacingYmm);

    // 拖拽手柄命中测试（scene 坐标）
    int hitHandle(const QPointF& scenePos) const; // -1 无, 0=p1, 1=p2
    void moveHandle(int handle, const QPointF& scenePos);

signals:
    void changed();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QPointF p1_, p2_;
    double spacingX_ = 1.0, spacingY_ = 1.0; // mm/px
};

// 角度测量图元（三点：端点-顶点-端点）
class AngleItem : public QGraphicsObject
{
    Q_OBJECT

public:
    AngleItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    void setPoints(const QPointF& a, const QPointF& vertex, const QPointF& b);

    int hitHandle(const QPointF& scenePos) const; // -1 无, 0=a, 1=vertex, 2=b
    void moveHandle(int handle, const QPointF& scenePos);

signals:
    void changed();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QPointF a_, v_, b_;
};
