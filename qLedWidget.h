#ifndef QLEDWIDGET_H
#define QLEDWIDGET_H

// *****************************************************************************
// File         [ qLedWidget.h ]
// Description  [ Implementation of the QLedWidget class ]
// Author       [ Keith Sabine ]
// *****************************************************************************

#include <QtGui/QColor>
#include <QtWidgets/QWidget>
#include <QPainter>

// *****************************************************************************
// Class        [ QLedWidget ]
// Description  [ A LED widget ]
// *****************************************************************************
class QLedWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool power READ power WRITE setPower NOTIFY powerChanged)
    // Don't allow these
    QLedWidget(const QLedWidget&)=delete;
    QLedWidget& operator=(const QLedWidget&)=delete;

public:
    explicit QLedWidget(QWidget *parent = nullptr) :
        QWidget(parent),
        m_power(false),
        m_colour(Qt::red)
    {
        setMinimumSize(QSize(20, 20));
    }

    bool power() const
    {
        return m_power;
    }

    QColor colour() const
    {
        return m_colour;
    }

signals:
    void powerChanged();
    void colourChanged();

public slots:
    void setPower(bool power)
    {
        if(power != m_power){
            m_power = power;
            emit powerChanged();
            update();
        }
    }
    void setColour(const QColor &colour)
    {
        if (colour != m_colour) {
            m_colour = colour;
            emit colourChanged();
            update();
        }
    }

protected:
    virtual void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter ledPainter(this);
        ledPainter.setRenderHint(QPainter::Antialiasing);
        QPen pen(Qt::darkGray);
        ledPainter.setPen(pen);
        if (m_power) {
            ledPainter.setBrush(m_colour);
        }
        else {
            ledPainter.setBrush(Qt::NoBrush);
        }
        ledPainter.drawEllipse(rect());
    }

private:
    bool            m_power;
    QColor          m_colour;
};

#endif /* QLEDWIDGET_H */
