TARGET = QtInstagraph

load(qt_module)

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

DEFINES += LIBQINSTAGRAPH_LIBRARY

QT += network

SOURCES += \
    $$PWD/instagraph.cpp \
    $$PWD/instagraphrequest.cpp \
    $$PWD/cripto/hmacsha.cpp

HEADERS += \
    $$PWD/instagraph.h \
    $$PWD/libqinstagraph_global.h \
    $$PWD/instagraphrequest.h \
    $$PWD/cripto/hmacsha.h
