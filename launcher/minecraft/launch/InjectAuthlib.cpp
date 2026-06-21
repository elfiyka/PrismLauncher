#include "InjectAuthlib.h"
#include <launch/LaunchTask.h>
#include <minecraft/MinecraftInstance.h>
#include <FileSystem.h>
#include <Application.h>
#include <Json.h>
#include <net/NetRequest.h>
#include <net/HttpMetaCache.h>
#include <utility>

InjectAuthlib::InjectAuthlib(LaunchTask *parent, AuthlibInjectorPtr* injector) : LaunchStep(parent)
{
    m_injector = injector;
}

void InjectAuthlib::executeTask()
{
    if (m_aborted)
    {
        emitFailed(tr("Task aborted."));
        return;
    }

    if (!m_offlineMode)
    {
        auto latestVersionInfo = QString("https://authlib-injector.yushi.moe/artifact/latest.json");
        auto netJob = new NetJob("Injector versions info download", APPLICATION->network());
        
        MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry("injectors", "version.json");
        entry->setStale(true);
        auto task = Net::NetRequest::makeCached(QUrl(latestVersionInfo), entry);;
        netJob->addNetAction(task);

        jobPtr.reset(std::move(netJob));
        
        QObject::connect(netJob, &NetJob::succeeded, this, &InjectAuthlib::onVersionDownloadSucceeded);
        QObject::connect(netJob, &NetJob::failed, this, &InjectAuthlib::onDownloadFailed);
        jobPtr->start();
    }
    else
    {
        onVersionDownloadSucceeded();
    }
}

void InjectAuthlib::onVersionDownloadSucceeded()
{
    QByteArray data;
    try
    {
        data = FS::read(QDir("injectors").absoluteFilePath("version.json"));
    }
    catch (const Exception &e)
    {
        qCritical() << "Injector Download Failed: index file not readable";
        jobPtr.reset();
        emitFailed("Error while reading JSON response from InjectorEndpoint");
        return;
    }

    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError)
    {
        qCritical() << "Error parsing JSON: " << parse_error.errorString();
        jobPtr.reset();
        emitFailed("Error while parsing JSON response from InjectorEndpoint");
        return;
    }

    if (!doc.isObject())
    {
        jobPtr.reset();
        emitFailed("JSON root is not an object");
        return;
    }

    QString downloadUrl;
    try
    {
        downloadUrl = Json::requireString(doc.object(), "download_url");
    }
    catch (const JSONValidationError &e)
    {
        qCritical() << "JSON validation error: " << e.cause();
        jobPtr.reset();
        emitFailed("Error while parsing download_url");
        return;
    }

    QFileInfo fi(downloadUrl);
    m_versionName = fi.fileName();

    qDebug() << "Authlib injector version:" << m_versionName;
    if (!m_offlineMode)
    {
        auto netJob = new NetJob("Injector download", APPLICATION->network());
        MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry("injectors", m_versionName);
        entry->setStale(true);
        auto task = Net::NetRequest::makeCached(QUrl(downloadUrl), entry);;
        netJob->addNetAction(task);

        jobPtr.reset(std::move(netJob));
        
        QObject::connect(netJob, &NetJob::succeeded, this, &InjectAuthlib::onDownloadSucceeded);
        QObject::connect(netJob, &NetJob::failed, this, &InjectAuthlib::onDownloadFailed);
        jobPtr->start();
    }
    else
    {
        onDownloadSucceeded();
    }
}

void InjectAuthlib::onDownloadSucceeded()
{
    QString jarPath = QDir("injectors").absoluteFilePath(m_versionName);
    QString injector = QString("-javaagent:%1=%2").arg(jarPath).arg(m_authServer);

    qDebug() << "Injecting " << injector;
    
    auto inj = std::make_shared<AuthlibInjector>(injector);
    *m_injector = inj;

    jobPtr.reset();
    emitSucceeded();
}

void InjectAuthlib::onDownloadFailed(QString reason)
{
    jobPtr.reset();
    emitFailed(reason);
}

void InjectAuthlib::proceed()
{

}

bool InjectAuthlib::canAbort() const
{
    if (jobPtr)
    {
        return jobPtr->canAbort();
    }
    return true;
}

bool InjectAuthlib::abort()
{
    m_aborted = true;
    if (jobPtr)
    {
        if (jobPtr->canAbort())
        {
            return jobPtr->abort();
        }
    }
    return true;
}
