// *****************************************************************************
// File         [ guiMainWindow.cpp ]
// Description  [ Implementation of the guiMainWindow class ]
// Author       [ Keith Sabine ]
// *****************************************************************************
 

#include <QtWidgets/QFileDialog>
#include "guiMainWindow.h"

guiMainWindow::guiMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    // Set the textEdit font to fixed spacing
    ui.textEdit->setFontFamily("Courier");
    m_HexFile.setMainWindow(this);

    // Set connections
    QObject::connect(ui.actionOpen_HEX_file, SIGNAL(triggered()), this, SLOT(openHexFile()));
    QObject::connect(ui.actionSave_HEX_file, SIGNAL(triggered()), this, SLOT(saveHexFile()));
    QObject::connect(ui.actionQuit,          SIGNAL(triggered()), this, SLOT(quit()));
    QObject::connect(ui.readButton,          SIGNAL(pressed()),   this, SLOT(read));

    // Sender thread
    QObject::connect(&m_senderThread,        SIGNAL(response(const QString &)),  this, SLOT(senderShowResponse(const QString &)));
    QObject::connect(&m_senderThread,        SIGNAL(error(const QString &)),     this, SLOT(senderProcessError(const QString &)));
    QObject::connect(&m_senderThread,        SIGNAL(timeout(const QString &)),   this, SLOT(senderProcessTimeout(const QString &)));

    // Receiver thread
    QObject::connect(&m_receiverThread,      SIGNAL(request(const QString &)),   this, SLOT(receiverShowRequest(const QString &)));
    QObject::connect(&m_receiverThread,      SIGNAL(error(const QString &)),     this, SLOT(receiverProcessError(const QString &)));
    QObject::connect(&m_receiverThread,      SIGNAL(timeout(const QString &)),   this, SLOT(receiverProcessTimeout(const QString &)));
}

// *****************************************************************************
// Function     [ openHexFile ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::openHexFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open HEX File...", ".", "*.hex");   
    m_HexFile.readHex(fileName);
}

// *****************************************************************************
// Function     [ saveHexFile ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::saveHexFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save HEX File As...", ".", "*.hex");
    if (false == fileName.endsWith(".hex")) {
        fileName += ".hex";
    }
    m_HexFile.writeHex(fileName);
}

// *****************************************************************************
// Function     [ read ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::read()
{

}

// *****************************************************************************
// Function     [ quit ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::quit()
{
    QCoreApplication::exit();
}

// *****************************************************************************
// Function     [ senderShowResponse ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::senderShowResponse(const QString &s)
{

}

// *****************************************************************************
// Function     [ senderProcessError ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::senderProcessError(const QString &s)
{

}

// *****************************************************************************
// Function     [ senderProcessTimeout ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::senderProcessTimeout(const QString &s)
{

}

// *****************************************************************************
// Function     [ receiverShowRequest ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::receiverShowRequest(const QString &s)
{

}

// *****************************************************************************
// Function     [ receiverProcessError ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::receiverProcessError(const QString &s)
{

}

// *****************************************************************************
// Function     [ receiverProcessTimeout ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::receiverProcessTimeout(const QString &s)
{

}
