# #####################################
# version check qt
# #####################################
contains(QT_VERSION, ^4\.[0-5]\..*) { 
    message("Cannot build FsGui with Qt version $$QT_VERSION.")
    error("Use at least Qt 4.6.")
}
QT += xml
TARGET = fsphone
macx:TARGET = FSPhone
TEMPLATE = app
INCLUDEPATH = ../../../src/include \
    ../../../libs/apr/include \
    ../../../libs/libteletone/src
LIBS = -L../../../.libs \
    -lfreeswitch \
    -lm
!win32:!macx { 
    # This is here to comply with the default freeswitch installation
    QMAKE_LFLAGS += -Wl,-rpath,/usr/local/freeswitch/lib
    LIBS += -lcrypt \
        -lrt
}
SOURCES += main.cpp \
    mainwindow.cpp \
    fshost.cpp \
    call.cpp \
    mod_qsettings/mod_qsettings.cpp \
    prefdialog.cpp
HEADERS += mainwindow.h \
    fshost.h \
    call.h \
    mod_qsettings/mod_qsettings.h \
    prefdialog.h
FORMS += mainwindow.ui \
    prefdialog.ui
RESOURCES += resources.qrc
OTHER_FILES += conf/portaudio.conf.xml \
    conf/event_socket.conf.xml \
    conf/freeswitch.xml
