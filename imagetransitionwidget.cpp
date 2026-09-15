#include "imagetransitionwidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QtMath>

static const qreal kPi = 3.14159265358979323846;

ImageTransitionWidget::ImageTransitionWidget(QWidget *parent) :
    QWidget(parent),
    m_progress(1.0),
    m_transition(Fade)
{
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_animation = new QPropertyAnimation(this, "progress", this);
    m_animation->setDuration(500);
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(QEasingCurve::InOutCubic);
}

void ImageTransitionWidget::setImage(const QPixmap &pixmap, bool animate)
{
    m_animation->stop();

    if (!animate || m_current.isNull() || pixmap.isNull()) {
        m_current = pixmap;
        m_previous = QPixmap();
        m_progress = 1.0;
        update();
        return;
    }

    m_previous = m_current;
    m_current = pixmap;
    m_transition = static_cast<Transition>(
        QRandomGenerator::global()->bounded(int(TransitionCount)));
    m_progress = 0.0;
    m_animation->start();
}

void ImageTransitionWidget::setProgress(qreal progress)
{
    m_progress = progress;
    update();
}

void ImageTransitionWidget::drawPixmap(QPainter &painter, const QPixmap &pixmap,
                                       qreal opacity, const QTransform &transform) const
{
    if (pixmap.isNull())
        return;

    painter.save();
    painter.setOpacity(opacity);
    painter.setTransform(transform, false);
    painter.drawPixmap(-pixmap.width() / 2, -pixmap.height() / 2, pixmap);
    painter.restore();
}

void ImageTransitionWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (m_current.isNull())
        return;

    const QPointF center(width() / 2.0, height() / 2.0);
    const qreal p = qBound(0.0, m_progress, 1.0);

    switch (m_transition) {
    case Fade:
        drawPixmap(painter, m_previous, 1.0 - p, QTransform::fromTranslate(center.x(), center.y()));
        drawPixmap(painter, m_current, p, QTransform::fromTranslate(center.x(), center.y()));
        break;

    case Slide: {
        const qreal w = width();
        drawPixmap(painter, m_previous, 1.0,
                   QTransform::fromTranslate(center.x() - w * p, center.y()));
        drawPixmap(painter, m_current, 1.0,
                   QTransform::fromTranslate(center.x() + w * (1.0 - p), center.y()));
        break;
    }

    case Zoom: {
        QTransform out;
        out.translate(center.x(), center.y());
        out.scale(1.0 + 0.12 * p, 1.0 + 0.12 * p);

        QTransform in;
        in.translate(center.x(), center.y());
        in.scale(0.88 + 0.12 * p, 0.88 + 0.12 * p);

        drawPixmap(painter, m_previous, 1.0 - p, out);
        drawPixmap(painter, m_current, p, in);
        break;
    }

    case Flip: {
        // 前半段旧图水平翻转到 0，后半段新图从 0 翻转展开（卡片翻转效果）
        if (p < 0.5) {
            const qreal s = qCos(kPi * p);
            if (s > 0.02) {
                QTransform t;
                t.translate(center.x(), center.y());
                t.scale(s, 1.0);
                drawPixmap(painter, m_previous, 1.0, t);
            }
        } else {
            const qreal s = -qCos(kPi * p);
            if (s > 0.02) {
                QTransform t;
                t.translate(center.x(), center.y());
                t.scale(s, 1.0);
                drawPixmap(painter, m_current, 1.0, t);
            }
        }
        break;
    }

    case Circle: {
        drawPixmap(painter, m_previous, 1.0,
                   QTransform::fromTranslate(center.x(), center.y()));
        const qreal radius = qSqrt(qreal(width()) * width() + qreal(height()) * height())
                             / 2.0 * p;
        QPainterPath path;
        path.addEllipse(center, radius, radius);
        painter.save();
        painter.setClipPath(path);
        drawPixmap(painter, m_current, 1.0,
                   QTransform::fromTranslate(center.x(), center.y()));
        painter.restore();
        break;
    }

    case Rotate: {
        QTransform out;
        out.translate(center.x(), center.y());
        out.rotate(8.0 * p);

        QTransform in;
        in.translate(center.x(), center.y());
        in.rotate(-10.0 * (1.0 - p));

        drawPixmap(painter, m_previous, 1.0 - p, out);
        drawPixmap(painter, m_current, p, in);
        break;
    }

    default:
        break;
    }
}
