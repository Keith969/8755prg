#ifndef GUIMAINWINDOW_H
#define GUIMAINWINDOW_H

// *****************************************************************************
// File         [ guiMainWindow.h ]
// Description  [ Implementation of the guiMainWindow class ]
// Author       [ Keith Sabine ]
// *****************************************************************************

#include <QtWidgets/QMainWindow>
#include "ui_guiMainWindow.h"
#include "hexFile.h"
#include "senderthread.h"
#include "receiverthread.h"

// *****************************************************************************
// Class        [ guiMainWindow ]
// Description  [ ] 
// *****************************************************************************
class guiMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    guiMainWindow(QWidget *parent = Q_NULLPTR);

public slots:
    void openHexFile();
    void saveHexFile();
    void quit();
    void read();

    void senderShowResponse(const QString &);
    void senderProcessError(const QString &);
    void senderProcessTimeout(const QString &);

    void receiverShowRequest(const QString &);
    void receiverProcessError(const QString &);
    void receiverProcessTimeout(const QString &);


    void appendText(const QString& s) { ui.textEdit->append(s); }
    void clearText() { ui.textEdit->clear(); }

private:

    // ui
    Ui::guiMainWindowClass ui;

    // The hex file structure
    hexFile                m_HexFile;

    // Sender thread
    SenderThread           m_senderThread;

    // Receiver thread
    ReceiverThread         m_receiverThread;

    // Status bar
    QStatusBar             m_statusBar;
};

#endif /* GUIMAINWINDOW_H */
