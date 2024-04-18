// Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "senderthread.h"

#include <QSerialPort>
#include <QTime>

// *****************************************************************************
// Function     [ constructor ]
// Description  [ ]
// *****************************************************************************
SenderThread::SenderThread(QObject *parent) :
    QThread(parent)
{
}

// *****************************************************************************
// Function     [ destructor ]
// Description  [ ]
// *****************************************************************************
SenderThread::~SenderThread()
{
    m_mutex.lock();
    m_quit = true;
    m_cond.wakeOne();
    m_mutex.unlock();
    wait();
}

// *****************************************************************************
// Function     [ transaction ]
// Description  [ The transaction for the thread to carry out. ]
// *****************************************************************************
void
SenderThread::transaction(const QString &portName, const QString &request, int waitTimeout, int baudRate, int flowControl)
{
    const QMutexLocker locker(&m_mutex);
    m_portName = portName;
    m_waitTimeout = waitTimeout;
    m_baudrate = baudRate;
    m_flowControl = flowControl;
    m_request = request;
    m_bytesSent = 0;
    m_bytesReceived = 0;

    if (!isRunning())
        start();
    else
        m_cond.wakeOne();
}

// *****************************************************************************
// Function     [ run ]
// Description  [ The thread's run body. Called when we start() the thread. ]
// *****************************************************************************
void
SenderThread::run()
{
    bool currentPortNameChanged = false;

    m_mutex.lock();

    QString currentPortName;
    if (currentPortName != m_portName) {
        currentPortName = m_portName;
        currentPortNameChanged = true;
    }

    int currentWaitTimeout = m_waitTimeout;
    QString currentRequest = m_request;
    m_mutex.unlock();

    QSerialPort serial;

    if (currentPortName.isEmpty()) {
        emit error(tr("No port name specified"));
        return;
    }

    while (!m_quit) {
        if (currentPortNameChanged) {
            serial.close();
            serial.setPortName(currentPortName);
            serial.setBaudRate(m_baudrate);
            serial.setFlowControl((QSerialPort::FlowControl) m_flowControl);

            if (!serial.open(QIODevice::ReadWrite)) {
                emit error(tr("Can't open %1, error code %2")
                           .arg(m_portName).arg(serial.error()));
                return;
            }
        }
        // write request to the PIC
        const QByteArray requestData = currentRequest.toUtf8();
        serial.write(requestData);

        // if we got a response...
        if (serial.waitForBytesWritten(m_waitTimeout)) {

            // read response from the PIC
            if (serial.waitForReadyRead(currentWaitTimeout)) {

                QByteArray responseData = serial.readAll();

                while (serial.waitForReadyRead(100)) {
                    responseData += serial.readAll();
                }

                const QString response = QString::fromUtf8(responseData);
                emit this->response(response);
            } else {
                emit timeout(tr("Wait read response timeout %1")
                             .arg(QTime::currentTime().toString()));
            }
        } else {
            emit timeout(tr("Wait write request timeout %1")
                         .arg(QTime::currentTime().toString()));
        }

        m_mutex.lock();
        m_cond.wait(&m_mutex);
        if (currentPortName != m_portName) {
            currentPortName = m_portName;
            currentPortNameChanged = true;
        } else {
            currentPortNameChanged = false;
        }
        currentWaitTimeout = m_waitTimeout;
        currentRequest = m_request;
        m_mutex.unlock();
    }
}
