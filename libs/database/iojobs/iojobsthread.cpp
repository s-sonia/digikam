/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2015-06-15
 * Description : IO Jobs thread for file system jobs
 *
 * Copyright (C) 2015 by Mohamed Anwer <m dot anwer at gmx dot com>
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

#include "iojobsthread.h"

#include "iojob.h"
#include "imageinfo.h"
#include "dnotificationwrapper.h"
#include "digikam_debug.h"
#include "digikamapp.h"

namespace Digikam
{

IOJobsThread::IOJobsThread(QObject *const parent)
    : RActionThreadBase(parent)
{
}

void IOJobsThread::copy(const QList<QUrl> &srcFiles, const QUrl destAlbum)
{
    RJobCollection collection;

    foreach (const QUrl &url, srcFiles)
    {
        CopyJob *j = new CopyJob(url, destAlbum, false);

        connect(j, SIGNAL(error(QString)),
                this, SIGNAL(error(QString)));

        connect(j, SIGNAL(signalDone()),
                this, SLOT(oneJobFinished()));

        collection.insert(j, 0);
    }

    appendJobs(collection);
}

void IOJobsThread::move(const QList<QUrl> &srcFiles, const QUrl destAlbum)
{
    RJobCollection collection;

    foreach (const QUrl &url, srcFiles)
    {
        CopyJob *j = new CopyJob(url, destAlbum, true);

        connect(j, SIGNAL(error(QString)),
                this, SIGNAL(error(QString)));

        connect(j, SIGNAL(signalDone()),
                this, SLOT(oneJobFinished()));

        collection.insert(j, 0);
    }

    appendJobs(collection);
}

void IOJobsThread::del(const QList<QUrl> &srcsToDelete, bool useTrash)
{
    RJobCollection collection;

    foreach (const QUrl &url, srcsToDelete)
    {
        DeleteJob *j = new DeleteJob(url, useTrash);

        connect(j, SIGNAL(error(QString)),
                this, SIGNAL(error(QString)));

        connect(j, SIGNAL(signalDone()),
                this, SLOT(oneJobFinished()));

        collection.insert(j, 0);
    }

    appendJobs(collection);
}

void IOJobsThread::renameFile(const QUrl &srcToRename, const QString &newName)
{
    RJobCollection collection;
    RenameFileJob *j = new RenameFileJob(srcToRename, newName);

    connect(j, SIGNAL(error(QString)),
            this, SIGNAL(error(QString)));

    // Connecting directly to signal finished
    // because it's only one job
    connect(j, SIGNAL(signalDone()),
            this, SIGNAL(finished()));

    connect(j, SIGNAL(signalRenamed(QUrl,QUrl)),
            this, SIGNAL(renamed(QUrl,QUrl)));

    collection.insert(j, 0);
    appendJobs(collection);
}

void IOJobsThread::oneJobFinished()
{
    if(isEmpty())
        emit finished();
}

} // namespace Digikam