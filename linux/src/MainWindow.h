#pragma once
#include <QHash>
#include <QMainWindow>

#include "Commands.h"

class PlayerBackend;
class SeekBar;
class QAction;
class QLabel;
class QMenu;
class QSlider;
class QToolButton;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openPath(const QString& pathOrUrl);

protected:
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    void closeEvent(QCloseEvent*) override;

private slots:
    void rebuildTrackMenus();
    void updateTime();

private:
    QAction* addCmd(QMenu* menu, const QString& text, Cmd::Id id, const QKeySequence& key = {},
                    bool checkable = false);
    void buildMenus();
    void buildControls();

    void toggleFullscreen();
    void openFileDialog();
    void openUrlDialog();
    void adjustRate(double factor);
    void applyPanScan();
    static QString formatTime(double seconds);

    PlayerBackend* m_player = nullptr;
    QWidget* m_video = nullptr;
    SeekBar* m_seek = nullptr;
    QWidget* m_controls = nullptr;
    QLabel* m_time = nullptr;
    QSlider* m_volume = nullptr;
    QToolButton* m_btnPlay = nullptr;
    QMenu* m_audioMenu = nullptr;
    QMenu* m_subMenu = nullptr;
    QHash<int, QAction*> m_actions;

    double m_zoomLog2 = 0, m_panX = 0, m_panY = 0;
    int m_rotation = 0;
    bool m_flipV = false, m_flipH = false;
    double m_abA = -1, m_abB = -1;
    bool m_fullscreen = false;
};
