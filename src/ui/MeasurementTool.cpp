#include "MeasurementTool.h"
#include "Theme.h"

#include <QPainter>
#include <QStaticText>

#include <cmath>

const double MeasurementTool::kHandleRadiusPx = 6.0;

double MeasurementTool::pixelDistance(const QPointF& a, const QPointF& b)
{
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    return std::sqrt(dx * dx + dy * dy);
}

double MeasurementTool::physicalDistance(const QPointF& a, const QPointF& b,
                                         double spacingXmm, double spacingYmm)
{
    if (spacingXmm <= 0.0)
        spacingXmm = 1.0;
    if (spacingYmm <= 0.0)
        spacingYmm = 1.0;
    const double dx = (b.x() - a.x()) * spacingXmm;
    const double dy = (b.y() - a.y()) * spacingYmm;
    return std::sqrt(dx * dx + dy * dy);
}

double MeasurementTool::angleDegrees(const QPointF& a, const QPointF& vertex, const QPointF& b)
{
    const QPointF v1 = a - vertex;
    const QPointF v2 = b - vertex;
    const double dot = v1.x() * v2.x() + v1.y() * v2.y();
    const double n1 = std::sqrt(v1.x() * v1.x() + v1.y() * v1.y());
    const double n2 = std::sqrt(v2.x() * v2.x() + v2.y() * v2.y());
    if (n1 <= 0.0 || n2 <= 0.0)
        return 0.0;
    double c = dot / (n1 * n2);
    c = std::max(-1.0, std::min(1.0, c));
    return std::acos(c) * 180.0 / M_PI;
}

QString MeasurementTool::formatDistance(double mm)
{
    return QString::number(mm, 'f', 2) + QStringLiteral(" mm");
}

QString MeasurementTool::formatAngle(double deg)
{
    return QString::number(deg, 'f', 1) + QStringLiteral("°");
}

// ---------------------------------------------------------------------------
// DistanceItem
// ---------------------------------------------------------------------------
DistanceItem::DistanceItem(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    setFlags(ItemIsSelectable | ItemIsMovable);
}

void DistanceItem::setPoints(const QPointF& p1, const QPointF& p2)
{
    prepareGeometryChange();
    p1_ = p1;
    p2_ = p2;
    update();
}

void DistanceItem::setSpacing(double spacingXmm, double spacingYmm)
{
    spacingX_ = spacingXmm;
    spacingY_ = spacingYmm;
    update();
}

QRectF DistanceItem::boundingRect() const
{
    const double m = MeasurementTool::kHandleRadiusPx + 20.0;
    return QRectF(p1_, p2_).normalized().adjusted(-m, -m, m, m);
}

void DistanceItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    const QColor line = QColor(Theme::measureColor());

    // 线段
    painter->setPen(QPen(line, 2.0));
    painter->drawLine(p1_, p2_);

    // 手柄
    painter->setBrush(line);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(p1_, MeasurementTool::kHandleRadiusPx / 2, MeasurementTool::kHandleRadiusPx / 2);
    painter->drawEllipse(p2_, MeasurementTool::kHandleRadiusPx / 2, MeasurementTool::kHandleRadiusPx / 2);

    // 标签（距离）
    const double mm = MeasurementTool::physicalDistance(p1_, p2_, spacingX_, spacingY_);
    const QString label = MeasurementTool::formatDistance(mm);
    const QPointF mid = (p1_ + p2_) / 2.0 + QPointF(8, -8);

    painter->setPen(Qt::NoPen);
    QFont f = painter->font();
    f.setBold(true);
    painter->setFont(f);
    const QFontMetrics fm(f);
    const QRect textRect = fm.boundingRect(label).adjusted(-4, -2, 4, 2)
                               .translated(mid.toPoint());
    painter->setBrush(QColor(0, 0, 0, 170));
    painter->drawRoundedRect(textRect, 3, 3);
    painter->setPen(QPen(line));
    painter->drawText(textRect, Qt::AlignCenter, label);
}

int DistanceItem::hitHandle(const QPointF& scenePos) const
{
    const QPointF p = mapFromScene(scenePos);
    if (QLineF(p, p1_).length() <= MeasurementTool::kHandleRadiusPx)
        return 0;
    if (QLineF(p, p2_).length() <= MeasurementTool::kHandleRadiusPx)
        return 1;
    return -1;
}

void DistanceItem::moveHandle(int handle, const QPointF& scenePos)
{
    if (handle == 0)
        setPoints(mapFromScene(scenePos), p2_);
    else if (handle == 1)
        setPoints(p1_, mapFromScene(scenePos));
}

QVariant DistanceItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged)
        emit changed();
    return QGraphicsObject::itemChange(change, value);
}

// ---------------------------------------------------------------------------
// AngleItem
// ---------------------------------------------------------------------------
AngleItem::AngleItem(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    setFlags(ItemIsSelectable | ItemIsMovable);
}

void AngleItem::setPoints(const QPointF& a, const QPointF& vertex, const QPointF& b)
{
    prepareGeometryChange();
    a_ = a;
    v_ = vertex;
    b_ = b;
    update();
}

QRectF AngleItem::boundingRect() const
{
    const double m = MeasurementTool::kHandleRadiusPx + 20.0;
    return QRectF(QPointF(std::min({a_.x(), v_.x(), b_.x()}),
                          std::min({a_.y(), v_.y(), b_.y()})),
                  QPointF(std::max({a_.x(), v_.x(), b_.x()}),
                          std::max({a_.y(), v_.y(), b_.y()})))
        .adjusted(-m, -m, m, m);
}

void AngleItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    const QColor line = QColor(Theme::measureColor());

    painter->setPen(QPen(line, 2.0));
    painter->drawLine(v_, a_);
    painter->drawLine(v_, b_);

    // 顶点角度弧
    const double deg = MeasurementTool::angleDegrees(a_, v_, b_);
    const double r = 14.0;
    const double a1 = std::atan2((a_ - v_).y(), (a_ - v_).x()) * 180.0 / M_PI;
    const double a2 = std::atan2((b_ - v_).y(), (b_ - v_).x()) * 180.0 / M_PI;
    QRectF arcRect(v_.x() - r, v_.y() - r, 2 * r, 2 * r);
    int startAngle = static_cast<int>(a1 * 16);
    int spanAngle = static_cast<int>((a2 - a1) * 16);
    while (spanAngle > 360 * 16)
        spanAngle -= 360 * 16;
    while (spanAngle < -360 * 16)
        spanAngle += 360 * 16;
    painter->drawArc(arcRect, startAngle, spanAngle);

    // 手柄
    painter->setBrush(line);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(a_, MeasurementTool::kHandleRadiusPx / 2, MeasurementTool::kHandleRadiusPx / 2);
    painter->drawEllipse(v_, MeasurementTool::kHandleRadiusPx / 2, MeasurementTool::kHandleRadiusPx / 2);
    painter->drawEllipse(b_, MeasurementTool::kHandleRadiusPx / 2, MeasurementTool::kHandleRadiusPx / 2);

    // 标签（角度）
    const QString label = MeasurementTool::formatAngle(deg);
    const QPointF labelPos = v_ + QPointF(10, -10);
    QFont f = painter->font();
    f.setBold(true);
    painter->setFont(f);
    const QFontMetrics fm(f);
    const QRect textRect = fm.boundingRect(label).adjusted(-4, -2, 4, 2)
                               .translated(labelPos.toPoint());
    painter->setBrush(QColor(0, 0, 0, 170));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(textRect, 3, 3);
    painter->setPen(QPen(line));
    painter->drawText(textRect, Qt::AlignCenter, label);
}

int AngleItem::hitHandle(const QPointF& scenePos) const
{
    const QPointF p = mapFromScene(scenePos);
    if (QLineF(p, a_).length() <= MeasurementTool::kHandleRadiusPx)
        return 0;
    if (QLineF(p, v_).length() <= MeasurementTool::kHandleRadiusPx)
        return 1;
    if (QLineF(p, b_).length() <= MeasurementTool::kHandleRadiusPx)
        return 2;
    return -1;
}

void AngleItem::moveHandle(int handle, const QPointF& scenePos)
{
    const QPointF p = mapFromScene(scenePos);
    if (handle == 0)
        setPoints(p, v_, b_);
    else if (handle == 1)
        setPoints(a_, p, b_);
    else if (handle == 2)
        setPoints(a_, v_, p);
}

QVariant AngleItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged)
        emit changed();
    return QGraphicsObject::itemChange(change, value);
}
