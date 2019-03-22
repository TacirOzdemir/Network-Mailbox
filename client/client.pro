TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.c
LIBS += -lws2_32
        Mswsock
        AdvApi32
