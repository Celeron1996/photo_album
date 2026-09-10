#ifndef ALBUMWINDOW_H
#define ALBUMWINDOW_H

#include <QMainWindow>
#include <QPoint>
#include <QString>

#include "playlistmodel.h"

class QLabel;
class QListWidget;
class QMediaPlayer;
class QPushButton;
class QStackedWidget;
class QTimer;
class QVideoWidget;
class QEvent;
class QResizeEvent;
class QKeyEvent;

class AlbumWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AlbumWindow(QWidget *parent = 0);
    ~AlbumWindow();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void chooseDirectory();
    void playNext();
    void playPrev();
    void togglePlayPause();
    void toggleAutoPlay(bool on);
    void toggleMute(bool on);
    void togglePlaylist(bool on);
    void onMediaStatusChanged();
    void onPlayerError();
    void onListRowChanged(int row);
    void onImageTimeout();

private:
    void buildUi();
    void loadDirectory(const QString &dir, bool autoStart);
    void playCurrent();
    void displayImage(const QString &path);
    void displayVideo(const QString &path);
    void restartImageTimer();
    void updateStatus();
    void refreshList();
    void syncListSelection();

    QStackedWidget *m_stack;
    QLabel *m_imageLabel;
    QWidget *m_videoPage;
    QVideoWidget *m_videoWidget;
    QMediaPlayer *m_player;
    QLabel *m_statusLabel;
    QPushButton *m_playPauseButton;
    QPushButton *m_autoButton;
    QPushButton *m_muteButton;
    QPushButton *m_listButton;
    QListWidget *m_listWidget;
    QTimer *m_imageTimer;

    PlaylistModel m_playlist;
    QPoint m_pressPos;
    bool m_autoPlay;
    bool m_updatingList;
};

#endif // ALBUMWINDOW_H
