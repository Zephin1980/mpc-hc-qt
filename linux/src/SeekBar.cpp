#include "SeekBar.h"

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

SeekBar::SeekBar(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(20);
    setCursor(Qt::PointingHandCursor);
}

void SeekBar::setRange(double d) {
    m_dur = std::max(0.0, d);
    update();
}

void SeekBar::setPosition(double s) {
    if (m_dragging)
        return;
    m_pos = s;
    update();
}

void SeekBar::setAbMarks(double a, double b) {
    m_a = a;
    m_b = b;
    update();
}

double SeekBar::xToSeconds(int x) const {
    const int margin = 6;
    const double w = std::max(1, width() - 2 * margin);
    const double f = std::clamp((x - margin) / w, 0.0, 1.0);
    return f * m_dur;
}

void SeekBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int margin = 6;
    const QRect track(margin, height() / 2 - 3, width() - 2 * margin, 6);
    p.setPen(Qt::NoPen);
    p.setBrush(palette().color(QPalette::Mid));
    p.drawRoundedRect(track, 2, 2);

    if (m_dur <= 0)
        return;

    const double f = std::clamp(m_pos / m_dur, 0.0, 1.0);
    QRect done = track;
    done.setWidth(int(track.width() * f));
    p.setBrush(palette().color(QPalette::Highlight));
    p.drawRoundedRect(done, 2, 2);

    auto mark = [&](double t) {
        if (t < 0)
            return;
        const int x = track.left() + int(track.width() * std::clamp(t / m_dur, 0.0, 1.0));
        p.setPen(QPen(palette().color(QPalette::WindowText), 1));
        p.drawLine(x, track.top() - 3, x, track.bottom() + 3);
    };
    mark(m_a);
    mark(m_b);

    // Thumb
    const int tx = track.left() + done.width();
    p.setPen(Qt::NoPen);
    p.setBrush(palette().color(QPalette::ButtonText));
    p.drawRect(QRect(tx - 2, height() / 2 - 8, 4, 16));
}

void SeekBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton || m_dur <= 0)
        return;
    m_dragging = true;
    m_pos = xToSeconds(int(e->position().x()));
    update();
}

void SeekBar::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging)
        return;
    m_pos = xToSeconds(int(e->position().x()));
    update();
}

void SeekBar::mouseReleaseEvent(QMouseEvent* e) {
    if (!m_dragging || e->button() != Qt::LeftButton)
        return;
    m_dragging = false;
    emit seekRequested(xToSeconds(int(e->position().x())));
}
