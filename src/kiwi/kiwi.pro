CONFIG += console link_pkgconfig 
TEMPLATE = app
TARGET = kiwi
HEADERS = VideoWindow.h VideoStreamEncoder.h VideoStreamDecoder.h VideoStreamSource.h VideoStreamSink.h NdnHandler.h MediaFetcher.h SourceList.h Params.h
SOURCES =  main.cpp VideoWindow.cpp VideoStreamDecoder.cpp VideoStreamEncoder.cpp VideoStreamSource.cpp VideoStreamSink.cpp NdnHandler.cpp MediaFetcher.cpp SourceList.cpp
RESOURCES = kiwi.qrc

DIST *= kiwi.icns kiwi.svg

QMAKE_LIBDIR *= /usr/local/lib /usr/lib
INCLUDEPATH *= /usr/local/include
LIBS *= -lccn -lssl -lcrypto
PKGCONFIG = opencv libavcodec libavformat libswscale libavutil
ICON = kiwi.icns
QT += xml

