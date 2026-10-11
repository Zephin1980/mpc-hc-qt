#pragma once
#include "PlayerBackend.h"

struct mpv_handle;

class MpvBackend : public PlayerBackend {
    Q_OBJECT
public:
    explicit MpvBackend(QObject* parent = nullptr);
    ~MpvBackend() override;

    bool init(quintptr wid) override;

    void open(const QString& fileOrUrl) override;
    void close() override;
    void play() override;
    void pause() override;
    void togglePause() override;
    void stop() override;
    void frameStep() override;
    void frameBackStep() override;
    void seekAbsolute(double seconds) override;
    void seekRelative(double seconds) override;

    double position() const override { return m_pos; }
    double duration() const override { return m_dur; }
    bool isPlaying() const override { return m_hasMedia && !m_paused; }
    bool hasMedia() const override { return m_hasMedia; }

    void setVolume(int percent) override;
    int volume() const override { return m_volume; }
    void setMute(bool m) override;
    bool muted() const override { return m_muted; }

    void setSpeed(double rate) override;
    double speed() const override { return m_speed; }

    void setAspect(const QString& aspect) override;
    void setPanScan(double zoomLog2, double panX, double panY) override;
    void setRotation(int degrees) override;
    void setFlip(bool vertical, bool horizontal) override;

    QVector<TrackInfo> audioTracks() const override { return tracks("audio"); }
    QVector<TrackInfo> subtitleTracks() const override { return tracks("sub"); }
    void selectAudio(int id) override;
    void selectSubtitle(int id) override;
    void loadSubtitle(const QString& path) override;

    void setAbLoop(double a, double b) override;
    void screenshot(const QString& path) override;

private slots:
    void drainEvents();

private:
    QVector<TrackInfo> tracks(const char* type) const;
    void command(const QStringList& args);
    void setOption(const char* name, const QString& value);

    mpv_handle* m_mpv = nullptr;
    double m_pos = 0;
    double m_dur = 0;
    bool m_paused = true;
    bool m_hasMedia = false;
    int m_volume = 100;
    bool m_muted = false;
    double m_speed = 1.0;
    bool m_flipV = false;
    bool m_flipH = false;
};
