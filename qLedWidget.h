#ifndef QLEDWIDGET_H
#define QLEDWIDGET_H

// *****************************************************************************
// File         [ qLedWidget.h ]
// Description  [ Implementation of the QLedWidget class ]
// Author       [ Keith Sabine ]
// *****************************************************************************

#include <QWidget>
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
        m_power(false) {}

    bool power() const
    {
        return m_power;
    }

signals:
    void powerChanged();

public slots:
    void setPower(bool power)
    {
        if(power!=m_power){
            m_power = power;
            emit powerChanged();
            update();
        }
    }

protected:
    virtual void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter ledPainter(this);
        ledPainter.setPen(Qt::black);
        if(m_power)
            ledPainter.setBrush(Qt::red);
        else
            ledPainter.setBrush(Qt::NoBrush);
        ledPainter.drawEllipse(rect());
    }

private:
    bool m_power;
};

#endif /* QLEDWIDGET_H */
