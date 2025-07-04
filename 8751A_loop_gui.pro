QT       += core gui network charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    calibratedialog.cpp \
    hp8751a.cpp \
    impedance.cpp \
    loopgain.cpp \
    main.cpp \
    networksettingsdialog.cpp \
    prologixgpib.cpp \
    startdialog.cpp

HEADERS += \
    calibratedialog.h \
    hp8751a.h \
    impedance.h \
    loopgain.h \
    networksettingsdialog.h \
    prologixgpib.h \
    startdialog.h

FORMS += \
    calibratedialog.ui \
    impedance.ui \
    loopgain.ui \
    networksettingsdialog.ui \
    startdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc

STATECHARTS +=
