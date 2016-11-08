PRI_DIR = ../../
include($${PRI_DIR}/modules.pri)

INCLUDEPATH += \
    $${_PRO_FILE_PWD_}/../../../kame/graph\

HEADERS += \
    thamwayprot.h \
    thamwaypulser.h \

SOURCES += \
    thamwayprot.cpp \
    thamwaypulser.cpp \

FORMS += \
    thamwayprotform.ui
win32: {
#exists("c:\cypress\usb\drivers\ezusbdrv\ezusbsys.h") {
    HEADERS += \
        fx2fw.h\
        cusb.h\
        ezusbthamway.h\
        thamwaydso.h

    SOURCES += \
        cusb.c\
        ezusbthamway.cpp\
        thamwaydso.cpp

    DEFINES += USE_EZUSB
    DEFINES += USE_EZUSB_FX2FW
}

unix {
    exists("/opt/local/include/libusb-1.0/libusb.h") {
        LIBS += -lusb-1.0
        HEADERS += \
            libusb2cusb.h\
            ezusbthamway.h\
            thamwaydso.h

        SOURCES += \
            libusb2cusb.cpp\
            ezusbthamway.cpp\
            thamwaydso.cpp

        DEFINES += USE_EZUSB
        DEFINES += USE_EZUSB_CYUSB
        macx: DEFINES += KAME_EZUSB_DIR=\"quotedefined(Contents/Resources/)\"
    }
    else {
        message("Missing library for libusb-1.0")
    }
}

win32:LIBS += -lcharinterface

INCLUDEPATH += $$PWD/../../charinterface
DEPENDPATH += $$PWD/../../charinterface

win32:LIBS += -lsgcore

INCLUDEPATH += $$PWD/../../sg/core
DEPENDPATH += $$PWD/../../sg/core

win32:LIBS += -lnetworkanalyzercore

INCLUDEPATH += $$PWD/../../networkanalyzer/core
DEPENDPATH += $$PWD/../../networkanalyzer/core

win32:LIBS += -lnmrpulsercore

INCLUDEPATH += $$PWD/../pulsercore
DEPENDPATH += $$PWD/../pulsercore

win32:LIBS += -ldsocore

INCLUDEPATH += $$PWD/../../dso/core
DEPENDPATH += $$PWD/../../dso/core
