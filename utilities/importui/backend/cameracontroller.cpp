/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2004-09-17
 * Description : digital camera controller
 *
 * Copyright (C) 2004-2005 by Renchi Raju <renchi dot raju at gmail dot com>
 * Copyright (C) 2006-2015 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * Copyright (C) 2006-2012 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include "cameracontroller.h"

// C ANSI includes

extern "C"
{
#include <unistd.h>
}

// C++ includes

#include <typeinfo>
#include <cstdio>

// Qt includes

#include <QMutex>
#include <QWaitCondition>
#include <QVariant>
#include <QImage>
#include <QFile>
#include <QRegExp>
#include <QFileInfo>
#include <QPointer>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QUrl>
#include <QDir>
#include <QMessageBox>

// KDE includes

#include <klocalizedstring.h>
#include <kprocess.h>
#include <kmacroexpander.h>

#include <kio/renamedialog.h>
#include <kio/global.h>

// Local includes

#include "digikam_debug.h"
#include "digikam_config.h"
#include "template.h"
#include "templatemanager.h"
#include "thumbnailsize.h"
#include "imagewindow.h"
#include "gpcamera.h"
#include "umscamera.h"
#include "jpegutils.h"

namespace Digikam
{

class CameraCommand
{
public:

    enum Action
    {
        cam_none = 0,
        cam_connect,
        cam_cancel,
        cam_cameraInformation,
        cam_listfolders,
        cam_listfiles,
        cam_download,
        cam_upload,
        cam_delete,
        cam_lock,
        cam_thumbsinfo,
        cam_metadata,
        cam_open,
        cam_freeSpace,
        cam_preview,
        cam_capture
    };

    Action                  action;
    QMap<QString, QVariant> map;
};

class CameraController::Private
{
public:

    Private() :
        close(false),
        overwriteAll(false),
        skipAll(false),
        canceled(false),
        running(false),
        folderList(0),
        parent(0),
        timer(0),
        camera(0)
    {
    }

    bool                  close;
    bool                  overwriteAll;
    bool                  skipAll;
    bool                  canceled;
    bool                  running;

    QStringList           folderList;

    QWidget*              parent;

    QTimer*               timer;

    DKCamera*             camera;

    QMutex                mutex;
    QWaitCondition        condVar;

    QList<CameraCommand*> commands;
};

CameraController::CameraController(QWidget* const parent,
                                   const QString& title, const QString& model,
                                   const QString& port, const QString& path)
    : QThread(parent), d(new Private)
{
    d->parent = parent;

    // URL parsing (c) Stephan Kulow
    if (path.startsWith(QLatin1String("camera:/")))
    {
        QUrl url(path);
        qCDebug(LOG_IMPORTUI) << "path " << path << " " << url <<  " " << url.host();
        QString xport = url.host();

        if (xport.startsWith(QLatin1String("usb:")))
        {
            qCDebug(LOG_IMPORTUI) << "xport " << xport;
            QRegExp x = QRegExp("(usb:[0-9,]*)");

            if (x.indexIn(xport) != -1)
            {
                QString usbport = x.cap(1);
                qCDebug(LOG_IMPORTUI) << "USB " << xport << " " << usbport;
                // if ((xport == usbport) || ((count == 1) && (xport == "usb:"))) {
                //   model = xmodel;
                d->camera = new GPCamera(title, url.userName(), "usb:", "/");
                // }
            }
        }
    }

    if (!d->camera)
    {
        if (model.toLower() == "directory browse")
        {
            d->camera = new UMSCamera(title, model, port, path);
        }
        else
        {
            d->camera = new GPCamera(title, model, port, path);
        }
    }

    connect(d->camera, SIGNAL(signalFolderList(QStringList)), this, SIGNAL(signalFolderList(QStringList)));

    // setup inter-thread signals

    qRegisterMetaType<CamItemInfo>("CamItemInfo");
    qRegisterMetaType<CamItemInfoList>("CamItemInfoList");

    connect(this, SIGNAL(signalInternalCheckRename(QString,QString,QString,QString,QString)),
            this, SLOT(slotCheckRename(QString,QString,QString,QString,QString)),
            Qt::BlockingQueuedConnection);

    connect(this, SIGNAL(signalInternalDownloadFailed(QString,QString)),
            this, SLOT(slotDownloadFailed(QString,QString)),
            Qt::BlockingQueuedConnection);

    connect(this, SIGNAL(signalInternalUploadFailed(QString,QString,QString)),
            this, SLOT(slotUploadFailed(QString,QString,QString)),
            Qt::BlockingQueuedConnection);

    connect(this, SIGNAL(signalInternalDeleteFailed(QString,QString)),
            this, SLOT(slotDeleteFailed(QString,QString)),
            Qt::BlockingQueuedConnection);

    connect(this, SIGNAL(signalInternalLockFailed(QString,QString)),
            this, SLOT(slotLockFailed(QString,QString)),
            Qt::BlockingQueuedConnection);

    d->running = true;
}

CameraController::~CameraController()
{
    // clear commands, stop camera
    slotCancel();

    // stop thread
    {
        QMutexLocker lock(&d->mutex);
        d->running = false;
        d->condVar.wakeAll();
    }
    wait();

    delete d->camera;
    delete d;
}

bool CameraController::cameraThumbnailSupport() const
{
    if (!d->camera)
    {
        return false;
    }

    return d->camera->thumbnailSupport();
}

bool CameraController::cameraDeleteSupport() const
{
    if (!d->camera)
    {
        return false;
    }

    return d->camera->deleteSupport();
}

bool CameraController::cameraUploadSupport() const
{
    if (!d->camera)
    {
        return false;
    }

    return d->camera->uploadSupport();
}

bool CameraController::cameraMkDirSupport() const
{
    if (!d->camera)
    {
        return false;
    }

    return d->camera->mkDirSupport();
}

bool CameraController::cameraDelDirSupport() const
{
    if (!d->camera)
    {
        return false;
    }

    return d->camera->delDirSupport();
}

bool CameraController::cameraCaptureImageSupport() const
{
    if (!d->camera)
    {
        return false;
    }

    return d->camera->captureImageSupport();
}

bool CameraController::cameraCaptureImagePreviewSupport() const
{
    if (!d->camera)
    {
        return false;
    }

    return d->camera->captureImageSupport() && d->camera->captureImagePreviewSupport();
}

QString CameraController::cameraPath() const
{
    if (!d->camera)
    {
        return QString();
    }

    return d->camera->path();
}

QString CameraController::cameraTitle() const
{
    if (!d->camera)
    {
        return QString();
    }

    return d->camera->title();
}

DKCamera::CameraDriverType CameraController::cameraDriverType() const
{
    if (!d->camera)
    {
        return DKCamera::UMSDriver;
    }

    return d->camera->cameraDriverType();
}

QByteArray CameraController::cameraMD5ID() const
{
    if (!d->camera)
    {
        return QByteArray();
    }

    return d->camera->cameraMD5ID();
}

QIcon CameraController::mimeTypeThumbnail(const QString& itemName) const
{
    if (!d->camera)
    {
        return QPixmap();
    }

    QFileInfo fi(itemName);
    QString mime = d->camera->mimeType(fi.suffix().toLower());

    if (mime.startsWith(QLatin1String("image/x-raw")))
    {
        return QIcon::fromTheme("kdcraw");
    }
    else if (mime.startsWith(QLatin1String("image/")))
    {
        return QIcon::fromTheme("image-x-generic");
    }
    else if (mime.startsWith(QLatin1String("video/")))
    {
        return QIcon::fromTheme("video-x-generic");
    }
    else if (mime.startsWith(QLatin1String("audio/")))
    {
        return QIcon::fromTheme("audio-x-generic");
    }

    return QIcon::fromTheme("unknown");
}

void CameraController::slotCancel()
{
    d->canceled = true;
    d->camera->cancel();
    QMutexLocker lock(&d->mutex);
    d->commands.clear();
}

void CameraController::run()
{
    while (d->running)
    {
        CameraCommand* command = 0;

        {
            QMutexLocker lock(&d->mutex);

            if (!d->commands.isEmpty())
            {
                command = d->commands.takeFirst();
            }
            else
            {
                emit signalBusy(false);
                d->condVar.wait(&d->mutex);
                continue;
            }
        }

        if (command)
        {
            // Special case with thumbs handling. We don't need progress bar in gui.
            if (command->action != CameraCommand::cam_thumbsinfo)
            {
                emit signalBusy(true);
            }

            executeCommand(command);
            delete command;
        }
    }

    emit signalBusy(false);
}

void CameraController::executeCommand(CameraCommand* const cmd)
{
    if (!cmd)
    {
        return;
    }

    switch (cmd->action)
    {
        case (CameraCommand::cam_connect):
        {
            sendLogMsg(i18n("Connecting to camera..."));

            bool result = d->camera->doConnect();

            emit signalConnected(result);

            if (result)
            {
                d->camera->printSupportedFeatures();
                sendLogMsg(i18n("Connection established."));
            }
            else
            {
                sendLogMsg(i18n("Connection failed."));
            }

            break;
        }

        case (CameraCommand::cam_cameraInformation):
        {
            QString summary, manual, about;

            d->camera->cameraSummary(summary);
            d->camera->cameraManual(manual);
            d->camera->cameraAbout(about);

            emit signalCameraInformation(summary, manual, about);
            break;
        }

        case (CameraCommand::cam_freeSpace):
        {
            unsigned long kBSize  = 0;
            unsigned long kBAvail = 0;

            if (!d->camera->getFreeSpace(kBSize, kBAvail))
            {
                sendLogMsg(i18n("Failed to get free space from camera"), DHistoryView::ErrorEntry);
            }

            emit signalFreeSpace(kBSize, kBAvail);
            break;
        }

        case (CameraCommand::cam_preview):
        {
            QImage preview;

            if (!d->camera->getPreview(preview))
            {
                sendLogMsg(i18n("Failed to get preview from camera"), DHistoryView::ErrorEntry);
            }

            emit signalPreview(preview);
            break;
        }

        case (CameraCommand::cam_capture):
        {
            CamItemInfo itemInfo;

            if (!d->camera->capture(itemInfo))
            {
                sendLogMsg(i18n("Failed to process capture from camera"), DHistoryView::ErrorEntry);
            }

            emit signalUploaded(itemInfo);
            break;
        }

        case (CameraCommand::cam_listfolders):
        {
            QString folder = cmd->map["folder"].toString();

            if (!d->camera->getFolders(folder))
            {
                sendLogMsg(xi18n("Failed to list folder <b>%1</b>", folder), DHistoryView::ErrorEntry);
            }

            break;
        }

        case (CameraCommand::cam_listfiles):
        {
            QString folder   = cmd->map["folder"].toString();
            bool useMetadata = cmd->map["useMetadata"].toBool();

            CamItemInfoList itemsList;

            if (!d->camera->getItemsInfoList(folder, useMetadata, itemsList))
            {
                sendLogMsg(xi18n("Failed to list files in <b>%1</b>", folder), DHistoryView::ErrorEntry);
            }

            // TODO would it be okay to pass this to the ImportImageModel and let it filter it for us?
            for (CamItemInfoList::iterator it = itemsList.begin() ; it != itemsList.end() ; ) 
            {
                CamItemInfo &info = (*it);

                if (info.mime.isEmpty())
                {
                    it = itemsList.erase(it);
                    continue;
                }

                ++it;
            }

            emit signalFileList(itemsList);

            break;
        }

        case (CameraCommand::cam_thumbsinfo):
        {
            QList<QVariant> list = cmd->map["list"].toList();
            int thumbSize        = cmd->map["thumbSize"].toInt();

            for (QList<QVariant>::const_iterator it = list.constBegin(); it != list.constEnd(); ++it)
            {
                if (d->canceled)
                {
                    break;
                }

                QString folder = (*it).toStringList().at(0);
                QString file   = (*it).toStringList().at(1);

                CamItemInfo info;
                info.folder = folder;
                info.name = file;
                QImage thumbnail;

                if (d->camera->getThumbnail(folder, file, thumbnail))
                {
                    thumbnail = thumbnail.scaled(thumbSize, thumbSize, Qt::KeepAspectRatio);
                    emit signalThumbInfo(folder, file, info, thumbnail);
                }
                else
                {
                    sendLogMsg(xi18n("Failed to get thumbnail for <b>%1</b>", file), DHistoryView::ErrorEntry, folder, file);
                    emit signalThumbInfoFailed(folder, file, info);
                }
            }

            break;
        }

        case (CameraCommand::cam_metadata):
        {
            QString folder = cmd->map["folder"].toString();
            QString file   = cmd->map["file"].toString();

            DMetadata meta;

            if (!d->camera->getMetadata(folder, file, meta))
                sendLogMsg(xi18n("Failed to get Metadata for <b>%1</b>", file), DHistoryView::ErrorEntry, folder, file);

            emit signalMetadata(folder, file, meta);

            break;
        }

        case (CameraCommand::cam_download):
        {
            QString   folder         = cmd->map["folder"].toString();
            QString   file           = cmd->map["file"].toString();
            QString   dest           = cmd->map["dest"].toString();
            bool      fixDateTime    = cmd->map["fixDateTime"].toBool();
            QDateTime newDateTime    = cmd->map["newDateTime"].toDateTime();
            QString   templateTitle  = cmd->map["template"].toString();
            bool      convertJpeg    = cmd->map["convertJpeg"].toBool();
            QString   losslessFormat = cmd->map["losslessFormat"].toString();
            QString   script         = cmd->map["script"].toString();
            int       pickLabel      = cmd->map["pickLabel"].toInt();
            int       colorLabel     = cmd->map["colorLabel"].toInt();
            int       rating         = cmd->map["rating"].toInt();

            // download to a temp file

            emit signalDownloaded(folder, file, CamItemInfo::DownloadStarted);

            // TODO clean-up up and generalize temporary file creation
            QUrl tempURL = QUrl::fromLocalFile(dest);
            tempURL = KIO::upUrl(tempURL);
            tempURL = tempURL.adjusted(QUrl::StripTrailingSlash);
            tempURL.setPath(tempURL.path() + '/' + (QString(".digikam-camera-tmp1-%1").arg(getpid()).append(file)));
            qCDebug(LOG_IMPORTUI) << "Downloading: " << file << " using (" << tempURL << ")";
            QString temp = tempURL.toLocalFile();

            bool result  = d->camera->downloadItem(folder, file, tempURL.toLocalFile());

            if (!result)
            {
                unlink(QFile::encodeName(tempURL.toLocalFile()).constData());
                sendLogMsg(xi18n("Failed to download <b>%1</b>", file), DHistoryView::ErrorEntry, folder, file);
                emit signalDownloaded(folder, file, CamItemInfo::DownloadFailed);
                break;
            }
            else if (JPEGUtils::isJpegImage(tempURL.toLocalFile()))
            {
                // Possible modification operations. Only apply it to JPEG for the moment.

                if (!templateTitle.isNull() || fixDateTime)
                {
                    qCDebug(LOG_IMPORTUI) << "Set metadata from: " << file << " using (" << tempURL << ")";
                    DMetadata metadata(tempURL.toLocalFile());

                    if (fixDateTime)
                    {
                        metadata.setImageDateTime(newDateTime, true);
                    }

                    //TODO: Set image tags using DMetadata.
                    metadata.setImagePickLabel(pickLabel);
                    metadata.setImageColorLabel(colorLabel);
                    metadata.setImageRating(rating);

                    TemplateManager* tm = TemplateManager::defaultManager();

                    if (tm && !templateTitle.isEmpty())
                    {
                        qCDebug(LOG_IMPORTUI) << "Metadata template title : " << templateTitle;

                        if (templateTitle == Template::removeTemplateTitle())
                        {
                            metadata.removeMetadataTemplate();
                        }
                        else if (templateTitle.isEmpty())
                        {
                            // Nothing to do.
                        }
                        else
                        {
                            metadata.removeMetadataTemplate();
                            metadata.setMetadataTemplate(tm->findByTitle(templateTitle));
                        }
                    }

                    metadata.applyChanges();
                }

                // Convert JPEG file to lossless format if wanted,
                // and move converted image to destination.

                if (convertJpeg)
                {
                    // TODO clean-up up and generalize temporary file creation
                    QUrl tempURL2 = QUrl::fromLocalFile(dest);
                    tempURL2 = KIO::upUrl(tempURL2);
                    tempURL2 = tempURL2.adjusted(QUrl::StripTrailingSlash);
                    tempURL2.setPath(tempURL2.path() + '/' + (QString(".digikam-camera-tmp2-%1").arg(getpid()).append(file)));
                    temp     = tempURL2.toLocalFile();

                    // when convertnig a file, we need to set the new format extension..
                    // would another place be better for this?
                    dest = dest.left(dest.lastIndexOf('.')) + losslessFormat;
                    qCDebug(LOG_IMPORTUI) << "Convert to LossLess: " << file << " using (" << tempURL << ")  destination: " << dest;

                    if (!JPEGUtils::jpegConvert(tempURL.toLocalFile(), tempURL2.toLocalFile(), file, losslessFormat))
                    {
                        qCDebug(LOG_IMPORTUI) << "  Convert failed?! eh";
                        // convert failed. delete the temp file
                        unlink(QFile::encodeName(tempURL.toLocalFile()).constData());
                        unlink(QFile::encodeName(tempURL2.toLocalFile()).constData());
                        sendLogMsg(xi18n("Failed to convert file <b>%1</b> to JPEG", file), DHistoryView::ErrorEntry, folder, file);
                    }
                    else
                    {
                        qCDebug(LOG_IMPORTUI) << "  Done, removing the temp file: " << tempURL;
                        // Else remove only the first temp file.
                        unlink(QFile::encodeName(tempURL.toLocalFile()).constData());
                    }
                }
            }

            // Now we need to move from temp file to destination file.
            // This possibly involves UI operation, do it from main thread
            emit signalInternalCheckRename(folder, file, dest, temp, script);
            break;
        }

        case (CameraCommand::cam_upload):
        {
            QString folder = cmd->map["destFolder"].toString();

            // We will using the same source file name to create the dest file
            // name in camera.
            QString file   = cmd->map["destFile"].toString();

            // The source file path to download in camera.
            QString src    = cmd->map["srcFilePath"].toString();

            CamItemInfo itemsInfo;

            bool result = d->camera->uploadItem(folder, file, src, itemsInfo);

            if (result)
            {
                emit signalUploaded(itemsInfo);
            }
            else
            {
                emit signalInternalUploadFailed(folder, file, src);
            }

            break;
        }

        case (CameraCommand::cam_delete):
        {
            QString folder = cmd->map["folder"].toString();
            QString file   = cmd->map["file"].toString();
            bool result    = d->camera->deleteItem(folder, file);

            if (result)
            {
                emit signalDeleted(folder, file, true);
            }
            else
            {
                emit signalInternalDeleteFailed(folder, file);
            }

            break;
        }

        case (CameraCommand::cam_lock):
        {
            QString folder = cmd->map["folder"].toString();
            QString file   = cmd->map["file"].toString();
            bool    lock   = cmd->map["lock"].toBool();
            bool result    = d->camera->setLockItem(folder, file, lock);

            if (result)
            {
                emit signalLocked(folder, file, true);
            }
            else
            {
                emit signalInternalLockFailed(folder, file);
            }

            break;
        }

        default:
        {
            qCWarning(LOG_IMPORTUI) << " unknown action specified";
            break;
        }
    }
}

void CameraController::sendLogMsg(const QString& msg, DHistoryView::EntryType type,
                                  const QString& folder, const QString& file)
{
    qCDebug(LOG_IMPORTUI) << "Log (" << file << " " << folder << ": " << msg;
    if (!d->canceled)
    {
        emit signalLogMsg(msg, type, folder, file);
    }
}

void CameraController::slotCheckRename(const QString& folder, const QString& file,
                                       const QString& destination, const QString& temp,
                                       const QString& script)
{
    // this is the direct continuation of executeCommand, case CameraCommand::cam_download
    bool skip      = false;
    bool cancel    = false;
    bool overwrite = d->overwriteAll;
    QString dest   = destination;

    // Check if dest file already exist, unless we overwrite anyway

    QFileInfo info(dest);

    if (!d->overwriteAll)
    {

        while (info.exists())
        {
            if (d->skipAll)
            {
                skip = true;
                break;
            }

            QPointer<KIO::RenameDialog> dlg = new KIO::RenameDialog(d->parent, i18nc("@title:window", "Rename File"),
                                                                    QUrl::fromLocalFile(folder + QLatin1String("/") + file),
                                                                    QUrl::fromLocalFile(dest),
                                                                    KIO::RenameDialog_Mode(KIO::M_MULTI     |
                                                                            KIO::M_OVERWRITE |
                                                                            KIO::M_SKIP));

            int result = dlg->exec();
            dest       = dlg->newDestUrl().toLocalFile();
            info       = QFileInfo(dest);

            delete dlg;

            switch (result)
            {
                case KIO::R_CANCEL:
                {
                    cancel = true;
                    break;
                }

                case KIO::R_SKIP:
                {
                    skip = true;
                    break;
                }

                case KIO::R_AUTO_SKIP:
                {
                    d->skipAll = true;
                    skip       = true;
                    break;
                }

                case KIO::R_OVERWRITE:
                {
                    overwrite = true;
                    break;
                }

                case KIO::R_OVERWRITE_ALL:
                {
                    d->overwriteAll = true;
                    overwrite       = true;
                    break;
                }

                default:
                    break;
            }

            if (cancel || skip || overwrite)
            {
                break;
            }
        }
    }

    if (cancel)
    {
        unlink(QFile::encodeName(temp).constData());
        slotCancel();
        emit signalSkipped(folder, file);
        return;
    }
    else if (skip)
    {
        unlink(QFile::encodeName(temp).constData());
        sendLogMsg(xi18n("Skipped file <b>%1</b>", file), DHistoryView::WarningEntry, folder, file);
        emit signalSkipped(folder, file);
        return;
    }

    // move the file to the destination file
    if (DMetadata::hasSidecar(temp))
    {
        qCDebug(LOG_IMPORTUI) << "File" << temp << " has a sidecar, renaming it to " << DMetadata::sidecarPath(dest);

        if (QFile::rename(DMetadata::sidecarPath(temp), DMetadata::sidecarPath(dest)) != 0)
        {
            sendLogMsg(xi18n("Failed to save sidecar file for <b>%1</b>", file), DHistoryView::ErrorEntry,  folder, file);
        }
    }

    if (QFile::rename(temp, dest) != 0)
    {
        qCDebug(LOG_IMPORTUI) << "Renaming " << temp << " to " << dest << " failed";
        // rename failed. delete the temp file
        unlink(QFile::encodeName(temp).constData());
        emit signalDownloaded(folder, file, CamItemInfo::DownloadFailed);
        sendLogMsg(xi18n("Failed to download <b>%1</b>", file), DHistoryView::ErrorEntry,  folder, file);
    }
    else
    {
        qCDebug(LOG_IMPORTUI) << "Rename done, emiting downloaded signals:" << file << " info.filename: " << info.fileName();
        // TODO why two signals??
        emit signalDownloaded(folder, file, CamItemInfo::DownloadedYes);
        emit signalDownloadComplete(folder, file, info.path(), info.fileName());

        // Run script
        if (!script.isEmpty())
        {
            qCDebug(LOG_IMPORTUI) << "Got a script, processing: " << script;
            KProcess process;

            process.setOutputChannelMode(KProcess::SeparateChannels);
            QString s;

            if (script.indexOf('%') > -1)
            {
                QHash<QString, QString> map;
                map.insert("file", dest);
                map.insert("filename", info.fileName());
                map.insert("path", info.path());
                map.insert("orgfilename", file);
                map.insert("orgpath", folder);
                s = KMacroExpander::expandMacros(script, map);
            }
            else
            {
                s = script + " \"" + dest + "\"";
            }

            process.setShellCommand(s);
            qCDebug(LOG_IMPORTUI) << "Running: " << s;
            int ret = process.execute();

            if (ret != 0)
            {
                sendLogMsg(xi18n("Failed to run script for <b>%1</b>", file), DHistoryView::ErrorEntry,  folder, file);
            }

            qCDebug(LOG_IMPORTUI) << "stdout" << process.readAllStandardOutput();
            qCDebug(LOG_IMPORTUI) << "stderr" << process.readAllStandardError();
        }
    }
}

void CameraController::slotDownloadFailed(const QString& folder, const QString& file)
{
    sendLogMsg(xi18n("Failed to download <b>%1</b>", file), DHistoryView::ErrorEntry, folder, file);

    if (!d->canceled)
    {
        if (queueIsEmpty())
        {
            QMessageBox::critical(d->parent, qApp->applicationName(), xi18n("Failed to download file <b>%1</b>.", file));
        }
        else
        {
            const QString msg = xi18n("<qt>Failed to download file <b>%1</b>. Do you want to continue?</qt>", file);
            int result        = QMessageBox::warning(d->parent, qApp->applicationName(), msg, QMessageBox::Yes | QMessageBox::Cancel);

            if (result != QMessageBox::Yes)
            {
                slotCancel();
            }
        }
    }
}

void CameraController::slotUploadFailed(const QString& folder, const QString& file, const QString& src)
{
    Q_UNUSED(folder);
    Q_UNUSED(src);

    sendLogMsg(xi18n("Failed to upload <b>%1</b>", file), DHistoryView::ErrorEntry);

    if (!d->canceled)
    {
        if (queueIsEmpty())
        {
            QMessageBox::critical(d->parent, qApp->applicationName(), xi18n("Failed to upload file <b>%1</b>.", file));
        }
        else
        {
            const QString msg = xi18n("<qt>Failed to upload file <b>%1</b>. Do you want to continue?</qt>", file);
            int result        = QMessageBox::warning(d->parent, qApp->applicationName(), msg, QMessageBox::Yes | QMessageBox::Cancel);

            if (result != QMessageBox::Yes)
            {
                slotCancel();
            }
        }
    }
}

void CameraController::slotDeleteFailed(const QString& folder, const QString& file)
{
    emit signalDeleted(folder, file, false);
    sendLogMsg(xi18n("Failed to delete <b>%1</b>", file), DHistoryView::ErrorEntry, folder, file);

    if (!d->canceled)
    {
        if (queueIsEmpty())
        {
            QMessageBox::critical(d->parent, qApp->applicationName(), xi18n("Failed to delete file <b>%1</b>.", file));
        }
        else
        {
            const QString msg = xi18n("<qt>Failed to delete file <b>%1</b>. Do you want to continue?</qt>", file);
            int result        = QMessageBox::warning(d->parent, qApp->applicationName(), msg, QMessageBox::Yes | QMessageBox::Cancel);

            if (result != QMessageBox::Yes)
            {
                slotCancel();
            }
        }
    }
}

void CameraController::slotLockFailed(const QString& folder, const QString& file)
{
    emit signalLocked(folder, file, false);
    sendLogMsg(xi18n("Failed to lock <b>%1</b>", file), DHistoryView::ErrorEntry, folder, file);

    if (!d->canceled)
    {
        if (queueIsEmpty())
        {
            QMessageBox::critical(d->parent, qApp->applicationName(), xi18n("Failed to toggle lock file <b>%1</b>.", file));
        }
        else
        {
            const QString msg = xi18n("<qt>Failed to toggle lock file <b>%1</b>. Do you want to continue?</qt>", file);
            int result        = QMessageBox::warning(d->parent, qApp->applicationName(), msg, QMessageBox::Yes | QMessageBox::Cancel);

            if (result != QMessageBox::Yes)
            {
                slotCancel();
            }
        }
    }
}

void CameraController::addCommand(CameraCommand* const cmd)
{
    QMutexLocker lock(&d->mutex);
    d->commands << cmd;
    d->condVar.wakeAll();
}

bool CameraController::queueIsEmpty() const
{
    QMutexLocker lock(&d->mutex);
    return d->commands.isEmpty();
}

void CameraController::slotConnect()
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_connect;
    addCommand(cmd);
}

void CameraController::listRootFolder(bool useMetadata)
{
    listFolders(d->camera->path());
    listFiles(d->camera->path(), useMetadata);
}

void CameraController::listFolders(const QString &folder)
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_listfolders;
    cmd->map.insert("folder", QVariant(folder));

    addCommand(cmd);
}

void CameraController::listFiles(const QString& folder, bool useMetadata)
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_listfiles;
    cmd->map.insert("folder",      QVariant(folder));
    cmd->map.insert("useMetadata", QVariant(useMetadata));
    addCommand(cmd);
}

void CameraController::getThumbsInfo(const CamItemInfoList& list, int thumbSize)
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_thumbsinfo;

    QList<QVariant> itemsList;

    foreach(CamItemInfo info, list)
    {
        itemsList.append(QStringList() << info.folder << info.name);
    }

    cmd->map.insert("list",      QVariant(itemsList));
    cmd->map.insert("thumbSize", QVariant(thumbSize));
    addCommand(cmd);
}

void CameraController::getMetadata(const QString& folder, const QString& file)
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_metadata;
    cmd->map.insert("folder", QVariant(folder));
    cmd->map.insert("file",   QVariant(file));
    addCommand(cmd);
}

void CameraController::getCameraInformation()
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_cameraInformation;
    addCommand(cmd);
}

void CameraController::getFreeSpace()
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_freeSpace;
    addCommand(cmd);
}

void CameraController::getPreview()
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_preview;
    addCommand(cmd);
}

void CameraController::capture()
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_capture;
    addCommand(cmd);
}

void CameraController::upload(const QFileInfo& srcFileInfo, const QString& destFile, const QString& destFolder)
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_upload;
    cmd->map.insert("srcFilePath", QVariant(srcFileInfo.filePath()));
    cmd->map.insert("destFile",    QVariant(destFile));
    cmd->map.insert("destFolder",  QVariant(destFolder));
    addCommand(cmd);
    qCDebug(LOG_IMPORTUI) << "Uploading '" << srcFileInfo.filePath() << "' into camera : '" << destFolder
             << "' (" << destFile << ")";
}

void CameraController::downloadPrep()
{
    d->overwriteAll  = false;
    d->skipAll       = false;
}

void CameraController::download(const DownloadSettingsList& list)
{
    foreach(const DownloadSettings& downloadSettings, list)
    {
        download(downloadSettings);
    }
}

void CameraController::download(const DownloadSettings& downloadSettings)
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_download;
    cmd->map.insert("folder",            QVariant(downloadSettings.folder));
    cmd->map.insert("file",              QVariant(downloadSettings.file));
    cmd->map.insert("dest",              QVariant(downloadSettings.dest));
    cmd->map.insert("fixDateTime",       QVariant(downloadSettings.fixDateTime));
    cmd->map.insert("newDateTime",       QVariant(downloadSettings.newDateTime));
    cmd->map.insert("template",          QVariant(downloadSettings.templateTitle));
    cmd->map.insert("convertJpeg",       QVariant(downloadSettings.convertJpeg));
    cmd->map.insert("losslessFormat",    QVariant(downloadSettings.losslessFormat));
    cmd->map.insert("script",            QVariant(downloadSettings.script));
    cmd->map.insert("pickLabel",         QVariant(downloadSettings.pickLabel));
    cmd->map.insert("colorLabel",        QVariant(downloadSettings.colorLabel));
    cmd->map.insert("rating",            QVariant(downloadSettings.rating));
    //cmd->map.insert("tagIds",            QVariant(downloadSettings.tagIds));
    addCommand(cmd);
}

void CameraController::deleteFile(const QString& folder, const QString& file)
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_delete;
    cmd->map.insert("folder", QVariant(folder));
    cmd->map.insert("file",   QVariant(file));
    addCommand(cmd);
}

void CameraController::lockFile(const QString& folder, const QString& file, bool locked)
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_lock;
    cmd->map.insert("folder", QVariant(folder));
    cmd->map.insert("file",   QVariant(file));
    cmd->map.insert("lock",   QVariant(locked));
    addCommand(cmd);
}

void CameraController::openFile(const QString& folder, const QString& file)
{
    d->canceled        = false;
    CameraCommand* cmd = new CameraCommand;
    cmd->action        = CameraCommand::cam_open;
    cmd->map.insert("folder", QVariant(folder));
    cmd->map.insert("file",   QVariant(file));
    cmd->map.insert("dest",   QVariant(QDir::tempPath() + QChar('/') + QString(file)));
    addCommand(cmd);
}

}  // namespace Digikam
