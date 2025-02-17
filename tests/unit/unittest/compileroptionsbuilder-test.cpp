/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include "googletest.h"

#include <cpptools/compileroptionsbuilder.h>
#include <cpptools/cppprojectfile.h>
#include <cpptools/projectpart.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorerconstants.h>

using CppTools::CompilerOptionsBuilder;
using CppTools::ProjectFile;
using CppTools::ProjectPart;
using ProjectExplorer::HeaderPath;
using ProjectExplorer::HeaderPathType;
using ProjectExplorer::Project;

MATCHER_P(IsPartOfHeader, headerPart, std::string(negation ? "isn't " : "is ") + headerPart)
{
    return arg.contains(QString::fromStdString(headerPart));
}
namespace {

class CompilerOptionsBuilder : public ::testing::Test
{
protected:
    void SetUp() final
    {
        projectPart.project = project.get();
        projectPart.toolchainType = ProjectExplorer::Constants::CLANG_TOOLCHAIN_TYPEID;
        projectPart.languageVersion = Utils::LanguageVersion::CXX17;
        projectPart.toolChainWordWidth = CppTools::ProjectPart::WordWidth64Bit;
        projectPart.toolChainTargetTriple = "x86_64-apple-darwin10";
        projectPart.precompiledHeaders = QStringList{TESTDATA_DIR "/compileroptionsbuilder.pch"};
        projectPart.toolChainMacros = {ProjectExplorer::Macro{"foo", "bar"},
                                       ProjectExplorer::Macro{"__cplusplus", "2"},
                                       ProjectExplorer::Macro{"__STDC_VERSION__", "2"},
                                       ProjectExplorer::Macro{"_MSVC_LANG", "2"},
                                       ProjectExplorer::Macro{"_MSC_BUILD", "2"},
                                       ProjectExplorer::Macro{"_MSC_FULL_VER", "1900"},
                                       ProjectExplorer::Macro{"_MSC_VER", "19"}};
        projectPart.projectMacros = {ProjectExplorer::Macro{"projectFoo", "projectBar"}};
        projectPart.qtVersion = Utils::QtVersion::Qt5;

        projectPart.headerPaths = {HeaderPath{"/tmp/builtin_path", HeaderPathType::BuiltIn},
                                   HeaderPath{"/tmp/system_path", HeaderPathType::System},
                                   HeaderPath{"/tmp/path", HeaderPathType::User}};
    }

    std::unique_ptr<Project> project{std::make_unique<ProjectExplorer::Project>()};
    ProjectPart projectPart;
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart};
};

TEST_F(CompilerOptionsBuilder, AddProjectMacros)
{
    compilerOptionsBuilder.addProjectMacros();

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("-DprojectFoo=projectBar"));
}

TEST_F(CompilerOptionsBuilder, CompilerFlagsFiltering_UnknownOptionsAreForwarded)
{
    ProjectPart part = projectPart;
    part.compilerFlags = QStringList{"-fancyFlag"};

    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{part,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::No,
                                                            CppTools::UseLanguageDefines::Yes};

    compilerOptionsBuilder.build(ProjectFile::CXXSource, CppTools::UsePrecompiledHeaders::No);

    ASSERT_THAT(compilerOptionsBuilder.options(), Contains(part.compilerFlags.first()));
}

TEST_F(CompilerOptionsBuilder, CompilerFlagsFiltering_WarningsFlagsAreNotFilteredIfRequested)
{
    ProjectPart part = projectPart;
    part.compilerFlags = QStringList{"-Whello"};

    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{part,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::No,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::Yes};

    compilerOptionsBuilder.build(ProjectFile::CXXSource, CppTools::UsePrecompiledHeaders::No);

    ASSERT_THAT(compilerOptionsBuilder.options(), Contains(part.compilerFlags.first()));
}

TEST_F(CompilerOptionsBuilder, CompilerFlagsFiltering_DiagnosticOptionsAreRemoved)
{
    ProjectPart part = projectPart;
    part.compilerFlags = QStringList{"-Wbla", "-pedantic"};

    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{part,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::No,
                                                            CppTools::UseLanguageDefines::Yes};

    compilerOptionsBuilder.build(ProjectFile::CXXSource, CppTools::UsePrecompiledHeaders::No);

    ASSERT_THAT(compilerOptionsBuilder.options(), Not(Contains(part.compilerFlags[0])));
    ASSERT_THAT(compilerOptionsBuilder.options(), Not(Contains(part.compilerFlags[1])));
}

TEST_F(CompilerOptionsBuilder, CompilerFlagsFiltering_CLanguageVersionIsRewritten)
{
    ProjectPart part = projectPart;
    part.compilerFlags = QStringList{"-std=c18"};
    // We need to set the language version here to overcome a QTC_ASSERT checking
    // consistency between ProjectFile::Kind and ProjectPart::LanguageVersion
    part.languageVersion = Utils::LanguageVersion::C18;

    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{part,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::No,
                                                            CppTools::UseLanguageDefines::Yes};

    compilerOptionsBuilder.build(ProjectFile::CSource, CppTools::UsePrecompiledHeaders::No);

    ASSERT_THAT(compilerOptionsBuilder.options(), Not(Contains(part.compilerFlags.first())));
    ASSERT_THAT(compilerOptionsBuilder.options(), Contains("-std=c17"));
}

TEST_F(CompilerOptionsBuilder, CompilerFlagsFiltering_LanguageVersionIsExplicitlySetIfNotProvided)
{
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::No,
                                                            CppTools::UseLanguageDefines::Yes};

    compilerOptionsBuilder.build(ProjectFile::CXXSource, CppTools::UsePrecompiledHeaders::No);

    ASSERT_THAT(compilerOptionsBuilder.options(), Contains("-std=c++17"));
}

TEST_F(CompilerOptionsBuilder, CompilerFlagsFiltering_ClLanguageVersionIsExplicitlySetIfNotProvided)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::No,
                                                            CppTools::UseLanguageDefines::Yes};

    compilerOptionsBuilder.build(ProjectFile::CXXSource, CppTools::UsePrecompiledHeaders::No);

    ASSERT_THAT(compilerOptionsBuilder.options(), Contains("/std:c++17"));
}

TEST_F(CompilerOptionsBuilder, AddWordWidth)
{
    compilerOptionsBuilder.addWordWidth();

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("-m64"));
}

TEST_F(CompilerOptionsBuilder, HeaderPathOptionsOrder)
{
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::Yes,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::No,
                                                            "dummy_version",
                                                            ""};

    compilerOptionsBuilder.addHeaderPathOptions();

    ASSERT_THAT(compilerOptionsBuilder.options(),
                ElementsAre("-nostdinc",
                            "-nostdinc++",
                            "-I",
                            QDir::toNativeSeparators("/tmp/path"),
                            "-I",
                            QDir::toNativeSeparators("/tmp/system_path"),
                            "-isystem",
                            QDir::toNativeSeparators(CLANG_RESOURCE_DIR ""),
                            "-isystem",
                            QDir::toNativeSeparators("/tmp/builtin_path")));
}

TEST_F(CompilerOptionsBuilder, HeaderPathOptionsOrderCl)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::Yes,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::No,
                                                            "dummy_version",
                                                            ""};
    compilerOptionsBuilder.evaluateCompilerFlags();

    compilerOptionsBuilder.addHeaderPathOptions();

    ASSERT_THAT(compilerOptionsBuilder.options(),
                ElementsAre("-nostdinc",
                            "-nostdinc++",
                            "-I",
                            QDir::toNativeSeparators("/tmp/path"),
                            "-I",
                            QDir::toNativeSeparators("/tmp/system_path"),
                            "/clang:-isystem",
                            "/clang:" + QDir::toNativeSeparators(CLANG_RESOURCE_DIR ""),
                            "/clang:-isystem",
                            "/clang:" + QDir::toNativeSeparators("/tmp/builtin_path")));
}

TEST_F(CompilerOptionsBuilder, UseSystemHeader)
{
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart,
                                                            CppTools::UseSystemHeader::Yes,
                                                            CppTools::UseTweakedHeaderPaths::Yes,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::No,
                                                            "dummy_version",
                                                            ""};

    compilerOptionsBuilder.addHeaderPathOptions();

    ASSERT_THAT(compilerOptionsBuilder.options(),
                ElementsAre("-nostdinc",
                            "-nostdinc++",
                            "-I",
                            QDir::toNativeSeparators("/tmp/path"),
                            "-isystem",
                            QDir::toNativeSeparators("/tmp/system_path"),
                            "-isystem",
                            QDir::toNativeSeparators(CLANG_RESOURCE_DIR ""),
                            "-isystem",
                            QDir::toNativeSeparators("/tmp/builtin_path")));
}

TEST_F(CompilerOptionsBuilder, NoClangHeadersPath)
{
    compilerOptionsBuilder.addHeaderPathOptions();

    ASSERT_THAT(compilerOptionsBuilder.options(),
                ElementsAre("-I",
                            QDir::toNativeSeparators("/tmp/path"),
                            "-I",
                            QDir::toNativeSeparators("/tmp/system_path")));
}

TEST_F(CompilerOptionsBuilder, ClangHeadersAndCppIncludesPathsOrderMacOs)
{
    auto defaultPaths = projectPart.headerPaths;
    projectPart.headerPaths = {HeaderPath{"/usr/include/c++/4.2.1", HeaderPathType::BuiltIn},
                               HeaderPath{"/usr/include/c++/4.2.1/backward", HeaderPathType::BuiltIn},
                               HeaderPath{"/usr/local/include", HeaderPathType::BuiltIn},
                               HeaderPath{"/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../lib/clang/6.0/include", HeaderPathType::BuiltIn},
                               HeaderPath{"/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include", HeaderPathType::BuiltIn},
                               HeaderPath{"/usr/include", HeaderPathType::BuiltIn}
                              };
    projectPart.headerPaths.append(defaultPaths);
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder(projectPart,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::Yes,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::No,
                                                            "dummy_version",
                                                            "");

    compilerOptionsBuilder.addHeaderPathOptions();

    ASSERT_THAT(compilerOptionsBuilder.options(),
                ElementsAre("-nostdinc",
                            "-nostdinc++",
                            "-I",
                            QDir::toNativeSeparators("/tmp/path"),
                            "-I",
                            QDir::toNativeSeparators("/tmp/system_path"),
                            "-isystem",
                            QDir::toNativeSeparators("/usr/include/c++/4.2.1"),
                            "-isystem",
                            QDir::toNativeSeparators("/usr/include/c++/4.2.1/backward"),
                            "-isystem",
                            QDir::toNativeSeparators("/usr/local/include"),
                            "-isystem",
                            QDir::toNativeSeparators(CLANG_RESOURCE_DIR ""),
                            "-isystem",
                            QDir::toNativeSeparators(
                                "/Applications/Xcode.app/Contents/Developer/Toolchains/"
                                "XcodeDefault.xctoolchain/usr/include"),
                            "-isystem",
                            QDir::toNativeSeparators("/usr/include"),
                            "-isystem",
                            QDir::toNativeSeparators("/tmp/builtin_path")));
}

TEST_F(CompilerOptionsBuilder, ClangHeadersAndCppIncludesPathsOrderLinux)
{
    projectPart.headerPaths = {HeaderPath{"/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8", HeaderPathType::BuiltIn},
                               HeaderPath{"/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8/backward", HeaderPathType::BuiltIn},
                               HeaderPath{"/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/x86_64-linux-gnu/c++/4.8", HeaderPathType::BuiltIn},
                               HeaderPath{"/usr/local/include", HeaderPathType::BuiltIn},
                               HeaderPath{"/usr/lib/gcc/x86_64-linux-gnu/4.8/include", HeaderPathType::BuiltIn},
                               HeaderPath{"/usr/include/x86_64-linux-gnu", HeaderPathType::BuiltIn},
                               HeaderPath{"/usr/include", HeaderPathType::BuiltIn}
                              };
    projectPart.toolChainTargetTriple = "x86_64-linux-gnu";
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder(projectPart,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::Yes,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::No,
                                                            "dummy_version",
                                                            "");

    compilerOptionsBuilder.addHeaderPathOptions();

    ASSERT_THAT(
        compilerOptionsBuilder.options(),
        ElementsAre(
            "-nostdinc",
            "-nostdinc++",
            "-isystem",
            QDir::toNativeSeparators(
                "/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8"),
            "-isystem",
            QDir::toNativeSeparators(
                "/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8/backward"),
            "-isystem",
            QDir::toNativeSeparators(
                "/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/x86_64-linux-gnu/c++/4.8"),
            "-isystem",
            QDir::toNativeSeparators(CLANG_RESOURCE_DIR ""),
            "-isystem",
            QDir::toNativeSeparators("/usr/local/include"),
            "-isystem",
            QDir::toNativeSeparators("/usr/lib/gcc/x86_64-linux-gnu/4.8/include"),
            "-isystem",
            QDir::toNativeSeparators("/usr/include/x86_64-linux-gnu"),
            "-isystem",
            QDir::toNativeSeparators("/usr/include")));
}

TEST_F(CompilerOptionsBuilder, ClangHeadersAndCppIncludesPathsOrderNoVersion)
{
    projectPart.headerPaths = {
        HeaderPath{"C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include", HeaderPathType::BuiltIn},
        HeaderPath{"C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++",
                   HeaderPathType::BuiltIn},
        HeaderPath{"C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++/i686-w64-mingw32",
                   HeaderPathType::BuiltIn},
        HeaderPath{"C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++/backward",
                   HeaderPathType::BuiltIn}};
    projectPart.toolChainTargetTriple = "x86_64-w64-windows-gnu";
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder(projectPart,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::Yes,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::No,
                                                            "dummy_version",
                                                            "");

    compilerOptionsBuilder.addHeaderPathOptions();

    ASSERT_THAT(
        compilerOptionsBuilder.options(),
        ElementsAre("-nostdinc",
                    "-nostdinc++",
                    "-isystem",
                    QDir::toNativeSeparators(
                        "C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++"),
                    "-isystem",
                    QDir::toNativeSeparators(
                        "C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++/i686-w64-mingw32"),
                    "-isystem",
                    QDir::toNativeSeparators(
                        "C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++/backward"),
                    "-isystem",
                    QDir::toNativeSeparators(CLANG_RESOURCE_DIR ""),
                    "-isystem",
                    QDir::toNativeSeparators("C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include")));
}

TEST_F(CompilerOptionsBuilder, ClangHeadersAndCppIncludesPathsOrderAndroidClang)
{
    projectPart.headerPaths
        = {HeaderPath{"C:/Users/test/AppData/Local/Android/sdk/ndk-"
                      "bundle/sysroot/usr/include/i686-linux-android",
                      HeaderPathType::BuiltIn},
           HeaderPath{"C:/Users/test/AppData/Local/Android/sdk/ndk-bundle/sources/cxx-"
                      "stl/llvm-libc++/include",
                      HeaderPathType::BuiltIn},
           HeaderPath{"C:/Users/test/AppData/Local/Android/sdk/ndk-"
                      "bundle/sources/android/support/include",
                      HeaderPathType::BuiltIn},
           HeaderPath{"C:/Users/test/AppData/Local/Android/sdk/ndk-bundle/sources/cxx-"
                      "stl/llvm-libc++abi/include",
                      HeaderPathType::BuiltIn},
           HeaderPath{
               "C:/Users/test/AppData/Local/Android/sdk/ndk-bundle/sysroot/usr/include",
               HeaderPathType::BuiltIn}};
    projectPart.toolChainTargetTriple = "i686-linux-android";
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder(projectPart,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::Yes,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::No,
                                                            "dummy_version",
                                                            "");

    compilerOptionsBuilder.addHeaderPathOptions();

    ASSERT_THAT(compilerOptionsBuilder.options(),
                ElementsAre("-nostdinc",
                            "-nostdinc++",
                            "-isystem",
                            QDir::toNativeSeparators("C:/Users/test/AppData/Local/Android/sdk/ndk-"
                                                     "bundle/sources/cxx-stl/llvm-libc++/include"),
                            "-isystem",
                            QDir::toNativeSeparators(
                                "C:/Users/test/AppData/Local/Android/sdk/ndk-"
                                "bundle/sources/cxx-stl/llvm-libc++abi/include"),
                            "-isystem",
                            QDir::toNativeSeparators(CLANG_RESOURCE_DIR ""),
                            "-isystem",
                            QDir::toNativeSeparators(
                                "C:/Users/test/AppData/Local/Android/sdk/ndk-"
                                "bundle/sysroot/usr/include/i686-linux-android"),
                            "-isystem",
                            QDir::toNativeSeparators("C:/Users/test/AppData/Local/Android/sdk/ndk-"
                                                     "bundle/sources/android/support/include"),
                            "-isystem",
                            QDir::toNativeSeparators("C:/Users/test/AppData/Local/Android/sdk/ndk-"
                                                     "bundle/sysroot/usr/include")));
}

TEST_F(CompilerOptionsBuilder, NoPrecompiledHeader)
{
    compilerOptionsBuilder.addPrecompiledHeaderOptions(CppTools::UsePrecompiledHeaders::No);

    ASSERT_THAT(compilerOptionsBuilder.options().empty(), true);
}

TEST_F(CompilerOptionsBuilder, UsePrecompiledHeader)
{
    compilerOptionsBuilder.addPrecompiledHeaderOptions(CppTools::UsePrecompiledHeaders::Yes);

    ASSERT_THAT(compilerOptionsBuilder.options(),
                ElementsAre("-include", QDir::toNativeSeparators(TESTDATA_DIR "/compileroptionsbuilder.pch")));
}

TEST_F(CompilerOptionsBuilder, UsePrecompiledHeaderCl)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart};
    compilerOptionsBuilder.evaluateCompilerFlags();

    compilerOptionsBuilder.addPrecompiledHeaderOptions(CppTools::UsePrecompiledHeaders::Yes);

    ASSERT_THAT(compilerOptionsBuilder.options(),
                ElementsAre("/FI",
                            QDir::toNativeSeparators(TESTDATA_DIR "/compileroptionsbuilder.pch")));
}

TEST_F(CompilerOptionsBuilder, AddMacros)
{
    compilerOptionsBuilder.addMacros(ProjectExplorer::Macros{ProjectExplorer::Macro{"key", "value"}});

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("-Dkey=value"));
}

TEST_F(CompilerOptionsBuilder, AddTargetTriple)
{
    compilerOptionsBuilder.addTargetTriple();

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("--target=x86_64-apple-darwin10"));
}

TEST_F(CompilerOptionsBuilder, InsertWrappedQtHeaders)
{
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart,
                                                            CppTools::UseSystemHeader::Yes,
                                                            CppTools::UseTweakedHeaderPaths::Yes,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::No,
                                                            "dummy_version",
                                                            ""};

    compilerOptionsBuilder.insertWrappedQtHeaders();

    ASSERT_THAT(compilerOptionsBuilder.options(), Contains(IsPartOfHeader("wrappedQtHeaders")));
}

TEST_F(CompilerOptionsBuilder, SetLanguageVersion)
{
    compilerOptionsBuilder.updateFileLanguage(ProjectFile::CXXSource);

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("-x", "c++"));
}

TEST_F(CompilerOptionsBuilder, SetLanguageVersionCl)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart};
    compilerOptionsBuilder.evaluateCompilerFlags();

    compilerOptionsBuilder.updateFileLanguage(ProjectFile::CXXSource);

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("/TP"));
}

TEST_F(CompilerOptionsBuilder, HandleLanguageExtension)
{
    projectPart.languageExtensions = Utils::LanguageExtension::ObjectiveC;

    compilerOptionsBuilder.updateFileLanguage(ProjectFile::CXXSource);

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("-x", "objective-c++"));
}

TEST_F(CompilerOptionsBuilder, UpdateLanguageVersion)
{
    compilerOptionsBuilder.updateFileLanguage(ProjectFile::CXXSource);

    compilerOptionsBuilder.updateFileLanguage(ProjectFile::CXXHeader);

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("-x", "c++-header"));
}

TEST_F(CompilerOptionsBuilder, UpdateLanguageVersionCl)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart};
    compilerOptionsBuilder.evaluateCompilerFlags();
    compilerOptionsBuilder.updateFileLanguage(ProjectFile::CXXSource);

    compilerOptionsBuilder.updateFileLanguage(ProjectFile::CSource);

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("/TC"));
}

TEST_F(CompilerOptionsBuilder, AddMsvcCompatibilityVersion)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;
    projectPart.toolChainMacros.append(ProjectExplorer::Macro{"_MSC_FULL_VER", "190000000"});

    compilerOptionsBuilder.addMsvcCompatibilityVersion();

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("-fms-compatibility-version=19.00"));
}

TEST_F(CompilerOptionsBuilder, UndefineCppLanguageFeatureMacrosForMsvc2015)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;
    projectPart.isMsvc2015Toolchain = true;

    compilerOptionsBuilder.undefineCppLanguageFeatureMacrosForMsvc2015();

    ASSERT_THAT(compilerOptionsBuilder.options(), Contains(QString{"-U__cpp_aggregate_bases"}));
}

TEST_F(CompilerOptionsBuilder, AddDefineFunctionMacrosMsvc)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;

    compilerOptionsBuilder.addDefineFunctionMacrosMsvc();

    ASSERT_THAT(compilerOptionsBuilder.options(), Contains(QString{"-D__FUNCTION__=\"\""}));
}

TEST_F(CompilerOptionsBuilder, AddProjectConfigFileInclude)
{
    projectPart.projectConfigFile = "dummy_file.h";

    compilerOptionsBuilder.addProjectConfigFileInclude();

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("-include", "dummy_file.h"));
}

TEST_F(CompilerOptionsBuilder, AddProjectConfigFileIncludeCl)
{
    projectPart.projectConfigFile = "dummy_file.h";
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder{projectPart};
    compilerOptionsBuilder.evaluateCompilerFlags();

    compilerOptionsBuilder.addProjectConfigFileInclude();

    ASSERT_THAT(compilerOptionsBuilder.options(), ElementsAre("/FI", "dummy_file.h"));
}

TEST_F(CompilerOptionsBuilder, NoUndefineClangVersionMacrosForNewMsvc)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;

    compilerOptionsBuilder.undefineClangVersionMacrosForMsvc();

    ASSERT_THAT(compilerOptionsBuilder.options(), Not(Contains(QString{"-U__clang__"})));
}

TEST_F(CompilerOptionsBuilder, UndefineClangVersionMacrosForOldMsvc)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;
    projectPart.toolChainMacros = {ProjectExplorer::Macro{"_MSC_FULL_VER", "1300"},
                                   ProjectExplorer::Macro{"_MSC_VER", "13"}};

    compilerOptionsBuilder.undefineClangVersionMacrosForMsvc();

    ASSERT_THAT(compilerOptionsBuilder.options(), Contains(QString{"-U__clang__"}));
}

TEST_F(CompilerOptionsBuilder, BuildAllOptions)
{
    projectPart.extraCodeModelFlags = QStringList{"-arch", "x86_64"};
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder(projectPart,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::Yes,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::No,
                                                            "dummy_version",
                                                            "");

    compilerOptionsBuilder.build(ProjectFile::CXXSource, CppTools::UsePrecompiledHeaders::No);

    ASSERT_THAT(compilerOptionsBuilder.options(),
                ElementsAre("-nostdinc",
                            "-nostdinc++",
                            "-fsyntax-only",
                            "-m64",
                            "--target=x86_64-apple-darwin10",
                            "-x",
                            "c++",
                            "-std=c++17",
                            "-arch",
                            "x86_64",
                            "-DprojectFoo=projectBar",
                            "-I",
                            IsPartOfHeader("wrappedQtHeaders"),
                            "-I",
                            IsPartOfHeader(
                                QDir::toNativeSeparators("wrappedQtHeaders/QtCore").toStdString()),
                            "-I",
                            QDir::toNativeSeparators("/tmp/path"),
                            "-I",
                            QDir::toNativeSeparators("/tmp/system_path"),
                            "-isystem",
                            QDir::toNativeSeparators(CLANG_RESOURCE_DIR ""),
                            "-isystem",
                            QDir::toNativeSeparators("/tmp/builtin_path")));
}

TEST_F(CompilerOptionsBuilder, BuildAllOptionsCl)
{
    projectPart.toolchainType = ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID;
    CppTools::CompilerOptionsBuilder compilerOptionsBuilder(projectPart,
                                                            CppTools::UseSystemHeader::No,
                                                            CppTools::UseTweakedHeaderPaths::Yes,
                                                            CppTools::UseLanguageDefines::No,
                                                            CppTools::UseBuildSystemWarnings::No,
                                                            "dummy_version",
                                                            "");

    compilerOptionsBuilder.build(ProjectFile::CXXSource, CppTools::UsePrecompiledHeaders::No);

    ASSERT_THAT(compilerOptionsBuilder.options(),
                ElementsAre("-nostdinc",
                            "-nostdinc++",
                            "--driver-mode=cl",
                            "/Zs",
                            "-m64",
                            "--target=x86_64-apple-darwin10",
                            "/TP",
                            "/std:c++17",
                            "-fms-compatibility-version=19.00",
                            "-DprojectFoo=projectBar",
                            "-D__FUNCSIG__=\"\"",
                            "-D__FUNCTION__=\"\"",
                            "-D__FUNCDNAME__=\"\"",
                            "-I",
                            IsPartOfHeader("wrappedQtHeaders"),
                            "-I",
                            IsPartOfHeader(
                                QDir::toNativeSeparators("wrappedQtHeaders/QtCore").toStdString()),
                            "-I",
                            QDir::toNativeSeparators("/tmp/path"),
                            "-I",
                            QDir::toNativeSeparators("/tmp/system_path"),
                            "/clang:-isystem",
                            "/clang:" + QDir::toNativeSeparators(CLANG_RESOURCE_DIR ""),
                            "/clang:-isystem",
                            "/clang:" + QDir::toNativeSeparators("/tmp/builtin_path")));
}
}
