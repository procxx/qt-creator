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

#pragma once

#include <coreplugin/id.h>

#include <QObject>
#include <QString>

namespace ClangTools {
namespace Internal {

// TODO: Remove need for "saved* members
class ClangToolsSettings : public QObject
{
    Q_OBJECT
public:
    static ClangToolsSettings *instance();

    void writeSettings();

    int savedSimultaneousProcesses() const;
    bool savedBuildBeforeAnalysis() const;
    Core::Id savedDiagnosticConfigId() const;
    QString savedClangTidyExecutable() const;
    QString savedClazyStandaloneExecutable() const;

    int simultaneousProcesses() const;
    void setSimultaneousProcesses(int processes);

    bool buildBeforeAnalysis() const;
    void setBuildBeforeAnalysis(bool build);

    Core::Id diagnosticConfigId() const;
    void setDiagnosticConfigId(Core::Id id);

    QString clangTidyExecutable() const;
    void setClangTidyExecutable(const QString &path);

    QString clazyStandaloneExecutable() const;
    void setClazyStandaloneExecutable(const QString &path);

signals:
    void buildBeforeAnalysisChanged(bool checked) const;

private:
    ClangToolsSettings();
    void readSettings();

    void updateSavedBuildBeforeAnalysiIfRequired();

    int m_simultaneousProcesses = -1;
    int m_savedSimultaneousProcesses = -1;
    bool m_buildBeforeAnalysis = false;
    bool m_savedBuildBeforeAnalysis= false;
    QString m_clangTidyExecutable;
    QString m_savedClangTidyExecutable;
    QString m_clazyStandaloneExecutable;
    QString m_savedClazyStandaloneExecutable;
    Core::Id m_diagnosticConfigId;
    Core::Id m_savedDiagnosticConfigId;
};

} // namespace Internal
} // namespace ClangTools
