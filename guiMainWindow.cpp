// *****************************************************************************
// File         [ guiMainWindow.cpp ]
// Description  [ Implementation of the guiMainWindow class ]
// Author       [ Keith Sabine ]
// *****************************************************************************
 
#include "guiMainWindow.h"
#include <QtWidgets/QFileDialog>

guiMainWindow::guiMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    // Set connections
    QObject::connect(ui.actionOpen_HEX_file, SIGNAL(triggered()), this, SLOT(openHexFile()));
    QObject::connect(ui.actionSave_HEX_file, SIGNAL(triggered()), this, SLOT(saveHexFile()));
    QObject::connect(ui.actionQuit,          SIGNAL(triggered()), this, SLOT(quit()));
}

// *****************************************************************************
// Function     [ constructor ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::openHexFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open HEX File...", ".", "*.hex");   
    m_HexFile.readHex(fileName);
}

// *****************************************************************************
// Function     [ constructor ]
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
// Function     [ constructor ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::quit()
{
    QCoreApplication::exit();
}
