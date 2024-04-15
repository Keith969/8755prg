TEMPLATE = app
TARGET = 8755prg
DESTDIR = .

QT += core gui widgets serialport
CONFIG += release
DEFINES += QT_GUI_LIB QT_WIDGETS_LIB
INCLUDEPATH += . \
    ./GeneratedFiles \
    $(QTDIR64)/include \
    $(QTDIR64)/include/QtCore \
    $(QTDIR64)/include/QtGui \
    $(QTDIR64)/include/QtSerialPort \
    $(QTDIR64)/include/QtWidgets
DEPENDPATH += .
MOC_DIR += ./GeneratedFiles
UI_DIR += ./GeneratedFiles
OBJECTS_DIR += release
RCC_DIR += ./GeneratedFiles

HEADERS += \
    hexFile.h \
	guiMainWindow.h \
    receiverthread.h \
    senderthread.h
SOURCES += \
    hexFile.cpp \
	guiMainWindow.cpp \
    main.cpp \
    receiverthread.cpp \
    senderthread.cpp
FORMS += \
    guiMainWindow.ui

