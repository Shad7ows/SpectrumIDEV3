QT += core testlib
QT -= gui

CONFIG += console testcase c++23
TEMPLATE = app
TARGET = tst_TLanguageRegistry

INCLUDEPATH += ../../source/languages

SOURCES += \
    ../../source/languages/TLanguageProfile.cpp \
    tst_TLanguageRegistry.cpp

HEADERS += \
    ../../source/languages/TLanguageProfile.h
