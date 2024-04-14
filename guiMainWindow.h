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

// *****************************************************************************
// Class        [ hexFile ]
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

    void appendText(const QString& s) { ui.textEdit->append(s); }
    void clearText() { ui.textEdit->clear(); }

private:
    Ui::guiMainWindowClass ui;

    // THe hex file structure
    hexFile m_HexFile;
};

#endif /* GUIMAINWINDOW_H */
