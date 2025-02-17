/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "clangtoolsunittests.h"

#include "clangtidyclazytool.h"
#include "clangtoolsdiagnostic.h"
#include "clangtoolssettings.h"
#include "clangtoolsutils.h"

#include <coreplugin/icore.h>
#include <cpptools/cppcodemodelsettings.h>
#include <cpptools/cppmodelmanager.h>
#include <cpptools/cpptoolstestcase.h>
#include <cpptools/cpptoolsreuse.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/toolchain.h>

#include <utils/executeondestruction.h>
#include <utils/fileutils.h>

#include <QEventLoop>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

using namespace ProjectExplorer;
using namespace Utils;

Q_DECLARE_METATYPE(CppTools::ClangDiagnosticConfig)

namespace ClangTools {
namespace Internal {

void ClangToolsUnitTests::initTestCase()
{
    const QList<Kit *> allKits = KitManager::kits();
    if (allKits.count() != 1)
        QSKIP("This test requires exactly one kit to be present");
    const ToolChain * const toolchain = ToolChainKitAspect::toolChain(allKits.first(),
                                                                           Constants::CXX_LANGUAGE_ID);
    if (!toolchain)
        QSKIP("This test requires that there is a kit with a toolchain.");

    if (Core::ICore::clangExecutable(CLANG_BINDIR).isEmpty())
        QSKIP("No clang suitable for analyzing found");

    m_tmpDir = new CppTools::Tests::TemporaryCopiedDir(QLatin1String(":/unit-tests"));
    QVERIFY(m_tmpDir->isValid());
}

void ClangToolsUnitTests::cleanupTestCase()
{
    delete m_tmpDir;
}

static CppTools::ClangDiagnosticConfig configFor(const QString &tidyChecks,
                                                 const QString &clazyChecks)
{
    CppTools::ClangDiagnosticConfig config;
    config.setId("Test.MyTestConfig");
    config.setDisplayName("Test");
    config.setIsReadOnly(true);
    config.setClangOptions(QStringList{QStringLiteral("-Wno-everything")});
    config.setClangTidyMode(CppTools::ClangDiagnosticConfig::TidyMode::ChecksPrefixList);
    config.setClangTidyChecks("-*," + tidyChecks);
    config.setClazyChecks(clazyChecks);
    return config;
}

void ClangToolsUnitTests::testProject()
{
    QFETCH(QString, projectFilePath);
    QFETCH(int, expectedDiagCount);
    QFETCH(CppTools::ClangDiagnosticConfig, diagnosticConfig);
    if (projectFilePath.contains("mingw")) {
        const ToolChain * const toolchain
                = ToolChainKitAspect::toolChain(KitManager::kits().constFirst(),
                                                Constants::CXX_LANGUAGE_ID);
        if (toolchain->typeId() != ProjectExplorer::Constants::MINGW_TOOLCHAIN_TYPEID)
            QSKIP("This test is mingw specific, does not run for other toolchains");
    }

    CppTools::Tests::ProjectOpenerAndCloser projectManager;
    const CppTools::ProjectInfo projectInfo = projectManager.open(projectFilePath, true);
    QVERIFY(projectInfo.isValid());
    ClangTool *tool = ClangTidyClazyTool::instance();

    // Change configs
    QSharedPointer<CppTools::CppCodeModelSettings> cppToolsSettings = CppTools::codeModelSettings();
    ClangToolsSettings *clangToolsSettings = ClangToolsSettings::instance();
    const CppTools::ClangDiagnosticConfigs originalConfigs = cppToolsSettings
                                                                 ->clangCustomDiagnosticConfigs();
    const Core::Id originalId = clangToolsSettings->diagnosticConfigId();

    CppTools::ClangDiagnosticConfigs modifiedConfigs = originalConfigs;
    modifiedConfigs.push_back(diagnosticConfig);

    ExecuteOnDestruction executeOnDestruction([=]() {
        // Restore configs
        cppToolsSettings->setClangCustomDiagnosticConfigs(originalConfigs);
        clangToolsSettings->setDiagnosticConfigId(originalId);
        clangToolsSettings->writeSettings();
    });

    cppToolsSettings->setClangCustomDiagnosticConfigs(modifiedConfigs);
    clangToolsSettings->setDiagnosticConfigId(diagnosticConfig.id());
    clangToolsSettings->writeSettings();

    tool->startTool(ClangTidyClazyTool::FileSelection::AllFiles);
    QSignalSpy waiter(tool, SIGNAL(finished(bool)));
    QVERIFY(waiter.wait(30000));

    const QList<QVariant> arguments = waiter.takeFirst();
    QVERIFY(arguments.first().toBool());
    QCOMPARE(tool->diagnostics().count(), expectedDiagCount);
}

void ClangToolsUnitTests::testProject_data()
{
    QTest::addColumn<QString>("projectFilePath");
    QTest::addColumn<int>("expectedDiagCount");
    QTest::addColumn<CppTools::ClangDiagnosticConfig>("diagnosticConfig");

    // Test simple C++ project.
    CppTools::ClangDiagnosticConfig config = configFor("modernize-use-nullptr", QString());
    addTestRow("simple/simple.qbs", 1, config);
    addTestRow("simple/simple.pro", 1, config);

    // Test simple Qt project.
    config = configFor("readability-static-accessed-through-instance", QString());
    addTestRow("qt-widgets-app/qt-widgets-app.qbs", 1, config);
    addTestRow("qt-widgets-app/qt-widgets-app.pro", 1, config);

    // Test that libraries can be analyzed.
    config = configFor(QString(), QString());
    addTestRow("simple-library/simple-library.qbs", 0, config);
    addTestRow("simple-library/simple-library.pro", 0, config);

    // Test that standard headers can be parsed.
    addTestRow("stdc++11-includes/stdc++11-includes.qbs", 0, config);
    addTestRow("stdc++11-includes/stdc++11-includes.pro", 0, config);

    // Test that qt essential headers can be parsed.
    addTestRow("qt-essential-includes/qt-essential-includes.qbs", 0, config);
    addTestRow("qt-essential-includes/qt-essential-includes.pro", 0, config);

    // Test that mingw includes can be parsed.
    addTestRow("mingw-includes/mingw-includes.qbs", 0, config);
    addTestRow("mingw-includes/mingw-includes.pro", 0, config);

    // Test that tidy and clazy diagnostics are emitted for the same project.
    addTestRow("clangtidy_clazy/clangtidy_clazy.pro",
               1 /*tidy*/ + 1 /*clazy*/,
               configFor("misc-unconventional-assign-operator", "base-class-event"));
}

void ClangToolsUnitTests::addTestRow(const QByteArray &relativeFilePath,
                                     int expectedDiagCount,
                                     const CppTools::ClangDiagnosticConfig &diagnosticConfig)
{
    const QString absoluteFilePath = m_tmpDir->absolutePath(relativeFilePath);
    const QString fileName = QFileInfo(absoluteFilePath).fileName();

    QTest::newRow(fileName.toUtf8().constData())
        << absoluteFilePath << expectedDiagCount << diagnosticConfig;
}

} // namespace Internal
} // namespace ClangTools
