PRI_DIR = ../
include($${PRI_DIR}/modules.pri)

HEADERS += \
    userdcsource.h

SOURCES += \
    userdcsource.cpp

#FORMS +=

macx {
  QMAKE_LFLAGS += -all_load  -undefined dynamic_lookup
}

LIBS += -lcharinterface

INCLUDEPATH += $$PWD/../charinterface
DEPENDPATH += $$PWD/../charinterface

LIBS += -ldcsourcecore

INCLUDEPATH += $$PWD/core
DEPENDPATH += $$PWD/core
