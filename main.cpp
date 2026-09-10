#include "albumwindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName(QStringLiteral("100ask"));
    a.setApplicationName(QStringLiteral("PhotoAlbum"));

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
