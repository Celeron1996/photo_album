#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QString>
#include <QStringList>

class PlaylistModel
{
public:
    PlaylistModel();

    bool setDirectory(const QString &dir);
    QString directory() const { return m_directory; }

    int count() const { return m_files.size(); }
    int currentIndex() const { return m_index; }
    QString currentFile() const;
    QString fileAt(int i) const;

    void setCurrentIndex(int i);
    bool next();
    bool prev();

    static bool isVideoFile(const QString &path);
    static bool isImageFile(const QString &path);
    static bool isSupportedFile(const QString &path);

private:
    QStringList m_files;
    QString m_directory;
    int m_index;
};

#endif // PLAYLISTMODEL_H
