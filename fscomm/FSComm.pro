# #####################################
# version check qt
# #####################################
contains(QT_VERSION, ^4\.[0-5]\..*) { 
    message("Cannot build FsGui with Qt version $$QT_VERSION.")
    error("Use at least Qt 4.6.")
}
QT += xml
TARGET = fscomm
macx:TARGET = FSComm
TEMPLATE = app
INCLUDEPATH = ../src/include \
    ../libs/apr/include \
    ../libs/libteletone/src
LIBS = -L../.libs \
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
    preferences/prefdialog.cpp \
    preferences/prefportaudio.cpp \
    preferences/prefsofia.cpp \
    preferences/accountdialog.cpp
HEADERS += mainwindow.h \
    fshost.h \
    call.h \
    mod_qsettings/mod_qsettings.h \
    preferences/prefdialog.h \
    preferences/prefportaudio.h \
    preferences/prefsofia.h \
    preferences/accountdialog.h
FORMS += mainwindow.ui \
    preferences/prefdialog.ui \
    preferences/accountdialog.ui
RESOURCES += resources.qrc
OTHER_FILES += conf/portaudio.conf.xml \
    conf/event_socket.conf.xml \
    conf/freeswitch.xml \
    conf/sofia.conf.xml \
    conf/account.conf.xml
