#pragma once
// Engine interface. The UI only talks to this, so libmpv can be swapped for
// GStreamer (or anything else) without touching MainWindow.

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct TrackInfo {
    int id = 0;
    QString title;
    QString lang;
    bool selected = false;
};

class PlayerBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~PlayerBackend() override = default;

    // Window id of the widget the video should be drawn into.
    virtual bool init(quintptr wid) = 0;

    virtual void open(const QString& fileOrUrl) = 0;
    virtual void close() = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void togglePause() = 0;
    virtual void stop() = 0;
    virtual void frameStep() = 0;
    virtual void frameBackStep() = 0;
    virtual void seekAbsolute(double seconds) = 0;
    virtual void seekRelative(double seconds) = 0;

    virtual double position() const = 0;
    virtual double duration() const = 0;
    virtual bool isPlaying() const = 0;
    virtual bool hasMedia() const = 0;

    virtual void setVolume(int percent) = 0;  // 0..100
    virtual int volume() const = 0;
    virtual void setMute(bool m) = 0;
    virtual bool muted() const = 0;

    virtual void setSpeed(double rate) = 0;
    virtual double speed() const = 0;

    // Video geometry. aspect: "-1" = default, otherwise "w:h".
    virtual void setAspect(const QString& aspect) = 0;
    virtual void setPanScan(double zoomLog2, double panX, double panY) = 0;
    virtual void setRotation(int degrees) = 0;
    virtual void setFlip(bool vertical, bool horizontal) = 0;

    virtual QVector<TrackInfo> audioTracks() const = 0;
    virtual QVector<TrackInfo> subtitleTracks() const = 0;
    virtual void selectAudio(int id) = 0;
    virtual void selectSubtitle(int id) = 0;  // 0 = off
    virtual void loadSubtitle(const QString& path) = 0;

    virtual void setAbLoop(double a, double b) = 0;  // negative = unset
    virtual void screenshot(const QString& path) = 0;

signals:
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void playingChanged(bool playing);
    void mediaLoaded(const QString& title);
    void mediaClosed();
    void tracksChanged();
    void endOfFile();
    void errorOccurred(const QString& message);
};
