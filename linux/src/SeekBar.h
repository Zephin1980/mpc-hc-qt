#pragma once
#include <QWidget>

// Flat seek bar in the style of MPC-HC's CPlayerSeekBar.
class SeekBar : public QWidget {
    Q_OBJECT
public:
    explicit SeekBar(QWidget* parent = nullptr);

    void setRange(double durationSeconds);
    void setPosition(double seconds);
    void setAbMarks(double a, double b);  // negative = unset

    QSize sizeHint() const override { return {400, 20}; }

signals:
    void seekRequested(double seconds);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    double xToSeconds(int x) const;

    double m_dur = 0;
    double m_pos = 0;
    double m_a = -1;
    double m_b = -1;
    bool m_dragging = false;
};
