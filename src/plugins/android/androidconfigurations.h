/****************************************************************************
**
** Copyright (C) 2016 BogDan Vatra <bog_dan_ro@yahoo.com>
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

#include "android_global.h"
#include "androidsdkpackage.h"
#include <projectexplorer/toolchain.h>

#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QHash>
#include <QMap>
#include <QFutureInterface>
#include <QVersionNumber>

#include <utils/fileutils.h>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace ProjectExplorer {
class Abi;
class Project;
}

namespace Android {

namespace Internal {
class AndroidSdkManager;
class AndroidPluginPrivate;
}

class AndroidDeviceInfo
{
public:
    QString serialNumber;
    QString avdname;
    QStringList cpuAbi;
    int sdk = -1;
    enum State { OkState, UnAuthorizedState, OfflineState };
    State state = OfflineState;
    bool unauthorized = false;
    enum AndroidDeviceType { Hardware, Emulator };
    AndroidDeviceType type = Emulator;

    static QStringList adbSelector(const QString &serialNumber);

    bool isValid() const { return !serialNumber.isEmpty() || !avdname.isEmpty(); }
    bool operator<(const AndroidDeviceInfo &other) const;
};
using AndroidDeviceInfoList = QList<AndroidDeviceInfo>;

class CreateAvdInfo
{
public:
    bool isValid() const { return sdkPlatform && sdkPlatform->isValid() && !name.isEmpty(); }
    const SdkPlatform *sdkPlatform = nullptr;
    QString name;
    QString abi;
    int sdcardSize = 0;
    QString error; // only used in the return value of createAVD
};

class ANDROID_EXPORT AndroidConfig
{
public:
    void load(const QSettings &settings);
    void save(QSettings &settings) const;

    static QStringList apiLevelNamesFor(const SdkPlatformList &platforms);
    static QString apiLevelNameFor(const SdkPlatform *platform);

    Utils::FilePath sdkLocation() const;
    void setSdkLocation(const Utils::FilePath &sdkLocation);
    QVersionNumber sdkToolsVersion() const;
    QVersionNumber buildToolsVersion() const;
    QStringList sdkManagerToolArgs() const;
    void setSdkManagerToolArgs(const QStringList &args);

    Utils::FilePath ndkLocation() const;
    Utils::FilePath gdbServer(const QString &androidAbi) const;
    QVersionNumber ndkVersion() const;
    void setNdkLocation(const Utils::FilePath &ndkLocation);

    Utils::FilePath openJDKLocation() const;
    void setOpenJDKLocation(const Utils::FilePath &openJDKLocation);

    Utils::FilePath keystoreLocation() const;
    void setKeystoreLocation(const Utils::FilePath &keystoreLocation);

    QString toolchainHost() const;

    unsigned partitionSize() const;
    void setPartitionSize(unsigned partitionSize);

    bool automaticKitCreation() const;
    void setAutomaticKitCreation(bool b);

    Utils::FilePath qtLiveApkPath() const;

    Utils::FilePath adbToolPath() const;
    Utils::FilePath androidToolPath() const;
    Utils::FilePath emulatorToolPath() const;
    Utils::FilePath sdkManagerToolPath() const;
    Utils::FilePath avdManagerToolPath() const;
    Utils::FilePath aaptToolPath() const;

    Utils::FilePath toolchainPath() const;
    Utils::FilePath clangPath() const;

    Utils::FilePath gdbPath(const ProjectExplorer::Abi &abi) const;
    Utils::FilePath makePath() const;

    Utils::FilePath keytoolPath() const;

    QVector<AndroidDeviceInfo> connectedDevices(QString *error = nullptr) const;
    static QVector<AndroidDeviceInfo> connectedDevices(const Utils::FilePath &adbToolPath, QString *error = nullptr);

    QString bestNdkPlatformMatch(int target) const;

    static ProjectExplorer::Abi abiForToolChainPrefix(const QString &toolchainPrefix);
    static QLatin1String toolchainPrefix(const ProjectExplorer::Abi &abi);
    static QLatin1String toolsPrefix(const ProjectExplorer::Abi &abi);
    static QLatin1String displayName(const ProjectExplorer::Abi &abi);

    QString getProductModel(const QString &device) const;
    enum class OpenGl { Enabled, Disabled, Unknown };
    OpenGl getOpenGLEnabled(const QString &emulator) const;
    bool isConnected(const QString &serialNumber) const;

    bool useNativeUiTools() const;

private:
    static QString getDeviceProperty(const Utils::FilePath &adbToolPath,
                                     const QString &device, const QString &property);

    Utils::FilePath openJDKBinPath() const;
    int getSDKVersion(const QString &device) const;
    static int getSDKVersion(const Utils::FilePath &adbToolPath, const QString &device);
    QStringList getAbis(const QString &device) const;
    static QStringList getAbis(const Utils::FilePath &adbToolPath, const QString &device);
    static bool isBootToQt(const Utils::FilePath &adbToolPath, const QString &device);
    bool isBootToQt(const QString &device) const;
    static QString getAvdName(const QString &serialnumber);

    void updateNdkInformation() const;

    Utils::FilePath m_sdkLocation;
    QStringList m_sdkManagerToolArgs;
    Utils::FilePath m_ndkLocation;
    Utils::FilePath m_openJDKLocation;
    Utils::FilePath m_keystoreLocation;
    unsigned m_partitionSize = 1024;
    bool m_automaticKitCreation = true;

    //caches
    mutable bool m_NdkInformationUpToDate = false;
    mutable QString m_toolchainHost;
    mutable QVector<int> m_availableNdkPlatforms;

    mutable QHash<QString, QString> m_serialNumberToDeviceName;
};

class ANDROID_EXPORT AndroidConfigurations : public QObject
{
    Q_OBJECT

public:
    static const AndroidConfig &currentConfig();
    static Internal::AndroidSdkManager *sdkManager();
    static void setConfig(const AndroidConfig &config);
    static AndroidConfigurations *instance();

    static AndroidDeviceInfo showDeviceDialog(ProjectExplorer::Project *project, int apiLevel, const QStringList &abis);
    static void setDefaultDevice(ProjectExplorer::Project *project, const QString &abi, const QString &serialNumber); // serial number or avd name
    static QString defaultDevice(ProjectExplorer::Project *project, const QString &abi); // serial number or avd name
    static void clearDefaultDevices(ProjectExplorer::Project *project);
    static void registerNewToolChains();
    static void removeOldToolChains();
    static void updateAutomaticKitList();
    static bool force32bitEmulator();
    static QProcessEnvironment toolsEnvironment(const AndroidConfig &config);

signals:
    void updated();

private:
    friend class Android::Internal::AndroidPluginPrivate;
    AndroidConfigurations();
    ~AndroidConfigurations() override;
    void load();
    void save();

    static void updateAndroidDevice();
    static AndroidConfigurations *m_instance;
    AndroidConfig m_config;
    std::unique_ptr<Internal::AndroidSdkManager> m_sdkManager;

    QMap<ProjectExplorer::Project *, QMap<QString, QString> > m_defaultDeviceForAbi;
    bool m_force32bit;
};

QDebug &operator<<(QDebug &stream, const AndroidDeviceInfo &device);

} // namespace Android

Q_DECLARE_METATYPE(Android::AndroidDeviceInfo)
