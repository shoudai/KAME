PRI_DIR = ../
include($${PRI_DIR}/modules.pri)

QT += opengl

INCLUDEPATH += \
    $${_PRO_FILE_PWD_}/../../kame/graph\
    $${_PRO_FILE_PWD_}/../../kame/icons\

INCLUDEPATH += $$OUT_PWD/../../kame
DEPENDPATH += $$OUT_PWD/../../kame

HEADERS += \
    autolctuner.h \
    nmrfspectrum.h \
    nmrpulse.h \
    nmrrelax.h \
    nmrrelaxfit.h \
    nmrspectrum.h \
    nmrspectrumbase_impl.h \
    nmrspectrumbase.h \
    nmrspectrumsolver.h \
    pulseanalyzer.h \

SOURCES += \
    autolctuner.cpp \
    nmrfspectrum.cpp \
    nmrpulse.cpp \
    nmrrelax.cpp \
    nmrrelaxfit.cpp \
    nmrspectrum.cpp \
    nmrspectrumsolver.cpp \
    pulseanalyzer.cpp \

FORMS += \
    autolctunerform.ui \
    nmrfspectrumform.ui \
    nmrpulseform.ui \
    nmrrelaxform.ui \
    nmrspectrumform.ui

unix: PKGCONFIG += fftw3
unix: PKGCONFIG += gsl

LIBS += -lcharinterface

INCLUDEPATH += $$PWD/../charinterface
DEPENDPATH += $$PWD/../charinterface

LIBS +=  -lnmrpulsercore

INCLUDEPATH += $$PWD/pulsercore
DEPENDPATH += $$PWD/pulsercore

LIBS += -lsgcore

INCLUDEPATH += $$PWD/../sg/core
DEPENDPATH += $$PWD/../sg/core

LIBS += -ldsocore

INCLUDEPATH += $$PWD/../dso/core
DEPENDPATH += $$PWD/../dso/core

LIBS += -lmotorcore

INCLUDEPATH += $$PWD/../motor/core
DEPENDPATH += $$PWD/../motor/core

LIBS += -lnetworkanalyzercore

INCLUDEPATH += $$PWD/../networkanalyzer/core
DEPENDPATH += $$PWD/../networkanalyzer/core

LIBS += -ldmmcore

INCLUDEPATH += $$PWD/../dmm/core
DEPENDPATH += $$PWD/../dmm/core

LIBS += -lmagnetpscore

INCLUDEPATH += $$PWD/../magnetps/core
DEPENDPATH += $$PWD/../magnetps/core