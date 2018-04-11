INCLUDEPATH += $$PWD
DEPENDEPATH += $$PWD

!build_instagram_lib:DEFINES += LIBQINSTAGRAM_LIBRARY

QT += network

SOURCES += \
    $$PWD/instagram.cpp \
    $$PWD/instagramquery.cpp

HEADERS += \
    $$PWD/instagram.h \
    $$PWD/libqinstagram_global.h \
    $$PWD/instagramquery.h
