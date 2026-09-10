#include "playlistmodel.h"

#include <QDir>
#include <QFileInfo>

namespace {

const char *const kVideoSuffixes[] = { "mp4", 0 };
const char *const kImageSuffixes[] = { "png", "jpg", "jpeg", "bmp", 0 };

bool suffixIn(const QString &suffix, const char *const *list)
{
    for (int i = 0; list[i]; ++i) {
        if (suffix == QLatin1String(list[i]))
            return true;
    }
    return false;
}

} // namespace

PlaylistModel::PlaylistModel() :
    m_index(-1)
{
}

bool PlaylistModel::isVideoFile(const QString &path)
{
    return suffixIn(QFileInfo(path).suffix().toLower(), kVideoSuffixes);
}

bool PlaylistModel::isImageFile(const QString &path)
{
    return suffixIn(QFileInfo(path).suffix().toLower(), kImageSuffixes);
}

bool PlaylistModel::isSupportedFile(const QString &path)
{
    return isVideoFile(path) || isImageFile(path);
}

// 播放列表即路径：扫描所选目录（不递归），按名称排序
bool PlaylistModel::setDirectory(const QString &dir)
{
    QDir d(dir);
    if (!d.exists())
        return false;

    const QStringList entries = d.entryList(QDir::Files | QDir::Readable,
                                            QDir::Name | QDir::LocaleAware);
    QStringList files;
    for (int i = 0; i < entries.size(); ++i) {
        const QString path = d.absoluteFilePath(entries.at(i));
        if (isSupportedFile(path))
            files << path;
    }

    m_directory = d.absolutePath();
    m_files = files;
    m_index = m_files.isEmpty() ? -1 : 0;
    return !m_files.isEmpty();
}

QString PlaylistModel::currentFile() const
{
    return fileAt(m_index);
}

QString PlaylistModel::fileAt(int i) const
{
    if (i < 0 || i >= m_files.size())
        return QString();
    return m_files.at(i);
}

void PlaylistModel::setCurrentIndex(int i)
{
    if (i >= 0 && i < m_files.size())
        m_index = i;
}

bool PlaylistModel::next()
{
    if (m_files.isEmpty())
        return false;
    m_index = (m_index + 1) % m_files.size();
    return true;
}

bool PlaylistModel::prev()
{
    if (m_files.isEmpty())
        return false;
    m_index = (m_index - 1 + m_files.size()) % m_files.size();
    return true;
}
