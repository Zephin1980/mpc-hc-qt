#include "MpvBackend.h"

#include <mpv/client.h>

#include <QByteArray>
#include <QMetaObject>
#include <QtGlobal>
#include <clocale>
#include <cstring>

namespace {
enum : uint64_t {
    ObsTimePos = 1, ObsDuration, ObsPause, ObsTracks, ObsVolume, ObsMute, ObsSpeed
};

void wakeup(void* ctx) {
    // Called from an mpv thread; bounce to the Qt thread.
    QMetaObject::invokeMethod(static_cast<MpvBackend*>(ctx), "drainEvents", Qt::QueuedConnection);
}
} // namespace

MpvBackend::MpvBackend(QObject* parent) : PlayerBackend(parent) {}

MpvBackend::~MpvBackend() {
    if (m_mpv) {
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
        mpv_terminate_destroy(m_mpv);
    }
}

bool MpvBackend::init(quintptr wid) {
    // libmpv requires the numeric locale to be "C".
    std::setlocale(LC_NUMERIC, "C");

    m_mpv = mpv_create();
    if (!m_mpv)
        return false;

    // Video output. Try the GPU renderer first and fall back to plain X11 output,
    // which works in virtual machines without 3D acceleration (e.g. Hyper-V).
    // Overrides for troubleshooting: MPC_HC_VO=x11|gpu|xv|..., MPC_HC_HWDEC=no|auto-safe|...
    const QByteArray vo = qEnvironmentVariableIsSet("MPC_HC_VO") ? qgetenv("MPC_HC_VO") : QByteArray("gpu,x11");
    const QByteArray hwdec = qEnvironmentVariableIsSet("MPC_HC_HWDEC") ? qgetenv("MPC_HC_HWDEC") : QByteArray("auto-safe");
    mpv_set_option_string(m_mpv, "vo", vo.constData());
    mpv_set_option_string(m_mpv, "hwdec", hwdec.constData());
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "idle", "yes");
    mpv_set_option_string(m_mpv, "osc", "no");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(m_mpv, "cursor-autohide", "no");
    mpv_set_option_string(m_mpv, "sub-auto", "fuzzy");

    int64_t w = static_cast<int64_t>(wid);
    mpv_set_option(m_mpv, "wid", MPV_FORMAT_INT64, &w);

    if (mpv_initialize(m_mpv) < 0) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }

    mpv_observe_property(m_mpv, ObsTimePos, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, ObsDuration, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, ObsPause, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, ObsTracks, "track-list", MPV_FORMAT_NONE);
    mpv_observe_property(m_mpv, ObsVolume, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, ObsMute, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, ObsSpeed, "speed", MPV_FORMAT_DOUBLE);

    // Forward mpv warnings/errors (video output init failures, decoder fallbacks) to stderr.
    mpv_request_log_messages(m_mpv, "warn");

    mpv_set_wakeup_callback(m_mpv, wakeup, this);
    return true;
}

void MpvBackend::command(const QStringList& args) {
    if (!m_mpv)
        return;
    QList<QByteArray> bytes;
    bytes.reserve(args.size());
    for (const QString& a : args)
        bytes.append(a.toUtf8());
    QVector<const char*> argv;
    for (const QByteArray& b : bytes)
        argv.append(b.constData());
    argv.append(nullptr);
    mpv_command_async(m_mpv, 0, argv.data());
}

void MpvBackend::setOption(const char* name, const QString& value) {
    if (m_mpv)
        mpv_set_property_string(m_mpv, name, value.toUtf8().constData());
}

void MpvBackend::open(const QString& fileOrUrl) {
    command({"loadfile", fileOrUrl, "replace"});
    setOption("pause", "no");
}

void MpvBackend::close() {
    command({"stop"});
}

void MpvBackend::play() { setOption("pause", "no"); }
void MpvBackend::pause() { setOption("pause", "yes"); }
void MpvBackend::togglePause() { command({"cycle", "pause"}); }

void MpvBackend::stop() {
    // MPC-HC's Stop returns to the start and stays stopped.
    if (!m_hasMedia)
        return;
    setOption("pause", "yes");
    command({"seek", "0", "absolute"});
}

void MpvBackend::frameStep() { command({"frame-step"}); }
void MpvBackend::frameBackStep() { command({"frame-back-step"}); }

void MpvBackend::seekAbsolute(double s) {
    command({"seek", QString::number(s, 'f', 3), "absolute+exact"});
}
void MpvBackend::seekRelative(double s) {
    command({"seek", QString::number(s, 'f', 3), "relative"});
}

void MpvBackend::setVolume(int percent) { setOption("volume", QString::number(percent)); }
void MpvBackend::setMute(bool m) { setOption("mute", m ? "yes" : "no"); }
void MpvBackend::setSpeed(double rate) { setOption("speed", QString::number(rate, 'f', 4)); }

void MpvBackend::setAspect(const QString& aspect) {
    setOption("video-aspect-override", aspect);
}

void MpvBackend::setPanScan(double zoomLog2, double panX, double panY) {
    setOption("video-zoom", QString::number(zoomLog2, 'f', 4));
    setOption("video-pan-x", QString::number(panX, 'f', 4));
    setOption("video-pan-y", QString::number(panY, 'f', 4));
}

void MpvBackend::setRotation(int degrees) {
    setOption("video-rotate", QString::number(((degrees % 360) + 360) % 360));
}

void MpvBackend::setFlip(bool vertical, bool horizontal) {
    m_flipV = vertical;
    m_flipH = horizontal;
    QStringList filters;
    if (vertical)
        filters << "vflip";
    if (horizontal)
        filters << "hflip";
    setOption("vf", filters.join(','));
}

void MpvBackend::selectAudio(int id) { setOption("aid", QString::number(id)); }
void MpvBackend::selectSubtitle(int id) { setOption("sid", id > 0 ? QString::number(id) : "no"); }
void MpvBackend::loadSubtitle(const QString& path) { command({"sub-add", path, "select"}); }

void MpvBackend::setAbLoop(double a, double b) {
    setOption("ab-loop-a", a >= 0 ? QString::number(a, 'f', 3) : "no");
    setOption("ab-loop-b", b >= 0 ? QString::number(b, 'f', 3) : "no");
}

void MpvBackend::screenshot(const QString& path) {
    command({"screenshot-to-file", path, "video"});
}

QVector<TrackInfo> MpvBackend::tracks(const char* type) const {
    QVector<TrackInfo> out;
    if (!m_mpv)
        return out;

    mpv_node list{};
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &list) < 0)
        return out;

    if (list.format == MPV_FORMAT_NODE_ARRAY) {
        for (int i = 0; i < list.u.list->num; ++i) {
            const mpv_node& item = list.u.list->values[i];
            if (item.format != MPV_FORMAT_NODE_MAP)
                continue;

            TrackInfo t;
            bool match = false;
            for (int k = 0; k < item.u.list->num; ++k) {
                const char* key = item.u.list->keys[k];
                const mpv_node& v = item.u.list->values[k];
                if (!std::strcmp(key, "type") && v.format == MPV_FORMAT_STRING)
                    match = !std::strcmp(v.u.string, type);
                else if (!std::strcmp(key, "id") && v.format == MPV_FORMAT_INT64)
                    t.id = static_cast<int>(v.u.int64);
                else if (!std::strcmp(key, "title") && v.format == MPV_FORMAT_STRING)
                    t.title = QString::fromUtf8(v.u.string);
                else if (!std::strcmp(key, "lang") && v.format == MPV_FORMAT_STRING)
                    t.lang = QString::fromUtf8(v.u.string);
                else if (!std::strcmp(key, "selected") && v.format == MPV_FORMAT_FLAG)
                    t.selected = v.u.flag != 0;
            }
            if (match)
                out.append(t);
        }
    }
    mpv_free_node_contents(&list);
    return out;
}

void MpvBackend::drainEvents() {
    while (m_mpv) {
        mpv_event* ev = mpv_wait_event(m_mpv, 0);
        if (ev->event_id == MPV_EVENT_NONE)
            break;

        switch (ev->event_id) {
        case MPV_EVENT_LOG_MESSAGE: {
            auto* m = static_cast<mpv_event_log_message*>(ev->data);
            qWarning("[mpv/%s] %s", m->prefix, QByteArray(m->text).trimmed().constData());
            break;
        }
        case MPV_EVENT_FILE_LOADED: {
            m_hasMedia = true;
            char* title = mpv_get_property_string(m_mpv, "media-title");
            emit mediaLoaded(title ? QString::fromUtf8(title) : QString());
            if (title)
                mpv_free(title);
            emit tracksChanged();
            break;
        }
        case MPV_EVENT_END_FILE: {
            auto* ef = static_cast<mpv_event_end_file*>(ev->data);
            if (ef->reason == MPV_END_FILE_REASON_ERROR)
                emit errorOccurred(QString::fromUtf8(mpv_error_string(ef->error)));
            if (ef->reason == MPV_END_FILE_REASON_EOF)
                emit endOfFile();
            if (ef->reason == MPV_END_FILE_REASON_STOP || ef->reason == MPV_END_FILE_REASON_ERROR) {
                m_hasMedia = false;
                m_pos = m_dur = 0;
                emit mediaClosed();
            }
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto* p = static_cast<mpv_event_property*>(ev->data);
            switch (ev->reply_userdata) {
            case ObsTimePos:
                if (p->format == MPV_FORMAT_DOUBLE) {
                    m_pos = *static_cast<double*>(p->data);
                    emit positionChanged(m_pos);
                }
                break;
            case ObsDuration:
                if (p->format == MPV_FORMAT_DOUBLE) {
                    m_dur = *static_cast<double*>(p->data);
                    emit durationChanged(m_dur);
                }
                break;
            case ObsPause:
                if (p->format == MPV_FORMAT_FLAG) {
                    m_paused = *static_cast<int*>(p->data) != 0;
                    emit playingChanged(isPlaying());
                }
                break;
            case ObsTracks:
                emit tracksChanged();
                break;
            case ObsVolume:
                if (p->format == MPV_FORMAT_DOUBLE)
                    m_volume = static_cast<int>(*static_cast<double*>(p->data));
                break;
            case ObsMute:
                if (p->format == MPV_FORMAT_FLAG)
                    m_muted = *static_cast<int*>(p->data) != 0;
                break;
            case ObsSpeed:
                if (p->format == MPV_FORMAT_DOUBLE)
                    m_speed = *static_cast<double*>(p->data);
                break;
            }
            break;
        }
        default:
            break;
        }
    }
}
