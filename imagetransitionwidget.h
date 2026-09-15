#ifndef IMAGETRANSITIONWIDGET_H
#define IMAGETRANSITIONWIDGET_H

#include <QPixmap>
#include <QWidget>

class QPropertyAnimation;

// 图片显示控件：在两张图片之间做过渡动画（平移/渐变/缩放/翻转/圆形展开/旋转）
class ImageTransitionWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress WRITE setProgress)

public:
    enum Transition {
        Fade,
        Slide,
        Zoom,
        Flip,
        Circle,
        Rotate,
        TransitionCount
    };

    explicit ImageTransitionWidget(QWidget *parent = 0);

    // animate 为 true 且已有上一张图片时播放过渡动画
    void setImage(const QPixmap &pixmap, bool animate);

    qreal progress() const { return m_progress; }
    void setProgress(qreal progress);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawPixmap(QPainter &painter, const QPixmap &pixmap, qreal opacity,
                    const QTransform &transform) const;

    QPixmap m_current;
    QPixmap m_previous;
    qreal m_progress;
    Transition m_transition;
    QPropertyAnimation *m_animation;
};

#endif // IMAGETRANSITIONWIDGET_H
