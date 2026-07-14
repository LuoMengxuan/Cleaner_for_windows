QT += core gui widgets
CONFIG += c++11 release
# Qt 5.15 qmake/MSVC may generate invalid deep relative paths for Qt headers
# when the source, shadow-build directory, and Qt installation share a drive.
# The compiler already tracks these includes; omit them from qmake's dependency scan.
CONFIG -= depend_includepath
TEMPLATE = app
TARGET = VincinzoCleaner

SOURCES += \
    $$PWD/src/main.cpp \
    $$PWD/src/mainwindow.cpp \
    $$PWD/src/scanner.cpp

HEADERS += \
    $$PWD/src/mainwindow.h \
    $$PWD/src/scanner.h

FORMS += \
    $$PWD/src/mainwindow.ui

RESOURCES += \
    $$PWD/resources/app_resources.qrc

win32:RC_ICONS = $$PWD/resources/app_icon.ico

win32-msvc:LIBS += shell32.lib
win32-g++:LIBS += -lshell32
win32-msvc:QMAKE_CXXFLAGS += /utf-8

# Put the executable directly in Qt Creator's configured build directory.
DESTDIR = $$OUT_PWD
OBJECTS_DIR = build/obj
MOC_DIR = build/moc
RCC_DIR = build/rcc
UI_DIR = build/ui
