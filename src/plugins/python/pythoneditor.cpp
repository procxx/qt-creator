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

#include "pythoneditor.h"
#include "pythonconstants.h"
#include "pythonhighlighter.h"
#include "pythonindenter.h"
#include "pythonplugin.h"
#include "pythonproject.h"
#include "pythonrunconfiguration.h"
#include "pythonsettings.h"

#include <coreplugin/infobar.h>

#include <languageclient/client.h>
#include <languageclient/languageclientinterface.h>
#include <languageclient/languageclientmanager.h>

#include <projectexplorer/session.h>
#include <projectexplorer/target.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditoractionhandler.h>
#include <texteditor/texteditorconstants.h>

#include <utils/qtcassert.h>
#include <utils/synchronousprocess.h>

#include <QCoreApplication>
#include <QRegularExpression>

using namespace ProjectExplorer;
using namespace Utils;

namespace Python {
namespace Internal {

static constexpr char startPylsInfoBarId[] = "PythonEditor::StartPyls";

struct PythonForProject
{
    FilePath path;
    PythonProject *project = nullptr;

    QString name() const
    {
        if (!path.exists())
            return {};
        if (cachedName.first != path) {
            SynchronousProcess pythonProcess;
            const CommandLine pythonVersionCommand(path, {"--version"});
            SynchronousProcessResponse response = pythonProcess.runBlocking(pythonVersionCommand);
            cachedName.first = path;
            cachedName.second = response.allOutput().trimmed();
        }
        return cachedName.second;
    }

private:
    mutable QPair<FilePath, QString> cachedName;
};

static PythonForProject detectPython(TextEditor::TextDocument *document)
{
    PythonForProject python;

    python.project = qobject_cast<PythonProject *>(
        SessionManager::projectForFile(document->filePath()));
    if (!python.project)
        python.project = qobject_cast<PythonProject *>(SessionManager::startupProject());

    if (python.project) {
        if (auto target = python.project->activeTarget()) {
            if (auto runConfig = qobject_cast<PythonRunConfiguration *>(
                    target->activeRunConfiguration())) {
                python.path = FilePath::fromString(runConfig->interpreter());
            }
        }
    }

    if (!python.path.exists())
        python.path = PythonSettings::defaultInterpreter().command;

    if (!python.path.exists() && !PythonSettings::interpreters().isEmpty())
        python.path = PythonSettings::interpreters().first().command;

    return python;
}

FilePath getPylsModulePath(CommandLine pylsCommand)
{
    pylsCommand.addArg("-h");
    SynchronousProcess pythonProcess;
    pythonProcess.setEnvironment(pythonProcess.environment() + QStringList("PYTHONVERBOSE=x"));
    SynchronousProcessResponse response = pythonProcess.runBlocking(pylsCommand);

    static const QString pylsInitPattern = "(.*)"
                                           + QRegularExpression::escape(
                                               QDir::toNativeSeparators("/pyls/__init__.py"))
                                           + '$';
    static const QString cachedPattern = " matches " + pylsInitPattern;
    static const QRegularExpression regexCached(" matches " + pylsInitPattern,
                                                QRegularExpression::MultilineOption);
    static const QRegularExpression regexNotCached(" code object from " + pylsInitPattern,
                                                   QRegularExpression::MultilineOption);

    const QString &output = response.allOutput();
    for (auto regex : {regexCached, regexNotCached}) {
        QRegularExpressionMatch result = regex.match(output);
        if (result.hasMatch())
            return FilePath::fromUserInput(result.captured(1));
    }
    return {};
}

struct PythonLanguageServerState
{
    enum { CanNotBeInstalled, CanBeInstalled, AlreadyInstalled, AlreadyConfigured } state;
    FilePath pylsModulePath;
};

static QList<const LanguageClient::StdIOSettings *> configuredPythonLanguageServer(
    Core::IDocument *doc)
{
    using namespace LanguageClient;
    QList<const StdIOSettings *> result;
    for (const BaseSettings *setting : LanguageClientManager::currentSettings()) {
        if (setting->m_languageFilter.isSupported(doc))
            result << dynamic_cast<const StdIOSettings *>(setting);
    }
    return result;
}

static PythonLanguageServerState checkPythonLanguageServer(const FilePath &python,
                                                           TextEditor::TextDocument *document)
{
    using namespace LanguageClient;
    SynchronousProcess pythonProcess;
    const CommandLine pythonLShelpCommand(python, {"-m", "pyls", "-h"});
    SynchronousProcessResponse response = pythonProcess.runBlocking(pythonLShelpCommand);
    if (response.allOutput().contains("Python Language Server")) {
        const FilePath &modulePath = getPylsModulePath(pythonLShelpCommand);
        for (const StdIOSettings *serverSetting : configuredPythonLanguageServer(document)) {
            CommandLine serverCommand(FilePath::fromUserInput(serverSetting->m_executable),
                                      serverSetting->arguments(),
                                      CommandLine::Raw);

            if (modulePath == getPylsModulePath(serverCommand))
                return {PythonLanguageServerState::AlreadyConfigured, FilePath()};
        }

        return {PythonLanguageServerState::AlreadyInstalled, getPylsModulePath(pythonLShelpCommand)};
    }

    const CommandLine pythonPipVersionCommand(python, {"-m", "pip", "-V"});
    response = pythonProcess.runBlocking(pythonPipVersionCommand);
    if (response.allOutput().startsWith("pip "))
        return {PythonLanguageServerState::CanBeInstalled, FilePath()};
    else
        return {PythonLanguageServerState::CanNotBeInstalled, FilePath()};
}

static LanguageClient::Client *registerLanguageServer(const PythonForProject &python)
{
    auto *settings = new LanguageClient::StdIOSettings();
    settings->m_executable = python.path.toString();
    settings->m_arguments = "-m pyls";
    settings->m_name = PythonEditorFactory::tr("Python Language Server (%1)").arg(python.name());
    settings->m_languageFilter.mimeTypes = QStringList(Constants::C_PY_MIMETYPE);
    LanguageClient::LanguageClientManager::registerClientSettings(settings);
    return LanguageClient::LanguageClientManager::clientForSetting(settings).value(0);
}

static void setupPythonLanguageServer(const PythonForProject &python,
                                      QPointer<TextEditor::TextDocument> document)
{
    document->infoBar()->removeInfo(startPylsInfoBarId);
    if (LanguageClient::Client *client = registerLanguageServer(python))
        LanguageClient::LanguageClientManager::reOpenDocumentWithClient(document, client);
}

static void updateEditorInfoBar(const PythonForProject &python, TextEditor::TextDocument *document)
{
    const PythonLanguageServerState &lsState = checkPythonLanguageServer(python.path, document);

    if (lsState.state == PythonLanguageServerState::CanNotBeInstalled
        || lsState.state == PythonLanguageServerState::AlreadyConfigured
        || lsState.state == PythonLanguageServerState::CanBeInstalled /* TODO */) {
        return;
    }

    Core::InfoBar *infoBar = document->infoBar();
    if (lsState.state == PythonLanguageServerState::AlreadyInstalled
        && infoBar->canInfoBeAdded(startPylsInfoBarId)) {
        auto message = PythonEditorFactory::tr("Found a Python language server for %1 (%2). "
                                               "Should this one be set up for this document?")
                           .arg(python.name(), python.path.toUserOutput());
        Core::InfoBarEntry info(startPylsInfoBarId,
                                message,
                                Core::InfoBarEntry::GlobalSuppression::Enabled);
        info.setCustomButtonInfo(TextEditor::BaseTextEditor::tr("Setup"),
                                 [=]() { setupPythonLanguageServer(python, document); });
        infoBar->addInfo(info);
    }
}

static void documentOpened(Core::IDocument *document)
{
    auto textDocument = qobject_cast<TextEditor::TextDocument *>(document);
    if (!textDocument || textDocument->mimeType() != Constants::C_PY_MIMETYPE)
        return;

    const PythonForProject &python = detectPython(textDocument);
    if (!python.path.exists())
        return;

    updateEditorInfoBar(python, textDocument);
}

PythonEditorFactory::PythonEditorFactory()
{
    setId(Constants::C_PYTHONEDITOR_ID);
    setDisplayName(
        QCoreApplication::translate("OpenWith::Editors", Constants::C_EDITOR_DISPLAY_NAME));
    addMimeType(Constants::C_PY_MIMETYPE);

    setEditorActionHandlers(TextEditor::TextEditorActionHandler::Format
                            | TextEditor::TextEditorActionHandler::UnCommentSelection
                            | TextEditor::TextEditorActionHandler::UnCollapseAll
                            | TextEditor::TextEditorActionHandler::FollowSymbolUnderCursor);

    setDocumentCreator([] { return new TextEditor::TextDocument(Constants::C_PYTHONEDITOR_ID); });
    setIndenterCreator([](QTextDocument *doc) { return new PythonIndenter(doc); });
    setSyntaxHighlighterCreator([] { return new PythonHighlighter; });
    setCommentDefinition(CommentDefinition::HashStyle);
    setParenthesesMatchingEnabled(true);
    setCodeFoldingSupported(true);

    connect(Core::EditorManager::instance(), &Core::EditorManager::documentOpened,
            this, documentOpened);
}

} // namespace Internal
} // namespace Python
