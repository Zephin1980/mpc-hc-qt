#include "MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QLineEdit>
#include <algorithm>
#include <cmath>

#include "MpvBackend.h"
#include "SeekBar.h"

namespace {
const char* kMediaFilter =
    "Media files (*.mkv *.mp4 *.avi *.mov *.webm *.m4v *.ts *.m2ts *.mpg *.mpeg *.wmv *.flv "
    "*.mp3 *.flac *.ogg *.opus *.m4a *.wav *.aac *.wma);;All files (*)";
const double kRates[] = {0.25, 0.5, 0.75, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 3.0, 4.0, 6.0, 8.0};
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("MPC-HC");
    setAcceptDrops(true);
    resize(960, 600);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Video surface: libmpv draws into this native window.
    m_video = new QWidget(central);
    m_video->setAttribute(Qt::WA_NativeWindow);
    m_video->setAttribute(Qt::WA_DontCreateNativeAncestors);
    m_video->setStyleSheet("background:black;");
    m_video->setMinimumSize(320, 180);
    m_video->setMouseTracking(true);
    layout->addWidget(m_video, 1);

    m_seek = new SeekBar(central);
    layout->addWidget(m_seek);

    setCentralWidget(central);

    m_player = new MpvBackend(this);
    if (!m_player->init(m_video->winId()))
        QMessageBox::critical(this, "MPC-HC", "Could not initialise libmpv.");

    buildControls();
    layout->addWidget(m_controls);
    buildMenus();
    statusBar()->showMessage("Ready");

    connect(m_seek, &SeekBar::seekRequested, m_player, &PlayerBackend::seekAbsolute);
    connect(m_player, &PlayerBackend::positionChanged, this, [this](double s) {
        m_seek->setPosition(s);
        updateTime();
    });
    connect(m_player, &PlayerBackend::durationChanged, this, [this](double d) {
        m_seek->setRange(d);
        updateTime();
    });
    connect(m_player, &PlayerBackend::playingChanged, this, [this](bool playing) {
        m_btnPlay->setIcon(style()->standardIcon(playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    });
    connect(m_player, &PlayerBackend::mediaLoaded, this, [this](const QString& title) {
        setWindowTitle(title.isEmpty() ? "MPC-HC" : title + " - MPC-HC");
        statusBar()->showMessage("Playing");
    });
    connect(m_player, &PlayerBackend::mediaClosed, this, [this] {
        setWindowTitle("MPC-HC");
        statusBar()->showMessage("Stopped");
        m_seek->setRange(0);
        updateTime();
    });
    connect(m_player, &PlayerBackend::errorOccurred, this,
            [this](const QString& e) { statusBar()->showMessage("Error: " + e); });
    connect(m_player, &PlayerBackend::tracksChanged, this, &MainWindow::rebuildTrackMenus);

    QSettings s;
    restoreGeometry(s.value("MainWindow/geometry").toByteArray());
    const int vol = s.value("Player/volume", 100).toInt();
    m_volume->setValue(vol);
    m_player->setVolume(vol);
}

MainWindow::~MainWindow() = default;

QAction* MainWindow::addCmd(QMenu* menu, const QString& text, Cmd::Id id, const QKeySequence& key,
                            bool checkable) {
    auto* a = menu->addAction(text);
    if (!key.isEmpty())
        a->setShortcut(key);
    a->setCheckable(checkable);
    a->setShortcutContext(Qt::ApplicationShortcut);
    m_actions.insert(id, a);
    return a;
}

void MainWindow::buildControls() {
    m_controls = new QWidget(this);
    auto* h = new QHBoxLayout(m_controls);
    h->setContentsMargins(6, 2, 6, 2);

    auto makeBtn = [&](QStyle::StandardPixmap icon, const QString& tip) {
        auto* b = new QToolButton(m_controls);
        b->setIcon(style()->standardIcon(icon));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        h->addWidget(b);
        return b;
    };

    m_btnPlay = makeBtn(QStyle::SP_MediaPlay, "Play/Pause");
    auto* stop = makeBtn(QStyle::SP_MediaStop, "Stop");
    auto* back = makeBtn(QStyle::SP_MediaSkipBackward, "Previous");
    auto* fwd = makeBtn(QStyle::SP_MediaSkipForward, "Next");
    Q_UNUSED(back);
    Q_UNUSED(fwd);

    connect(m_btnPlay, &QToolButton::clicked, m_player, &PlayerBackend::togglePause);
    connect(stop, &QToolButton::clicked, m_player, &PlayerBackend::stop);

    h->addStretch(1);
    m_time = new QLabel("00:00:00 / 00:00:00", m_controls);
    h->addWidget(m_time);

    auto* mute = makeBtn(QStyle::SP_MediaVolume, "Mute");
    connect(mute, &QToolButton::clicked, this, [this] { m_player->setMute(!m_player->muted()); });
    m_volume = new QSlider(Qt::Horizontal, m_controls);
    m_volume->setRange(0, 100);
    m_volume->setFixedWidth(90);
    h->addWidget(m_volume);
    connect(m_volume, &QSlider::valueChanged, m_player, &PlayerBackend::setVolume);
}

void MainWindow::buildMenus() {
    // ---------------- File ----------------
    QMenu* file = menuBar()->addMenu("&File");
    addCmd(file, "&Quick Open File...", Cmd::ID_FILE_OPENQUICK, Qt::CTRL | Qt::Key_Q);
    file->addSeparator();
    addCmd(file, "&Open File/URL...", Cmd::ID_FILE_OPENMEDIA, Qt::CTRL | Qt::Key_O);
    addCmd(file, "Open Dir&ectory...", Cmd::ID_FILE_OPENDIRECTORY);
    addCmd(file, "&Close", Cmd::ID_FILE_CLOSE_AND_RESTORE);
    file->addSeparator();
    addCmd(file, "Save &Image...", Cmd::ID_FILE_SAVE_IMAGE, Qt::ALT | Qt::Key_I);
    file->addSeparator();
    QMenu* subs = file->addMenu("S&ubtitles");
    addCmd(subs, "&Load Subtitles...", Cmd::ID_FILE_SUBTITLES_LOAD);
    file->addSeparator();
    addCmd(file, "E&xit", Cmd::ID_FILE_EXIT, Qt::ALT | Qt::Key_X);

    connect(m_actions[Cmd::ID_FILE_OPENQUICK], &QAction::triggered, this, &MainWindow::openFileDialog);
    connect(m_actions[Cmd::ID_FILE_OPENMEDIA], &QAction::triggered, this, &MainWindow::openUrlDialog);
    connect(m_actions[Cmd::ID_FILE_OPENDIRECTORY], &QAction::triggered, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, "Open Directory");
        if (!d.isEmpty())
            openPath(d);
    });
    connect(m_actions[Cmd::ID_FILE_CLOSE_AND_RESTORE], &QAction::triggered, m_player, &PlayerBackend::close);
    connect(m_actions[Cmd::ID_FILE_SAVE_IMAGE], &QAction::triggered, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, "Save Image", "snapshot.png", "PNG (*.png);;JPEG (*.jpg)");
        if (!f.isEmpty())
            m_player->screenshot(f);
    });
    connect(m_actions[Cmd::ID_FILE_SUBTITLES_LOAD], &QAction::triggered, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, "Load Subtitles", {},
                                                       "Subtitles (*.srt *.ass *.ssa *.sub *.idx *.vtt *.sup);;All files (*)");
        if (!f.isEmpty())
            m_player->loadSubtitle(f);
    });
    connect(m_actions[Cmd::ID_FILE_EXIT], &QAction::triggered, this, &QWidget::close);

    // ---------------- View ----------------
    QMenu* view = menuBar()->addMenu("&View");
    auto* seekVis = addCmd(view, "See&k Bar", Cmd::ID_VIEW_SEEKER, Qt::CTRL | Qt::Key_1, true);
    auto* ctlVis = addCmd(view, "&Controls", Cmd::ID_VIEW_CONTROLS, Qt::CTRL | Qt::Key_2, true);
    auto* statVis = addCmd(view, "St&atus", Cmd::ID_VIEW_STATUS, Qt::CTRL | Qt::Key_5, true);
    seekVis->setChecked(true);
    ctlVis->setChecked(true);
    statVis->setChecked(true);
    connect(seekVis, &QAction::toggled, m_seek, &QWidget::setVisible);
    connect(ctlVis, &QAction::toggled, m_controls, &QWidget::setVisible);
    connect(statVis, &QAction::toggled, statusBar(), &QWidget::setVisible);
    view->addSeparator();
    addCmd(view, "F&ull Screen", Cmd::ID_VIEW_FULLSCREEN, Qt::ALT | Qt::Key_Return);
    connect(m_actions[Cmd::ID_VIEW_FULLSCREEN], &QAction::triggered, this, &MainWindow::toggleFullscreen);
    view->addSeparator();

    QMenu* aspect = view->addMenu("&Aspect Ratio");
    auto* aspectGroup = new QActionGroup(this);
    struct AR { const char* text; Cmd::Id id; const char* val; };
    const AR ars[] = {
        {"&Default (DAR)", Cmd::ID_ASPECTRATIO_DAR, "-1"},
        {"&4:3", Cmd::ID_ASPECTRATIO_4_3, "4:3"},
        {"&5:4", Cmd::ID_ASPECTRATIO_5_4, "5:4"},
        {"&16:9", Cmd::ID_ASPECTRATIO_16_9, "16:9"},
        {"&235:100", Cmd::ID_ASPECTRATIO_235_100, "235:100"},
        {"1&85:100", Cmd::ID_ASPECTRATIO_185_100, "185:100"},
    };
    for (const AR& ar : ars) {
        QAction* a = addCmd(aspect, ar.text, ar.id, {}, true);
        aspectGroup->addAction(a);
        const QString v = ar.val;
        connect(a, &QAction::triggered, this, [this, v] { m_player->setAspect(v); });
    }
    m_actions[Cmd::ID_ASPECTRATIO_DAR]->setChecked(true);

    QMenu* pns = view->addMenu("P&nS Frame Adjust");
    addCmd(pns, "&Increase Size", Cmd::ID_VIEW_INCSIZE);
    addCmd(pns, "&Decrease Size", Cmd::ID_VIEW_DECSIZE);
    pns->addSeparator();
    addCmd(pns, "Move &Right", Cmd::ID_PANSCAN_MOVERIGHT);
    addCmd(pns, "Move &Left", Cmd::ID_PANSCAN_MOVELEFT);
    addCmd(pns, "Move &Up", Cmd::ID_PANSCAN_MOVEUP);
    addCmd(pns, "Move &Down", Cmd::ID_PANSCAN_MOVEDOWN);
    addCmd(pns, "Cen&ter", Cmd::ID_PANSCAN_CENTER);
    pns->addSeparator();
    addCmd(pns, "Rotate Cloc&kwise", Cmd::ID_PANSCAN_ROTATEZM);
    addCmd(pns, "Rotate Counter-Clock&wise", Cmd::ID_PANSCAN_ROTATEZ270);
    addCmd(pns, "&Flip Vertically", Cmd::ID_PANSCAN_ROTATEXM);
    addCmd(pns, "&Mirror", Cmd::ID_PANSCAN_ROTATEYM);
    pns->addSeparator();
    addCmd(pns, "Re&set", Cmd::ID_VIEW_RESET);

    auto pan = [this](Cmd::Id id, double dz, double dx, double dy) {
        connect(m_actions[id], &QAction::triggered, this, [this, dz, dx, dy] {
            m_zoomLog2 += dz;
            m_panX += dx;
            m_panY += dy;
            applyPanScan();
        });
    };
    pan(Cmd::ID_VIEW_INCSIZE, 0.05, 0, 0);
    pan(Cmd::ID_VIEW_DECSIZE, -0.05, 0, 0);
    pan(Cmd::ID_PANSCAN_MOVERIGHT, 0, 0.02, 0);
    pan(Cmd::ID_PANSCAN_MOVELEFT, 0, -0.02, 0);
    pan(Cmd::ID_PANSCAN_MOVEUP, 0, 0, -0.02);
    pan(Cmd::ID_PANSCAN_MOVEDOWN, 0, 0, 0.02);
    connect(m_actions[Cmd::ID_PANSCAN_CENTER], &QAction::triggered, this, [this] {
        m_panX = m_panY = 0;
        applyPanScan();
    });
    connect(m_actions[Cmd::ID_PANSCAN_ROTATEZM], &QAction::triggered, this, [this] {
        m_rotation = (m_rotation + 90) % 360;
        m_player->setRotation(m_rotation);
    });
    connect(m_actions[Cmd::ID_PANSCAN_ROTATEZ270], &QAction::triggered, this, [this] {
        m_rotation = (m_rotation + 270) % 360;
        m_player->setRotation(m_rotation);
    });
    connect(m_actions[Cmd::ID_PANSCAN_ROTATEXM], &QAction::triggered, this, [this] {
        m_flipV = !m_flipV;
        m_player->setFlip(m_flipV, m_flipH);
    });
    connect(m_actions[Cmd::ID_PANSCAN_ROTATEYM], &QAction::triggered, this, [this] {
        m_flipH = !m_flipH;
        m_player->setFlip(m_flipV, m_flipH);
    });
    connect(m_actions[Cmd::ID_VIEW_RESET], &QAction::triggered, this, [this] {
        m_zoomLog2 = m_panX = m_panY = 0;
        m_rotation = 0;
        m_flipV = m_flipH = false;
        applyPanScan();
        m_player->setRotation(0);
        m_player->setFlip(false, false);
    });

    view->addSeparator();
    QMenu* onTop = view->addMenu("On &Top");
    auto* topAlways = addCmd(onTop, "&Always", Cmd::ID_ONTOP_ALWAYS, {}, true);
    connect(topAlways, &QAction::toggled, this, [this](bool on) {
        setWindowFlag(Qt::WindowStaysOnTopHint, on);
        show();
    });

    // ---------------- Play ----------------
    QMenu* play = menuBar()->addMenu("&Play");
    addCmd(play, "&Play/Pause", Cmd::ID_PLAY_PLAYPAUSE, Qt::Key_Space);
    addCmd(play, "&Stop", Cmd::ID_PLAY_STOP, Qt::Key_Period);
    addCmd(play, "Fra&me Step", Cmd::ID_PLAY_FRAMESTEP, Qt::CTRL | Qt::Key_Right);
    addCmd(play, "Frame Step &Back", Cmd::ID_PLAY_FRAMESTEP_BACK, Qt::CTRL | Qt::Key_Left);
    connect(m_actions[Cmd::ID_PLAY_PLAYPAUSE], &QAction::triggered, m_player, &PlayerBackend::togglePause);
    connect(m_actions[Cmd::ID_PLAY_STOP], &QAction::triggered, m_player, &PlayerBackend::stop);
    connect(m_actions[Cmd::ID_PLAY_FRAMESTEP], &QAction::triggered, m_player, &PlayerBackend::frameStep);
    connect(m_actions[Cmd::ID_PLAY_FRAMESTEP_BACK], &QAction::triggered, m_player, &PlayerBackend::frameBackStep);

    QMenu* repeat = play->addMenu("&Repeat");
    addCmd(repeat, "A-B (&Reset)", Cmd::ID_PLAY_REPEAT_AB);
    addCmd(repeat, "A-B (Mark &A)", Cmd::ID_PLAY_REPEAT_AB_MARK_A);
    addCmd(repeat, "A-B (Mark &B)", Cmd::ID_PLAY_REPEAT_AB_MARK_B);
    connect(m_actions[Cmd::ID_PLAY_REPEAT_AB_MARK_A], &QAction::triggered, this, [this] {
        m_abA = m_player->position();
        m_player->setAbLoop(m_abA, m_abB);
        m_seek->setAbMarks(m_abA, m_abB);
    });
    connect(m_actions[Cmd::ID_PLAY_REPEAT_AB_MARK_B], &QAction::triggered, this, [this] {
        m_abB = m_player->position();
        m_player->setAbLoop(m_abA, m_abB);
        m_seek->setAbMarks(m_abA, m_abB);
    });
    connect(m_actions[Cmd::ID_PLAY_REPEAT_AB], &QAction::triggered, this, [this] {
        m_abA = m_abB = -1;
        m_player->setAbLoop(-1, -1);
        m_seek->setAbMarks(-1, -1);
    });

    QMenu* rate = play->addMenu("&Playback Rate");
    auto* rateGroup = new QActionGroup(this);
    for (double r : kRates) {
        auto* a = rate->addAction(QString::asprintf("%.2f x", r));
        a->setCheckable(true);
        rateGroup->addAction(a);
        if (r == 1.0)
            a->setChecked(true);
        connect(a, &QAction::triggered, this, [this, r] { m_player->setSpeed(r); });
    }
    rate->addSeparator();
    addCmd(rate, "&Decrease Rate", Cmd::ID_PLAY_DECRATE, Qt::CTRL | Qt::Key_Down);
    addCmd(rate, "&Increase Rate", Cmd::ID_PLAY_INCRATE, Qt::CTRL | Qt::Key_Up);
    addCmd(rate, "R&eset Rate", Cmd::ID_PLAY_RESETRATE, Qt::CTRL | Qt::Key_R);
    connect(m_actions[Cmd::ID_PLAY_DECRATE], &QAction::triggered, this, [this] { adjustRate(1 / 1.25); });
    connect(m_actions[Cmd::ID_PLAY_INCRATE], &QAction::triggered, this, [this] { adjustRate(1.25); });
    connect(m_actions[Cmd::ID_PLAY_RESETRATE], &QAction::triggered, this, [this] { m_player->setSpeed(1.0); });

    play->addSeparator();
    m_audioMenu = play->addMenu("&Audio Track");
    m_subMenu = play->addMenu("Su&btitle Track");
    play->addSeparator();
    QMenu* vol = play->addMenu("&Volume");
    addCmd(vol, "&Up", Cmd::ID_VOLUME_UP, Qt::Key_Up);
    addCmd(vol, "&Down", Cmd::ID_VOLUME_DOWN, Qt::Key_Down);
    addCmd(vol, "&Mute", Cmd::ID_VOLUME_MUTE, Qt::CTRL | Qt::Key_M);
    connect(m_actions[Cmd::ID_VOLUME_UP], &QAction::triggered, this, [this] { m_volume->setValue(m_volume->value() + 5); });
    connect(m_actions[Cmd::ID_VOLUME_DOWN], &QAction::triggered, this, [this] { m_volume->setValue(m_volume->value() - 5); });
    connect(m_actions[Cmd::ID_VOLUME_MUTE], &QAction::triggered, this, [this] { m_player->setMute(!m_player->muted()); });

    // ---------------- Navigate ----------------
    QMenu* nav = menuBar()->addMenu("&Navigate");
    addCmd(nav, "&Jump Forward (medium)", Cmd::ID_PLAY_SEEKFORWARDMED, Qt::Key_Right);
    addCmd(nav, "J&ump Backward (medium)", Cmd::ID_PLAY_SEEKBACKWARDMED, Qt::Key_Left);
    connect(m_actions[Cmd::ID_PLAY_SEEKFORWARDMED], &QAction::triggered, this, [this] { m_player->seekRelative(5); });
    connect(m_actions[Cmd::ID_PLAY_SEEKBACKWARDMED], &QAction::triggered, this, [this] { m_player->seekRelative(-5); });

    // ---------------- Help ----------------
    QMenu* help = menuBar()->addMenu("&Help");
    auto* about = help->addAction("&About...");
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, "About MPC-HC (Linux)",
                           "MPC-HC Linux port (early skeleton)\nQt 6 + libmpv\n\n"
                           "Based on MPC-HC, licensed under GPLv3.");
    });

    rebuildTrackMenus();
}

void MainWindow::rebuildTrackMenus() {
    auto fill = [this](QMenu* menu, const QVector<TrackInfo>& list, bool allowOff, bool audio) {
        menu->clear();
        auto* group = new QActionGroup(menu);
        if (allowOff) {
            QAction* off = menu->addAction("Disabled");
            off->setCheckable(true);
            group->addAction(off);
            bool anySelected = false;
            for (const TrackInfo& t : list)
                anySelected = anySelected || t.selected;
            off->setChecked(!anySelected);
            connect(off, &QAction::triggered, this, [this] { m_player->selectSubtitle(0); });
        }
        for (const TrackInfo& t : list) {
            QString label = QString("%1: %2").arg(t.id).arg(t.title.isEmpty() ? t.lang : t.title);
            if (label.endsWith(": "))
                label.chop(2);
            QAction* a = menu->addAction(label);
            a->setCheckable(true);
            a->setChecked(t.selected);
            group->addAction(a);
            const int id = t.id;
            connect(a, &QAction::triggered, this, [this, id, audio] {
                if (audio)
                    m_player->selectAudio(id);
                else
                    m_player->selectSubtitle(id);
            });
        }
        menu->setEnabled(!menu->actions().isEmpty());
    };
    fill(m_audioMenu, m_player->audioTracks(), false, true);
    fill(m_subMenu, m_player->subtitleTracks(), true, false);
}

void MainWindow::updateTime() {
    m_time->setText(formatTime(m_player->position()) + " / " + formatTime(m_player->duration()));
}

QString MainWindow::formatTime(double seconds) {
    const int s = int(std::max(0.0, seconds));
    return QString::asprintf("%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
}

void MainWindow::adjustRate(double factor) {
    m_player->setSpeed(std::clamp(m_player->speed() * factor, 0.25, 8.0));
}

void MainWindow::applyPanScan() {
    m_player->setPanScan(m_zoomLog2, m_panX, m_panY);
}

void MainWindow::openPath(const QString& pathOrUrl) {
    m_player->open(pathOrUrl);
}

void MainWindow::openFileDialog() {
    const QString f = QFileDialog::getOpenFileName(this, "Quick Open File", {}, kMediaFilter);
    if (!f.isEmpty())
        openPath(f);
}

void MainWindow::openUrlDialog() {
    bool ok = false;
    const QString s = QInputDialog::getText(this, "Open", "Type the address of a file or URL:",
                                            QLineEdit::Normal, {}, &ok);
    if (ok && !s.trimmed().isEmpty())
        openPath(s.trimmed());
    else if (ok)
        openFileDialog();
}

void MainWindow::toggleFullscreen() {
    m_fullscreen = !m_fullscreen;
    menuBar()->setVisible(!m_fullscreen);
    statusBar()->setVisible(!m_fullscreen && m_actions[Cmd::ID_VIEW_STATUS]->isChecked());
    m_controls->setVisible(!m_fullscreen && m_actions[Cmd::ID_VIEW_CONTROLS]->isChecked());
    m_seek->setVisible(!m_fullscreen && m_actions[Cmd::ID_VIEW_SEEKER]->isChecked());
    if (m_fullscreen)
        showFullScreen();
    else
        showNormal();
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent*) {
    toggleFullscreen();
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape && m_fullscreen) {
        toggleFullscreen();
        return;
    }
    QMainWindow::keyPressEvent(e);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    const auto urls = e->mimeData()->urls();
    if (!urls.isEmpty())
        openPath(urls.first().isLocalFile() ? urls.first().toLocalFile() : urls.first().toString());
}

void MainWindow::closeEvent(QCloseEvent* e) {
    QSettings s;
    s.setValue("MainWindow/geometry", saveGeometry());
    s.setValue("Player/volume", m_volume->value());
    QMainWindow::closeEvent(e);
}
