include(../../qtcreatorplugin.pri)
include(../../shared/clang/clang_installation.pri)
include(../../shared/clang/clang_defines.pri)

requires(!isEmpty(LLVM_VERSION))

LIBS += $$LIBCLANG_LIBS
INCLUDEPATH += $$LLVM_INCLUDEPATH

include(../../shared/yaml-cpp/yaml-cpp_installation.pri)
isEmpty(EXTERNAL_YAML_CPP_FOUND) {
    DEFINES += YAML_CPP_DLL
} else {
    LIBS += $$EXTERNAL_YAML_CPP_LIBS
    QMAKE_CXXFLAGS += $$EXTERNAL_YAML_CPP_CXXFLAGS
}

SOURCES += \
    clangfixitsrefactoringchanges.cpp \
    clangselectablefilesdialog.cpp \
    clangtoolsdiagnosticview.cpp \
    clangtoolsprojectsettingswidget.cpp \
    clangtidyclazyrunner.cpp \
    clangtidyclazytool.cpp \
    clangtool.cpp \
    clangtoolruncontrol.cpp \
    clangtoolrunner.cpp \
    clangtoolsdiagnostic.cpp \
    clangtoolsdiagnosticmodel.cpp \
    clangtoolslogfilereader.cpp \
    clangtoolsplugin.cpp \
    clangtoolsprojectsettings.cpp \
    clangtoolssettings.cpp \
    clangtoolsutils.cpp \
    settingswidget.cpp \

HEADERS += \
    clangfileinfo.h \
    clangfixitsrefactoringchanges.h \
    clangselectablefilesdialog.h \
    clangtoolsdiagnosticview.h \
    clangtoolsprojectsettingswidget.h \
    clangtidyclazyrunner.h \
    clangtidyclazytool.h \
    clangtool.h \
    clangtoolruncontrol.h \
    clangtoolrunner.h \
    clangtools_global.h \
    clangtoolsconstants.h \
    clangtoolsdiagnostic.h \
    clangtoolsdiagnosticmodel.h \
    clangtoolslogfilereader.h \
    clangtoolsplugin.h \
    clangtoolsprojectsettings.h \
    clangtoolssettings.h \
    clangtoolsutils.h \
    settingswidget.h \

FORMS += \
    clangselectablefilesdialog.ui \
    clangtoolsprojectsettingswidget.ui \
    settingswidget.ui \

equals(TEST, 1) {
    HEADERS += \
        clangtoolspreconfiguredsessiontests.h \
        clangtoolsunittests.h

    SOURCES += \
        clangtoolspreconfiguredsessiontests.cpp \
        clangtoolsunittests.cpp

    RESOURCES += clangtoolsunittests.qrc
}

DISTFILES += \
    tests/tests.pri \
    $${IDE_SOURCE_TREE}/doc/src/analyze/creator-clang-static-analyzer.qdoc
