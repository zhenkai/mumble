CONFIG += console link_pkgconfig
TEMPLATE = app
TARGET = kiwi
HEADERS = VideoWindow.h 
SOURCES =  main.cpp VideoWindow.cpp
RESOURCES = kiwi.qrc

DIST *=  

PKGCONFIG = opencv
ICON = 


