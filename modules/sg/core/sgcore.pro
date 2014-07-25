TEMPLATE = lib

CONFIG += static
CONFIG += qt exceptions
CONFIG += sse2 rtti

QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

greaterThan(QT_MAJOR_VERSION, 4) {
	CONFIG += c++11
}
else {
# for g++ with C++0x spec.
	QMAKE_CXXFLAGS += -std=c++0x -Wall
#	 -stdlib=libc++
}

VERSTR = '\\"4.0\\"'
DEFINES += VERSION=\"$${VERSTR}\"
DEFINES += KAME_MODULE_DIR_SURFIX=\'\"/kame/modules\"\'
greaterThan(QT_MAJOR_VERSION, 4) {
}
else {
    DEFINES += DATA_INSTALL_DIR=\'\"/usr/share/kame\"\'
}

INCLUDEPATH += \
    $${_PRO_FILE_PWD_}/../../../kame\
    $${_PRO_FILE_PWD_}/../../../kame/analyzer\
    $${_PRO_FILE_PWD_}/../../../kame/driver\
    $${_PRO_FILE_PWD_}/../../../kame/math\
#    $${_PRO_FILE_PWD_}/../../../kame/thermometer\
#    $${_PRO_FILE_PWD_}/../../../kame/graph\

HEADERS += \
    signalgenerator.h

SOURCES += \
    signalgenerator.cpp

FORMS += \
    signalgeneratorform.ui

macx {
  QMAKE_LFLAGS += -all_load  -undefined dynamic_lookup
}
