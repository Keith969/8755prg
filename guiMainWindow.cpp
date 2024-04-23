// *****************************************************************************
// File         [ guiMainWindow.cpp ]
// Description  [ Implementation of the guiMainWindow class ]
// Author       [ Keith Sabine ]
// *****************************************************************************

#include "guiMainWindow.h"

#include <QtWidgets/QFileDialog>
#include <QMessageBox>
#include <QSerialPortInfo>

// *****************************************************************************
// Function     [ constructor ]
// Description  [ ]
// *****************************************************************************
guiMainWindow::guiMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    // Set the textEdit font to fixed spacing
    ui.textEdit->setFontFamily("Courier");

    // Set connections
    QObject::connect(ui.actionOpen_HEX_file, SIGNAL(triggered()), this, SLOT(openHexFile()));
    QObject::connect(ui.actionSave_HEX_file, SIGNAL(triggered()), this, SLOT(saveHexFile()));
    QObject::connect(ui.actionQuit,          SIGNAL(triggered()), this, SLOT(quit()));
    QObject::connect(ui.readButton,          SIGNAL(pressed()),   this, SLOT(read()));
    QObject::connect(ui.checkButton,         SIGNAL(pressed()),   this, SLOT(check()));
    QObject::connect(ui.writeButton,         SIGNAL(pressed()),   this, SLOT(write()));

    // Sender read thread
    QObject::connect(&m_senderThread,        SIGNAL(response(const QString &)),  this, SLOT(senderShowResponse(const QString &)));
    QObject::connect(&m_senderThread,        SIGNAL(error(const QString &)),     this, SLOT(senderProcessError(const QString &)));
    QObject::connect(&m_senderThread,        SIGNAL(timeout(const QString &)),   this, SLOT(senderProcessTimeout(const QString &)));

    // Stuff the serial pport combo box
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QString portName = info.portName();
        // Lets ignore bluetooth stuff...
        if (! portName.contains("bluetooth", Qt::CaseInsensitive) && ! portName.contains("BLTH", Qt::CaseInsensitive)) {
            ui.serialPort->addItem(info.portName());
        }
    }

    // We only allow this baud rate for now
    ui.baudRate->addItem("115200");

    this->setStatusBar(&m_statusBar);
    statusBar()->showMessage("Ready");

    m_HexFile = new hexFile;
    m_HexFile->setMainWindow(this);
}

// *****************************************************************************
// Function     [ cdestructor ]
// Description  [ ]
// *****************************************************************************
guiMainWindow::~guiMainWindow()
{
    delete m_HexFile;
}

// *****************************************************************************
// Function     [ openHexFile ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::openHexFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open HEX File...", ".", "*.hex");
    m_HexFile->readHex(fileName);
}

// *****************************************************************************
// Function     [ saveHexFile ]
// Description  [ Save the contents of the textEdit to a hex file ]
// *****************************************************************************
void
guiMainWindow::saveHexFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save HEX File As...", ".", "*.hex");
    if (false == fileName.endsWith(".hex")) {
        fileName += ".hex";
    }

    // Clear any existing hex file
    m_HexFile->clear();

    // Read the textEdit and fill hexfile
    QString text = ui.textEdit->toPlainText();
    QStringList lines = text.split("\n", Qt::SkipEmptyParts);

    uint16_t address=0;
    const int8_t blocksize = 16;
    hexDataChunk chunk;
    bool ok=false;

    // foreach line
    for (auto line_iter = lines.begin(); line_iter != lines.end(); ++line_iter) {
        QString line = *line_iter;
        std::vector<uint8_t> data;
        uint32_t checksum=0;

        // split lines by spaces
        QStringList textlist = line.split(" ", Qt::SkipEmptyParts);
        for (auto token_iter = textlist.begin(); token_iter != textlist.end(); ++token_iter) {

            QString addr = *token_iter;
            // The first ite is the address followed by ':'
            if (addr.contains(QChar(':'))) {
                addr.remove(':');
                address = addr.toUShort(&ok, 16);
                chunk.setByteCount(blocksize);
                checksum += blocksize;
                chunk.setAddress(address);
                checksum += address & 0xff;
                checksum += (address >> 8) & 0xff;
                chunk.setRecordType(0);
                checksum += 0;
                token_iter++;
            }

            QString item = *token_iter;
            uint8_t d = (uint8_t) item.toUShort(&ok, 16);
            data.push_back(d);
            checksum += d;
            if (!ok) {
                QString message = QString("Invalid byte %1").arg(item);
                QMessageBox::warning(nullptr, "Not a valid hex value", message);
                return;
            }
        }
        chunk.setData(data);
        uint8_t lsb = ~(checksum & 0xff)+1;
        chunk.setCheckSum(lsb);
        address += blocksize;
        m_HexFile->addChunk(chunk);
    }

    m_HexFile->writeHex(fileName);
}

// *****************************************************************************
// Function     [ getFlowControl ]
// Description  [  ]
// *****************************************************************************
int32_t
guiMainWindow::getFlowControl()
{
    if (ui.flowNone->isChecked())
        return 0;
    else if (ui.flowRtsCts->isChecked())
        return 1;
    else if (ui.flowXonXoff->isChecked())
        return 2;
}

// *****************************************************************************
// Function     [ read ]
// Description  [ Send a read command to the PIC ]
// *****************************************************************************
void
guiMainWindow::read()
{
    QString portName = ui.serialPort->currentText();
    int timeout = ui.timeOut->value() * 1000;
    int baudRate = ui.baudRate->currentText().toInt();
    int flowControl = getFlowControl();

    statusBar()->showMessage(QString("Status: Running, connected to port %1.")
                                 .arg(portName));

    // Send the cmd.
    m_senderThread.transaction(portName, CMD_READ, timeout, baudRate, flowControl);

    statusBar()->showMessage("Ready");
}

// *****************************************************************************
// Function     [ check ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::check()
{
    QString portName = ui.serialPort->currentText();
    int timeout = ui.timeOut->value() * 1000;
    int baudRate = ui.baudRate->currentText().toInt();
    int flowControl = getFlowControl();

    statusBar()->showMessage(QString("Status: Running, connected to port %1.")
                                 .arg(portName));
    qApp->processEvents();

    // Send the cmd.
    m_senderThread.transaction(portName, CMD_CHEK, timeout, baudRate, flowControl);

    statusBar()->showMessage("Ready");
}

// *****************************************************************************
// Function     [ write ]
// Description  [ Write the data we read in from a hex file to the PIC ]
// *****************************************************************************
void
guiMainWindow::write()
{
    if (size() > 0) {
        QString portName = ui.serialPort->currentText();
        int timeout = ui.timeOut->value() * 1000;
        int baudRate = ui.baudRate->currentText().toInt();
        int flowControl = getFlowControl();

        statusBar()->showMessage(QString("Status: Running, connected to port %1.")
                                     .arg(portName));

        // Send the cmd, followed by the data.
        QString request(CMD_WRTE);

        // Send the data as bytes, using pairs of chars.
        std::vector<hexDataChunk> hData = m_HexFile->hexData();

        for (auto iter = hData.begin(); iter != hData.end(); ++iter) {
            hexDataChunk chunk = *iter;
            std::vector<uint8_t> data = chunk.data();
            uint8_t count = chunk.byteCount();
            for (int8_t i=0; i < count; ++i) {
                const short d = data.at(i);
                QString c=QString("%1").arg(d, 2, 16, QChar('0'));
                request.append(c);
            }
        }

        m_senderThread.transaction(portName, request, timeout, baudRate, flowControl, true);

        size_t sent = m_senderThread.bytesSent();
        size_t received = m_senderThread.bytesReceived();
        qApp->processEvents();

        statusBar()->showMessage("Ready");
    }
    else {
        clearText();
        appendText("No HEX data - please open a HEX file!\n");
    }
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
    clearText();
    appendText(s);
}

// *****************************************************************************
// Function     [ senderProcessError ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::senderProcessError(const QString &s)
{
    QString message = QString("Error %1").arg(s);
    QMessageBox::warning(nullptr, "Sender error", message);
}

// *****************************************************************************
// Function     [ senderProcessTimeout ]
// Description  [ ]
// *****************************************************************************
void
guiMainWindow::senderProcessTimeout(const QString &s)
{
    QString message = QString("Timeout %1").arg(s);
    QMessageBox::warning(nullptr, "Sender timeout", message);
}

