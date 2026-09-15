#include "albumwindow.h"

#include <QDir>
#include <QDebug>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoWidget>

static const int kImageIntervalMs = 5000;
static const int kSwipeThreshold = 80;
static const int kClickThreshold = 12;
static const int kVideoWidth = 340;
static const int kVideoHeight = 200;

AlbumWindow::AlbumWindow(QWidget *parent) :
    QMainWindow(parent),
    m_autoPlay(true),
    m_updatingList(false),
    m_lastVolume(80),
    m_errorRetries(0)
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
    setCentralWidget(central);

    QVBoxLayout *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 顶部状态栏：左侧状态文本 + 右侧音量滑块（0~100）
    QWidget *statusBar = new QWidget(central);
    statusBar->setMinimumHeight(44);
    statusBar->setStyleSheet(QStringLiteral("background:#202020;"));
    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(12, 4, 12, 4);
    statusLayout->setSpacing(10);

    m_statusLabel = new QLabel(QStringLiteral("请点击「选择目录」开始"), statusBar);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#ffffff; font-size:16px;"));
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusLayout->addWidget(m_statusLabel, 1);

    QLabel *volumeLabel = new QLabel(QStringLiteral("音量"), statusBar);
    volumeLabel->setStyleSheet(QStringLiteral("color:#ffffff; font-size:16px;"));
    statusLayout->addWidget(volumeLabel);

    m_volumeSlider = new QSlider(Qt::Horizontal, statusBar);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(200);
    m_volumeSlider->setMinimumHeight(36);
    m_volumeSlider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height:8px; background:#555555; border-radius:4px; }"
        "QSlider::sub-page:horizontal { background:#4da3ff; border-radius:4px; }"
        "QSlider::handle:horizontal { width:22px; margin:-8px 0; background:#e0e0e0;"
        " border-radius:11px; }"));
    statusLayout->addWidget(m_volumeSlider);

    rootLayout->addWidget(statusBar);

    // 中间显示区（图片页 + 视频页）+ 可开关的播放列表面板
    QWidget *mainArea = new QWidget(central);
    QHBoxLayout *mainLayout = new QHBoxLayout(mainArea);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_stack = new QStackedWidget(mainArea);
    m_stack->setStyleSheet(QStringLiteral("background:#000000;"));

    m_imageLabel = new QLabel(m_stack);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet(QStringLiteral("background:#000000;"));
    m_stack->addWidget(m_imageLabel);

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

    m_listWidget = new QListWidget(mainArea);
    m_listWidget->setFixedWidth(260);
    m_listWidget->setVisible(false);
    m_listWidget->setStyleSheet(QStringLiteral("font-size:14px;"));
    mainLayout->addWidget(m_listWidget);

    rootLayout->addWidget(mainArea, 1);

    // 底部控制栏
    QWidget *bar = new QWidget(central);
    bar->setStyleSheet(QStringLiteral("background:#202020;"));
    QHBoxLayout *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(8, 6, 8, 6);
    barLayout->setSpacing(8);

    QPushButton *dirButton = new QPushButton(QStringLiteral("选择目录"), bar);
    m_listButton = new QPushButton(QStringLiteral("列表"), bar);
    m_listButton->setCheckable(true);
    QPushButton *prevButton = new QPushButton(QStringLiteral("上一张"), bar);
    m_playPauseButton = new QPushButton(QStringLiteral("播放/暂停"), bar);
    QPushButton *nextButton = new QPushButton(QStringLiteral("下一张"), bar);
    m_autoButton = new QPushButton(QStringLiteral("自动轮播: 开"), bar);
    m_autoButton->setCheckable(true);
    m_autoButton->setChecked(true);
    m_muteButton = new QPushButton(QStringLiteral("静音"), bar);
    m_muteButton->setCheckable(true);
    QPushButton *exitButton = new QPushButton(QStringLiteral("退出"), bar);

    QPushButton *buttons[] = { dirButton, m_listButton, prevButton, m_playPauseButton,
                               nextButton, m_autoButton, m_muteButton, exitButton };
    for (int i = 0; i < 8; ++i) {
        buttons[i]->setMinimumHeight(46);
        buttons[i]->setMinimumWidth(96);
        buttons[i]->setStyleSheet(QStringLiteral("font-size:15px;"));
        barLayout->addWidget(buttons[i]);
    }
    barLayout->addStretch();

    rootLayout->addWidget(bar);

    // 播放器
    m_player = new QMediaPlayer(this);
    m_player->setVideoOutput(m_videoWidget);

    m_imageTimer = new QTimer(this);
    m_imageTimer->setSingleShot(true);
    m_imageTimer->setInterval(kImageIntervalMs);

    // 信号连接
    connect(dirButton, &QPushButton::clicked, this, &AlbumWindow::chooseDirectory);
    connect(prevButton, &QPushButton::clicked, this, &AlbumWindow::playPrev);
    connect(nextButton, &QPushButton::clicked, this, &AlbumWindow::playNext);
    connect(m_playPauseButton, &QPushButton::clicked, this, &AlbumWindow::togglePlayPause);
    connect(m_autoButton, &QPushButton::toggled, this, &AlbumWindow::toggleAutoPlay);
    connect(m_muteButton, &QPushButton::toggled, this, &AlbumWindow::toggleMute);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &AlbumWindow::onVolumeChanged);
    connect(m_listButton, &QPushButton::toggled, this, &AlbumWindow::togglePlaylist);
    connect(exitButton, &QPushButton::clicked, this, &QWidget::close);
    connect(m_imageTimer, &QTimer::timeout, this, &AlbumWindow::onImageTimeout);
    connect(m_listWidget, &QListWidget::currentRowChanged, this, &AlbumWindow::onListRowChanged);
    connect(m_player, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),
            this, SLOT(onMediaStatusChanged()));
    connect(m_player, SIGNAL(error(QMediaPlayer::Error)), this, SLOT(onPlayerError()));

    // 触摸/鼠标滑动与点击切换（触摸会被合成为鼠标事件）
    m_stack->installEventFilter(this);
    m_imageLabel->installEventFilter(this);
    m_videoPage->installEventFilter(this);
    m_videoWidget->installEventFilter(this);
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

    if (PlaylistModel::isVideoFile(path)) {
        displayVideo(path);
        m_playPauseButton->setEnabled(true);
    } else {
        displayImage(path);
        m_playPauseButton->setEnabled(false);
        if (m_autoPlay)
            restartImageTimer();
    }

    updateStatus();
    syncListSelection();
}

void AlbumWindow::displayImage(const QString &path)
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

    m_imageLabel->setPixmap(QPixmap::fromImage(image));
    m_stack->setCurrentWidget(m_imageLabel);
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
    m_autoButton->setText(on ? QStringLiteral("自动轮播: 开")
                             : QStringLiteral("自动轮播: 关"));

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
    m_muteButton->setText(muted ? QStringLiteral("取消静音")
                                : QStringLiteral("静音"));

    QSettings settings;
    settings.setValue(QStringLiteral("volume"), value);
}

void AlbumWindow::togglePlaylist(bool on)
{
    m_listWidget->setVisible(on);
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

    m_statusLabel->setText(QStringLiteral("%1    [%2/%3]    %4")
                               .arg(QFileInfo(path).fileName())
                               .arg(m_playlist.currentIndex() + 1)
                               .arg(m_playlist.count())
                               .arg(m_autoPlay ? QStringLiteral("自动轮播")
                                               : QStringLiteral("手动")));
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
