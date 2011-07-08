include(../mumble.pri)
TEMPLATE = app
TARGET = actd
HEADERS = mainwindow.h \
		  newconfwizard.h \
		  announcement.h \
		  sessionenum.h \
		  debugbox.h

SOURCES = mainwindow.cpp \
		  newconfwizard.cpp \
		  announcement.cpp \
		  sessionenum.cpp \
		  debugbox.cpp \
		  main.cpp 

RESOURCES = actd.qrc

QMAKE_LIBDIR *= /usr/local/lib
INCLUDEPATH *= /usr/local/include
LIBS *= -lccn -lssl -lcrypto
ICON = actd.icns
CONFIG += console 
QT += xml

#QMAKE_CXXFLAGS += -Werror 

system("(test -e ~/.actd/actd_private_key.pem) &&( test -e ~/.actd/actd_cert.pem) || ./actd_initkeystore.sh")
include(../../symbols.pri)
