#include "albumwindow.h"

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QList>
#include <QPalette>
#include <QString>

#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

// 扫描 /dev/input/event*，按设备能力（是否支持 ABS_MT_POSITION_X/Y）找到触摸屏。
// 本板 goodix 触摸屏被 udev 误标为 tablet（ID_INPUT_TABLET=1），linuxfb 默认
// 优先使用的 libinput 会依据 udev 标签将其忽略，因此需要显式指定设备。
static QString findTouchScreen()
{
    unsigned char absBits[(ABS_CNT + 7) / 8];

    for (int i = 0; i < 32; ++i) {
        const QString path = QStringLiteral("/dev/input/event%1").arg(i);
        const QByteArray nativePath = path.toLocal8Bit();
        const int fd = ::open(nativePath.constData(), O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;

        std::memset(absBits, 0, sizeof(absBits));
        const bool hasMtAxes =
            ::ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) >= 0
            && (absBits[ABS_MT_POSITION_X / 8] & (1 << (ABS_MT_POSITION_X % 8)))
            && (absBits[ABS_MT_POSITION_Y / 8] & (1 << (ABS_MT_POSITION_Y % 8)));
        ::close(fd);

        if (hasMtAxes)
            return path;
    }
    return QString();
}

static void setupInputEnvironment()
{
    // 禁用 libinput，走 evdev 输入通道
    if (qEnvironmentVariableIsEmpty("QT_QPA_FB_NO_LIBINPUT"))
        qputenv("QT_QPA_FB_NO_LIBINPUT", "1");

    const QString touchDevice = findTouchScreen();
    if (touchDevice.isEmpty())
        return;

    // 修正触摸屏上报范围：本板 DTS 声明 touchscreen-size 为 800x480，
    // 但 gt9xx 驱动实际输出 1024x600 坐标且不做缩放。Qt 按 800x480 归一化
    // 会把坐标放大，屏幕底部按钮点不到。这里通过 EVIOCSABS 把范围改成
    // 实际屏幕分辨率（读 /sys/class/graphics/fb0/virtual_size）。
    int screenW = 0;
    int screenH = 0;
    QFile sizeFile(QStringLiteral("/sys/class/graphics/fb0/virtual_size"));
    if (sizeFile.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> parts = sizeFile.readAll().trimmed().split(',');
        if (parts.size() == 2) {
            screenW = parts.at(0).toInt();
            screenH = parts.at(1).toInt();
        }
    }

    const int fd = ::open(touchDevice.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd >= 0) {
        struct input_absinfo info;
        if (screenW > 1 && ::ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &info) == 0
            && info.maximum != screenW - 1) {
            info.maximum = screenW - 1;
            ::ioctl(fd, EVIOCSABS(ABS_MT_POSITION_X), &info);
        }
        if (screenH > 1 && ::ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &info) == 0
            && info.maximum != screenH - 1) {
            info.maximum = screenH - 1;
            ::ioctl(fd, EVIOCSABS(ABS_MT_POSITION_Y), &info);
        }
        ::close(fd);
    }

    // 显式指定触摸屏设备（自动发现依赖 udev 标签，本板会漏掉）
    if (qEnvironmentVariableIsEmpty("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS"))
        qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", touchDevice.toLocal8Bit());
}

// 系统 PulseAudio 的 socket 路径带随机后缀（/tmp/pulse-XXXX/native），
// 自动探测并设置 PULSE_SERVER，否则 GStreamer 音频输出无法连接
static void setupAudioEnvironment()
{
    if (!qEnvironmentVariableIsEmpty("PULSE_SERVER"))
        return;

    const QStringList candidates = QDir(QStringLiteral("/tmp"))
        .entryList(QStringList() << QStringLiteral("pulse-*"),
                   QDir::Dirs | QDir::NoDotAndDotDot);
    for (int i = 0; i < candidates.size(); ++i) {
        const QString socket = QStringLiteral("/tmp/%1/native").arg(candidates.at(i));
        if (QFile::exists(socket)) {
            qputenv("PULSE_SERVER", QStringLiteral("unix:%1").arg(socket).toLocal8Bit());
            return;
        }
    }

    const char *const fallbacks[] = { "/run/pulse/native", "/var/run/pulse/native", 0 };
    for (int i = 0; fallbacks[i]; ++i) {
        if (QFile::exists(QLatin1String(fallbacks[i]))) {
            qputenv("PULSE_SERVER", QByteArray("unix:") + fallbacks[i]);
            return;
        }
    }
}

int main(int argc, char *argv[])
{
    setupInputEnvironment();
    setupAudioEnvironment();

    QApplication a(argc, argv);
    a.setOrganizationName(QStringLiteral("100ask"));
    a.setApplicationName(QStringLiteral("PhotoAlbum"));

    // 统一使用 Fusion 风格 + 深色调色板（文件选择对话框等系统控件风格一致）
    QApplication::setStyle(QStringLiteral("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x10, 0x10, 0x14));
    palette.setColor(QPalette::WindowText, QColor(0xea, 0xea, 0xf0));
    palette.setColor(QPalette::Base, QColor(0x1a, 0x1a, 0x20));
    palette.setColor(QPalette::AlternateBase, QColor(0x22, 0x22, 0x2a));
    palette.setColor(QPalette::Text, QColor(0xea, 0xea, 0xf0));
    palette.setColor(QPalette::Button, QColor(0x1a, 0x1a, 0x20));
    palette.setColor(QPalette::ButtonText, QColor(0xea, 0xea, 0xf0));
    palette.setColor(QPalette::Highlight, QColor(0x2f, 0x9c, 0xf4));
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::ToolTipBase, QColor(0x1a, 0x1a, 0x20));
    palette.setColor(QPalette::ToolTipText, QColor(0xea, 0xea, 0xf0));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6a, 0x6a, 0x74));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6a, 0x6a, 0x74));
    a.setPalette(palette);

    // 开发板中文字体（/usr/lib/fonts/msyh.ttc），存在则作为全局字体
    const QString fontPath = QStringLiteral("/usr/lib/fonts/msyh.ttc");
    if (QFileInfo::exists(fontPath)) {
        const int id = QFontDatabase::addApplicationFont(fontPath);
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty())
            a.setFont(QFont(families.first(), 12));
    }

    AlbumWindow w;
    w.showFullScreen();
    return a.exec();
}
