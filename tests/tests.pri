TEMPLATE = app

CONFIG += exceptions
CONFIG += sse sse2 rtti

greaterThan(QT_MAJOR_VERSION, 4) {
	CONFIG += c++11
}
else {
# for g++ with C++0x spec.
	QMAKE_CXXFLAGS += -std=c++0x -Wall
#	 -stdlib=libc++
}

CONFIG += console
CONFIG += testcase
CONFIG -= app_bundle #macosx

INCLUDEPATH += $${_PRO_FILE_PWD_}/../kame

win32-msvc* {
    QMAKE_CXXFLAGS += /arch:SSE2
}
else {
    QMAKE_CXXFLAGS += -mfpmath=sse -msse -msse2
}
