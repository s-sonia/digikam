/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2006-01-20
 * Description : image file IO threaded interface.
 *
 * Copyright (C) 2005-2007 by Marcel Wiesweg <marcel.wiesweg@gmx.de>
 * Copyright (C) 2005-2007 by Gilles Caulier <caulier dot gilles at gmail dot com>
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


#ifndef LOAD_SAVE_TASK_H
#define LOAD_SAVE_TASK_H

// Qt includes.

#include <QList>
#include <QEvent>

// Local includes.

#include "dimg.h"
#include "dimgloaderobserver.h"
#include "loadingdescription.h"
#include "loadingcache.h"

namespace Digikam
{

class LoadSaveThread;

class LoadSaveTask
{
public:

    LoadSaveTask(LoadSaveThread* thread)
        : m_thread(thread)
        {};
    virtual ~LoadSaveTask() {};

    virtual void execute() = 0;

    enum TaskType
    {
        TaskTypeLoading,
        TaskTypeSaving
    };

    virtual TaskType type() = 0;

protected:

    LoadSaveThread *m_thread;
};

//---------------------------------------------------------------------------------------------------

class LoadingTask : public LoadSaveTask, public DImgLoaderObserver
{
public:

    enum LoadingTaskStatus
    {
        LoadingTaskStatusLoading,
        LoadingTaskStatusPreloading,
        LoadingTaskStatusStopping
    };

    LoadingTask(LoadSaveThread* thread, LoadingDescription description,
                LoadingTaskStatus loadingTaskStatus = LoadingTaskStatusLoading)
        : LoadSaveTask(thread), m_loadingDescription(description), m_loadingTaskStatus(loadingTaskStatus)
        {}

    // LoadSaveTask

    virtual void execute();
    virtual TaskType type();

    // DImgLoaderObserver

    virtual void progressInfo(const DImg *, float progress);
    virtual bool continueQuery(const DImg *);
    virtual bool isShuttingDown();

    virtual void setStatus(LoadingTaskStatus status);

    LoadingTaskStatus status() const
    {
        return m_loadingTaskStatus;
    }

    QString filePath() const
    {
        return m_loadingDescription.filePath;
    }

    LoadingDescription loadingDescription() const
    {
        return m_loadingDescription;
    }

protected:

    LoadingDescription m_loadingDescription;
    LoadingTaskStatus m_loadingTaskStatus;
};

//---------------------------------------------------------------------------------------------------

class SharedLoadingTask : public LoadingTask, public LoadingProcess, public LoadingProcessListener
{
public:

    SharedLoadingTask(LoadSaveThread* thread, LoadingDescription description,
                      LoadSaveThread::AccessMode mode = LoadSaveThread::AccessModeReadWrite,
                      LoadingTaskStatus loadingTaskStatus = LoadingTaskStatusLoading)
        : LoadingTask(thread, description, loadingTaskStatus),
          m_accessMode(mode), m_completed(false), m_usedProcess(0)
        {}

    virtual void execute();
    virtual void progressInfo(const DImg *, float progress);
    virtual bool continueQuery(const DImg *);
    virtual void setStatus(LoadingTaskStatus status);

    // LoadingProcess

    virtual bool completed();
    virtual QString filePath();
    virtual QString cacheKey();
    virtual void addListener(LoadingProcessListener *listener);
    virtual void removeListener(LoadingProcessListener *listener);
    virtual void notifyNewLoadingProcess(LoadingProcess *process, LoadingDescription description);

    // LoadingProcessListener

    virtual bool querySendNotifyEvent();
    virtual LoadSaveNotifier *loadSaveNotifier();
    virtual LoadSaveThread::AccessMode accessMode();

protected:

    LoadSaveThread::AccessMode m_accessMode;
    bool m_completed;
    LoadingProcess *m_usedProcess;
    QList<LoadingProcessListener*> m_listeners;
};

//---------------------------------------------------------------------------------------------------

class SavingTask : public LoadSaveTask, public DImgLoaderObserver
{
public:

    enum SavingTaskStatus
    {
        SavingTaskStatusSaving,
        SavingTaskStatusStopping
    };

    SavingTask(LoadSaveThread* thread, DImg &img, const QString &filePath, const QString &format)
        : LoadSaveTask(thread), m_img(img), m_filePath(filePath), m_format(format)
        {};

    virtual void execute();
    virtual TaskType type();

    virtual void progressInfo(const DImg *, float progress);
    virtual bool continueQuery(const DImg *);

    virtual void setStatus(SavingTaskStatus status);

    SavingTaskStatus status() const
    {
        return m_savingTaskStatus;
    }

    QString filePath() const
    {
        return m_filePath;
    }

private:

    DImg        m_img;
    QString     m_filePath;
    QString     m_format;
    SavingTaskStatus m_savingTaskStatus;
};

}   // namespace Digikam

#endif // LOAD_SAVE_TASK_H
