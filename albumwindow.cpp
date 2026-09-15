#include "albumwindow.h"

#include "imagetransitionwidget.h"

#include <QDir>
#include <QDebug>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QtMath>

static const int kImageIntervalMs = 5000;
static const int kSwipeThreshold = 80;
static const int kClickThreshold = 12;
static const int kVideoWidth = 340;
static const int kVideoHeight = 200;

// 主题色
static const QColor kAccent(0x2f, 0x9c, 0xf4);
static const QColor kTextPrimary(0xea, 0xea, 0xf0);
static const QColor kTextSecondary(0xc8, 0xc8, 0xd0);
static const QColor kIconIdle(0xd5, 0xd5, 0xdd);
static const QColor kIconDisabled(0x6a, 0x6a, 0x74);

namespace {

enum class Icon { Folder, List, Loop, Volume, Mute, Exit, Play, Pause, Prev, Next };

// 用 QPainter 绘制矢量图标（不依赖图片资源，抗锯齿）
QIcon makeIcon(Icon icon, const QColor &color, int size = 24)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal s = size;

    QPen pen(color, s * 0.09);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(color);

    switch (icon) {
    case Icon::Play: {
        QPolygonF tri;
        tri << QPointF(s * 0.33, s * 0.22)
            << QPointF(s * 0.80, s * 0.50)
            << QPointF(s * 0.33, s * 0.78);
        p.drawPolygon(tri);
        break;
    }
    case Icon::Pause:
        p.drawRoundedRect(QRectF(s * 0.30, s * 0.24, s * 0.14, s * 0.52), s * 0.05, s * 0.05);
        p.drawRoundedRect(QRectF(s * 0.56, s * 0.24, s * 0.14, s * 0.52), s * 0.05, s * 0.05);
        break;
    case Icon::Prev:
        p.drawRoundedRect(QRectF(s * 0.24, s * 0.26, s * 0.09, s * 0.48), s * 0.03, s * 0.03);
        {
            QPolygonF tri;
            tri << QPointF(s * 0.78, s * 0.26)
                << QPointF(s * 0.78, s * 0.74)
                << QPointF(s * 0.40, s * 0.50);
            p.drawPolygon(tri);
        }
        break;
    case Icon::Next:
        {
            QPolygonF tri;
            tri << QPointF(s * 0.22, s * 0.26)
                << QPointF(s * 0.60, s * 0.50)
                << QPointF(s * 0.22, s * 0.74);
            p.drawPolygon(tri);
        }
        p.drawRoundedRect(QRectF(s * 0.67, s * 0.26, s * 0.09, s * 0.48), s * 0.03, s * 0.03);
        break;
    case Icon::Folder: {
        QPainterPath path;
        path.moveTo(s * 0.14, s * 0.74);
        path.lineTo(s * 0.14, s * 0.30);
        path.lineTo(s * 0.38, s * 0.30);
        path.lineTo(s * 0.46, s * 0.40);
        path.lineTo(s * 0.86, s * 0.40);
        path.lineTo(s * 0.86, s * 0.74);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Icon::List:
        for (int i = 0; i < 3; ++i) {
            const qreal y = s * (0.30 + i * 0.20);
            p.drawEllipse(QPointF(s * 0.24, y), s * 0.05, s * 0.05);
            p.drawRoundedRect(QRectF(s * 0.38, y - s * 0.05, s * 0.40, s * 0.10),
                              s * 0.05, s * 0.05);
        }
        break;
    case Icon::Loop: {
        const QRectF r(s * 0.20, s * 0.20, s * 0.60, s * 0.60);
        p.setBrush(Qt::NoBrush);
        p.drawArc(r, 30 * 16, 300 * 16);
        const qreal pi = 3.14159265358979323846;
        const qreal a = 30.0 * pi / 180.0;
        const QPointF tip(r.center().x() + r.width() / 2 * qCos(a),
                          r.center().y() - r.height() / 2 * qSin(a));
        QPolygonF head;
        head << tip + QPointF(-s * 0.02, -s * 0.13)
             << tip + QPointF(s * 0.11, s * 0.01)
             << tip + QPointF(-s * 0.08, s * 0.09);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawPolygon(head);
        break;
    }
    case Icon::Volume: {
        QPolygonF body;
        body << QPointF(s * 0.16, s * 0.40) << QPointF(s * 0.32, s * 0.40)
             << QPointF(s * 0.50, s * 0.24) << QPointF(s * 0.50, s * 0.76)
             << QPointF(s * 0.32, s * 0.60) << QPointF(s * 0.16, s * 0.60);
        p.drawPolygon(body);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(s * 0.42, s * 0.34, s * 0.26, s * 0.32), -60 * 16, 120 * 16);
        p.drawArc(QRectF(s * 0.46, s * 0.24, s * 0.40, s * 0.52), -60 * 16, 120 * 16);
        break;
    }
    case Icon::Mute: {
        QPolygonF body;
        body << QPointF(s * 0.14, s * 0.40) << QPointF(s * 0.30, s * 0.40)
             << QPointF(s * 0.46, s * 0.24) << QPointF(s * 0.46, s * 0.76)
             << QPointF(s * 0.30, s * 0.60) << QPointF(s * 0.14, s * 0.60);
        p.drawPolygon(body);
        p.drawLine(QPointF(s * 0.62, s * 0.40), QPointF(s * 0.86, s * 0.64));
        p.drawLine(QPointF(s * 0.86, s * 0.40), QPointF(s * 0.62, s * 0.64));
        break;
    }
    case Icon::Exit: {
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(s * 0.22, s * 0.22, s * 0.56, s * 0.56), 60 * 16, 240 * 16);
        p.drawLine(QPointF(s * 0.50, s * 0.18), QPointF(s * 0.50, s * 0.46));
        break;
    }
    }

    p.end();
    return QIcon(pm);
}

QToolButton *makeToolButton(Icon icon, const QString &text, QWidget *parent)
{
    QToolButton *button = new QToolButton(parent);
    button->setText(text);
    button->setIcon(makeIcon(icon, kIconIdle, 22));
    button->setIconSize(QSize(22, 22));
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setMinimumSize(86, 60);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QToolButton *makeTransportButton(Icon icon, QWidget *parent)
{
    QToolButton *button = new QToolButton(parent);
    button->setIcon(makeIcon(icon, kTextPrimary, 30));
    button->setIconSize(QSize(30, 30));
    button->setFixedSize(60, 60);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

AlbumWindow::AlbumWindow(QWidget *parent) :
    QMainWindow(parent),
    m_autoPlay(true),
    m_updatingList(false),
    m_lastVolume(80),
    m_errorRetries(0),
    m_lastItemWasImage(false)
{
    buildUi();

    // 启动时恢复上次目录与音量；首次运行默认 /root/album_media、音量 80
    QSettings settings;
    const int volume = settings.value(QStringLiteral("volume"), 80).toInt();
    m_volumeSlider->setValue(qBound(0, volume, 100));

    QString dir = settings.value(QStringLiteral("lastDir")).toString();
    if (dir.isEmpty() || !QDir(dir).exists())
        dir = QStringLiteral("/root/album_media");
    if (QDir(dir).exists()) {
        const QString startDir = dir;
        QTimer::singleShot(0, this, [this, startDir]() { loadDirectory(startDir, true); });
    }
}

AlbumWindow::~AlbumWindow()
{
    if (m_player)
        m_player->stop();
}

void AlbumWindow::buildUi()
{
    setWindowTitle(QStringLiteral("电子相册"));

    QWidget *central = new QWidget(this);
    central->setStyleSheet(QStringLiteral("background:#101014;"));
    setCentralWidget(central);

    QVBoxLayout *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ===== 顶部状态栏：状态指示 + 文件名 + 音量 =====
    QWidget *statusBar = new QWidget(central);
    statusBar->setFixedHeight(52);
    statusBar->setStyleSheet(QStringLiteral(
        "background:#1a1a20; border-bottom:1px solid #2a2a32;"));
    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 6, 16, 6);
    statusLayout->setSpacing(12);

    QLabel *statusDot = new QLabel(statusBar);
    statusDot->setFixedSize(10, 10);
    statusDot->setStyleSheet(QStringLiteral(
        "background:#2f9cf4; border-radius:5px;"));
    statusLayout->addWidget(statusDot);

    m_statusLabel = new QLabel(QStringLiteral("请点击「选择目录」开始"), statusBar);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#eaeaf0; font-size:16px;"));
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusLayout->addWidget(m_statusLabel, 1);

    QLabel *volumeIcon = new QLabel(statusBar);
    volumeIcon->setPixmap(makeIcon(Icon::Volume, QColor(0x9a, 0x9a, 0xa5), 22)
                              .pixmap(22, 22));
    statusLayout->addWidget(volumeIcon);

    m_volumeSlider = new QSlider(Qt::Horizontal, statusBar);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(180);
    m_volumeSlider->setMinimumHeight(36);
    m_volumeSlider->setFocusPolicy(Qt::NoFocus);
    m_volumeSlider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height:6px; background:#2e2e38; border-radius:3px; }"
        "QSlider::sub-page:horizontal { background:#2f9cf4; border-radius:3px; }"
        "QSlider::add-page:horizontal { background:#2e2e38; border-radius:3px; }"
        "QSlider::handle:horizontal { width:20px; height:20px; margin:-7px 0;"
        " background:#ffffff; border-radius:10px; }"));
    statusLayout->addWidget(m_volumeSlider);

    rootLayout->addWidget(statusBar);

    // ===== 中间显示区（图片页 + 视频页）+ 播放列表面板 =====
    QWidget *mainArea = new QWidget(central);
    QHBoxLayout *mainLayout = new QHBoxLayout(mainArea);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_stack = new QStackedWidget(mainArea);
    m_stack->setStyleSheet(QStringLiteral("background:#000000;"));

    m_imageWidget = new ImageTransitionWidget(m_stack);
    m_stack->addWidget(m_imageWidget);

    // 视频页：固定为视频原始尺寸并居中。QVideoWidget 的 QPainterVideoSurface
    // 在上一帧未画完时 present() 会返回 false（Qt 的 GStreamer sink 视为致命
    // 错误），单核板子上全屏缩放绘制太慢，会随机触发 Internal data stream error。
    // 1:1 尺寸绘制开销最小，可避免该问题。
    m_videoPage = new QWidget(m_stack);
    m_videoPage->setStyleSheet(QStringLiteral("background:#000000;"));
    QVBoxLayout *videoOuter = new QVBoxLayout(m_videoPage);
    videoOuter->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *videoInner = new QHBoxLayout;
    videoInner->setContentsMargins(0, 0, 0, 0);
    m_videoWidget = new QVideoWidget(m_videoPage);
    m_videoWidget->setFixedSize(kVideoWidth, kVideoHeight);
    videoInner->addStretch();
    videoInner->addWidget(m_videoWidget);
    videoInner->addStretch();
    videoOuter->addStretch();
    videoOuter->addLayout(videoInner);
    videoOuter->addStretch();
    m_stack->addWidget(m_videoPage);

    mainLayout->addWidget(m_stack, 1);

    // 播放列表面板
    m_listPanel = new QWidget(mainArea);
    m_listPanel->setFixedWidth(280);
    m_listPanel->setStyleSheet(QStringLiteral(
        "background:#16161c; border-left:1px solid #2a2a32;"));
    QVBoxLayout *listLayout = new QVBoxLayout(m_listPanel);
    listLayout->setContentsMargins(12, 12, 12, 12);
    listLayout->setSpacing(10);

    QLabel *listTitle = new QLabel(QStringLiteral("播放列表"), m_listPanel);
    listTitle->setStyleSheet(QStringLiteral("color:#9a9aa5; font-size:14px; font-weight:bold;"));
    listLayout->addWidget(listTitle);

    m_listWidget = new QListWidget(m_listPanel);
    m_listWidget->setStyleSheet(QStringLiteral(
        "QListWidget { background:transparent; border:none; color:#c8c8d0;"
        " font-size:14px; outline:0; }"
        "QListWidget::item { height:42px; padding-left:10px; border-radius:8px; margin:2px 0; }"
        "QListWidget::item:hover { background:rgba(255,255,255,0.06); }"
        "QListWidget::item:selected { background:#2f9cf4; color:#ffffff; }"));
    listLayout->addWidget(m_listWidget, 1);

    m_listPanel->setVisible(false);
    mainLayout->addWidget(m_listPanel);

    rootLayout->addWidget(mainArea, 1);

    // ===== 底部控制栏 =====
    QWidget *bar = new QWidget(central);
    bar->setFixedHeight(78);
    bar->setStyleSheet(QStringLiteral(
        "background:#1a1a20; border-top:1px solid #2a2a32;"
        "QToolButton { background:transparent; border:none; color:#c8c8d0;"
        " font-size:13px; padding:4px 8px; border-radius:10px; }"
        "QToolButton:hover { background:rgba(255,255,255,0.08); }"
        "QToolButton:pressed { background:rgba(255,255,255,0.16); }"
        "QToolButton:checked { background:rgba(47,156,244,0.22); color:#2f9cf4; }"
        "QToolButton:disabled { color:#5a5a64; }"));
    QHBoxLayout *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(16, 8, 16, 8);
    barLayout->setSpacing(10);

    QToolButton *dirButton = makeToolButton(Icon::Folder, QStringLiteral("选择目录"), bar);
    m_listButton = makeToolButton(Icon::List, QStringLiteral("列表"), bar);
    m_listButton->setCheckable(true);

    // 左右两组等宽，保证中间的播放控制组精确居中
    QWidget *leftGroup = new QWidget(bar);
    QHBoxLayout *leftLayout = new QHBoxLayout(leftGroup);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);
    leftLayout->addWidget(dirButton);
    leftLayout->addWidget(m_listButton);
    leftLayout->addStretch();
    leftGroup->setFixedWidth(300);

    QToolButton *prevButton = makeTransportButton(Icon::Prev, bar);
    m_playPauseButton = makeTransportButton(Icon::Play, bar);
    m_playPauseButton->setStyleSheet(QStringLiteral(
        "QToolButton { background:#2f9cf4; border:none; border-radius:30px; }"
        "QToolButton:hover { background:#4aabff; }"
        "QToolButton:pressed { background:#1f86dc; }"
        "QToolButton:disabled { background:#2a2a32; }"));
    QToolButton *nextButton = makeTransportButton(Icon::Next, bar);

    m_autoButton = makeToolButton(Icon::Loop, QStringLiteral("轮播开"), bar);
    m_autoButton->setCheckable(true);
    m_autoButton->setChecked(true);
    m_muteButton = makeToolButton(Icon::Volume, QStringLiteral("静音"), bar);
    m_muteButton->setCheckable(true);
    QToolButton *exitButton = makeToolButton(Icon::Exit, QStringLiteral("退出"), bar);

    QWidget *rightGroup = new QWidget(bar);
    QHBoxLayout *rightLayout = new QHBoxLayout(rightGroup);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);
    rightLayout->addStretch();
    rightLayout->addWidget(m_autoButton);
    rightLayout->addWidget(m_muteButton);
    rightLayout->addWidget(exitButton);
    rightGroup->setFixedWidth(300);

    barLayout->addWidget(leftGroup);
    barLayout->addStretch(1);
    barLayout->addWidget(prevButton);
    barLayout->addWidget(m_playPauseButton);
    barLayout->addWidget(nextButton);
    barLayout->addStretch(1);
    barLayout->addWidget(rightGroup);

    rootLayout->addWidget(bar);

    // ===== 播放器 =====
    m_player = new QMediaPlayer(this);
    m_player->setVideoOutput(m_videoWidget);

    m_imageTimer = new QTimer(this);
    m_imageTimer->setSingleShot(true);
    m_imageTimer->setInterval(kImageIntervalMs);

    // ===== 信号连接 =====
    connect(dirButton, &QToolButton::clicked, this, &AlbumWindow::chooseDirectory);
    connect(prevButton, &QToolButton::clicked, this, &AlbumWindow::playPrev);
    connect(nextButton, &QToolButton::clicked, this, &AlbumWindow::playNext);
    connect(m_playPauseButton, &QToolButton::clicked, this, &AlbumWindow::togglePlayPause);
    connect(m_autoButton, &QToolButton::toggled, this, &AlbumWindow::toggleAutoPlay);
    connect(m_muteButton, &QToolButton::toggled, this, &AlbumWindow::toggleMute);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &AlbumWindow::onVolumeChanged);
    connect(m_listButton, &QToolButton::toggled, this, &AlbumWindow::togglePlaylist);
    connect(exitButton, &QToolButton::clicked, this, &QWidget::close);
    connect(m_imageTimer, &QTimer::timeout, this, &AlbumWindow::onImageTimeout);
    connect(m_listWidget, &QListWidget::currentRowChanged, this, &AlbumWindow::onListRowChanged);
    connect(m_player, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),
            this, SLOT(onMediaStatusChanged()));
    connect(m_player, SIGNAL(stateChanged(QMediaPlayer::State)),
            this, SLOT(onPlayerStateChanged()));
    connect(m_player, SIGNAL(error(QMediaPlayer::Error)), this, SLOT(onPlayerError()));

    // 触摸/鼠标滑动与点击切换（触摸会被合成为鼠标事件）
    m_stack->installEventFilter(this);
    m_imageWidget->installEventFilter(this);
    m_videoPage->installEventFilter(this);
    m_videoWidget->installEventFilter(this);

    // 初始按钮状态
    updateAutoButton();
    updateMuteButton();
    updatePlayPauseButton();
}

void AlbumWindow::chooseDirectory()
{
    QString startDir = m_playlist.directory();
    if (startDir.isEmpty())
        startDir = QDir::homePath();

    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择播放列表目录"), startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
    if (dir.isEmpty())
        return;

    loadDirectory(dir, true);
}

void AlbumWindow::loadDirectory(const QString &dir, bool autoStart)
{
    if (!m_playlist.setDirectory(dir)) {
        m_statusLabel->setText(QStringLiteral("该目录下没有支持的多媒体文件（mp4/png/jpg/jpeg/bmp）"));
        return;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("lastDir"), m_playlist.directory());

    refreshList();
    m_playlist.setCurrentIndex(0);
    if (autoStart)
        playCurrent();
    else
        updateStatus();
}

void AlbumWindow::playCurrent()
{
    const QString path = m_playlist.currentFile();
    if (path.isEmpty())
        return;

    // 切换到新的媒体项时重置错误重试计数（重试同一项时不清零）
    if (path != m_lastPlayedPath) {
        m_errorRetries = 0;
        m_lastPlayedPath = path;
    }

    m_imageTimer->stop();
    m_player->stop();

    qDebug() << "play" << path;

    const bool previousWasImage = m_lastItemWasImage;
    const bool isVideo = PlaylistModel::isVideoFile(path);

    if (isVideo) {
        displayVideo(path);
    } else {
        // 只在“图片 → 图片”时播放过渡动画
        displayImage(path, previousWasImage);
        if (m_autoPlay)
            restartImageTimer();
    }
    m_lastItemWasImage = !isVideo;

    updatePlayPauseButton();
    updateStatus();
    syncListSelection();
}

void AlbumWindow::displayImage(const QString &path, bool animate)
{
    QImageReader reader(path);
    // 按文件内容识别格式，忽略扩展名：有些图片扩展名与实际格式不符
    // （例如 bg2.png 实际是 JPEG），默认按扩展名选插件会导致解码失败
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        qWarning() << "cannot read image:" << path << reader.errorString();
        m_statusLabel->setText(QStringLiteral("无法读取图片: %1").arg(QFileInfo(path).fileName()));
        if (m_autoPlay)
            QTimer::singleShot(2000, this, &AlbumWindow::playNext);
        return;
    }

    const QSize target = m_stack->size();
    const QSize orig = reader.size();
    if (target.isValid() && orig.isValid()) {
        const QSize scaled = orig.scaled(target, Qt::KeepAspectRatio);
        if (scaled != orig)
            reader.setScaledSize(scaled);
    }

    const QImage image = reader.read();
    if (image.isNull()) {
        qWarning() << "image decode failed:" << path << reader.errorString();
        m_statusLabel->setText(QStringLiteral("图片解码失败: %1").arg(QFileInfo(path).fileName()));
        if (m_autoPlay)
            QTimer::singleShot(2000, this, &AlbumWindow::playNext);
        return;
    }

    m_imageWidget->setImage(QPixmap::fromImage(image), animate);
    m_stack->setCurrentWidget(m_imageWidget);
}

void AlbumWindow::displayVideo(const QString &path)
{
    m_stack->setCurrentWidget(m_videoPage);
    m_player->setMedia(QUrl::fromLocalFile(path));
    m_player->setVolume(m_volumeSlider->value());
    m_player->play();
}

void AlbumWindow::playNext()
{
    if (m_playlist.next())
        playCurrent();
}

void AlbumWindow::playPrev()
{
    if (m_playlist.prev())
        playCurrent();
}

void AlbumWindow::togglePlayPause()
{
    const QString path = m_playlist.currentFile();
    if (path.isEmpty() || !PlaylistModel::isVideoFile(path))
        return;

    if (m_player->state() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void AlbumWindow::toggleAutoPlay(bool on)
{
    m_autoPlay = on;
    updateAutoButton();

    const QString path = m_playlist.currentFile();
    if (!on) {
        m_imageTimer->stop();
    } else if (!path.isEmpty()) {
        if (PlaylistModel::isVideoFile(path)) {
            if (m_player->state() != QMediaPlayer::PlayingState)
                m_player->play();
        } else {
            restartImageTimer();
        }
    }
    updateStatus();
}

void AlbumWindow::toggleMute(bool on)
{
    if (on) {
        // 静音：记住当前音量并把滑块拉到 0（由 onVolumeChanged 同步按钮状态）
        if (m_volumeSlider->value() > 0)
            m_lastVolume = m_volumeSlider->value();
        m_volumeSlider->setValue(0);
    } else {
        m_volumeSlider->setValue(m_lastVolume > 0 ? m_lastVolume : 80);
    }
}

// 音量滑块（0~100）：直接控制播放器音量，并与静音按钮状态联动
void AlbumWindow::onVolumeChanged(int value)
{
    m_player->setVolume(value);

    if (value > 0)
        m_lastVolume = value;

    const bool muted = (value == 0);
    if (m_muteButton->isChecked() != muted) {
        const bool blocked = m_muteButton->blockSignals(true);
        m_muteButton->setChecked(muted);
        m_muteButton->blockSignals(blocked);
    }
    updateMuteButton();

    QSettings settings;
    settings.setValue(QStringLiteral("volume"), value);
}

void AlbumWindow::togglePlaylist(bool on)
{
    m_listPanel->setVisible(on);
    if (on)
        refreshList();

    const QString path = m_playlist.currentFile();
    if (!path.isEmpty() && PlaylistModel::isImageFile(path))
        displayImage(path);
}

void AlbumWindow::onMediaStatusChanged()
{
    if (m_player->mediaStatus() == QMediaPlayer::EndOfMedia) {
        if (m_autoPlay)
            playNext();
        else
            updateStatus();
    }
}

void AlbumWindow::onPlayerStateChanged()
{
    updatePlayPauseButton();
}

void AlbumWindow::onPlayerError()
{
    if (m_player->error() == QMediaPlayer::NoError)
        return;

    qWarning() << "player error:" << m_player->errorString();

    const QString fileName = QFileInfo(m_playlist.currentFile()).fileName();

    // 切换视频时偶发的流错误：先重试当前项，避免直接跳过内容
    if (m_errorRetries == 0) {
        m_errorRetries++;
        m_statusLabel->setText(QStringLiteral("播放重试: %1").arg(fileName));
        QTimer::singleShot(500, this, &AlbumWindow::playCurrent);
        return;
    }

    m_statusLabel->setText(QStringLiteral("播放错误: %1 (%2)")
                               .arg(fileName, m_player->errorString()));
    if (m_autoPlay)
        QTimer::singleShot(2000, this, &AlbumWindow::playNext);
}

void AlbumWindow::onListRowChanged(int row)
{
    if (m_updatingList || row < 0)
        return;

    m_playlist.setCurrentIndex(row);
    playCurrent();
}

void AlbumWindow::onImageTimeout()
{
    if (m_autoPlay)
        playNext();
}

void AlbumWindow::restartImageTimer()
{
    m_imageTimer->start(kImageIntervalMs);
}

void AlbumWindow::updateStatus()
{
    const QString path = m_playlist.currentFile();
    if (path.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("请点击「选择目录」开始"));
        return;
    }

    const QString suffix = QStringLiteral("  ·  %1 / %2  ·  %3")
                               .arg(m_playlist.currentIndex() + 1)
                               .arg(m_playlist.count())
                               .arg(m_autoPlay ? QStringLiteral("自动轮播")
                                               : QStringLiteral("手动"));

    const QString fileName = QFileInfo(path).fileName();
    const QFontMetrics fm(m_statusLabel->font());
    const int available = m_statusLabel->width() - fm.horizontalAdvance(suffix) - 8;
    const QString displayName = (available > 60)
            ? fm.elidedText(fileName, Qt::ElideMiddle, available)
            : fileName;

    m_statusLabel->setText(displayName + suffix);
}

void AlbumWindow::refreshList()
{
    m_updatingList = true;
    m_listWidget->clear();
    for (int i = 0; i < m_playlist.count(); ++i)
        m_listWidget->addItem(QFileInfo(m_playlist.fileAt(i)).fileName());
    m_listWidget->setCurrentRow(m_playlist.currentIndex());
    m_updatingList = false;
}

void AlbumWindow::syncListSelection()
{
    if (!m_listWidget->isVisible())
        return;

    m_updatingList = true;
    m_listWidget->setCurrentRow(m_playlist.currentIndex());
    m_updatingList = false;
}

void AlbumWindow::updatePlayPauseButton()
{
    const QString path = m_playlist.currentFile();
    const bool isVideo = PlaylistModel::isVideoFile(path);
    m_playPauseButton->setEnabled(isVideo);

    const bool playing = (m_player->state() == QMediaPlayer::PlayingState);
    const QColor color = isVideo ? QColor(0xff, 0xff, 0xff) : kIconDisabled;
    m_playPauseButton->setIcon(makeIcon(playing ? Icon::Pause : Icon::Play, color, 30));
}

void AlbumWindow::updateMuteButton()
{
    const bool muted = (m_volumeSlider->value() == 0);
    m_muteButton->setText(muted ? QStringLiteral("取消静音")
                                : QStringLiteral("静音"));
    m_muteButton->setIcon(makeIcon(muted ? Icon::Mute : Icon::Volume,
                                   muted ? kAccent : kIconIdle, 22));
}

void AlbumWindow::updateAutoButton()
{
    m_autoButton->setText(m_autoPlay ? QStringLiteral("轮播开")
                                     : QStringLiteral("轮播关"));
    m_autoButton->setIcon(makeIcon(Icon::Loop,
                                   m_autoPlay ? kAccent : kIconIdle, 22));
}

bool AlbumWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton)
            m_pressPos = me->pos();
        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton)
            return false;

        const QPoint delta = me->pos() - m_pressPos;
        if (qAbs(delta.x()) > kSwipeThreshold && qAbs(delta.x()) > qAbs(delta.y())) {
            if (delta.x() < 0)
                playNext();
            else
                playPrev();
            return true;
        }
        if (qAbs(delta.x()) < kClickThreshold && qAbs(delta.y()) < kClickThreshold) {
            playNext();
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void AlbumWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    const QString path = m_playlist.currentFile();
    if (!path.isEmpty() && PlaylistModel::isImageFile(path))
        displayImage(path);
}

void AlbumWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        playPrev();
        break;
    case Qt::Key_Right:
        playNext();
        break;
    case Qt::Key_Space:
        togglePlayPause();
        break;
    case Qt::Key_Escape:
        close();
        break;
    default:
        QMainWindow::keyPressEvent(event);
        break;
    }
}
