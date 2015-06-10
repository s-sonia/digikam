#ifndef DBJOB_H
#define DBJOB_H

#include "KDCRAW/RActionJob"
#include "dbjobinfo.h"
#include "dbjobsthread.h"
#include "duplicatesprogressobserver.h"

using namespace KDcrawIface;

namespace Digikam {

class DuplicatesProgressObserver;

class DBJob : public RActionJob
{
    Q_OBJECT

public:

    DBJob();
    ~DBJob();

Q_SIGNALS:

    void data(const QByteArray &ba);
    void done();
};

// ----------------------------------------------

class AlbumsJob : public DBJob
{
    Q_OBJECT

public:

    AlbumsJob(AlbumsDBJobInfo *jobInfo);
    ~AlbumsJob();

protected:

    void run();

private:

    AlbumsDBJobInfo *m_jobInfo;
};

// ----------------------------------------------

class DatesJob : public DBJob
{
    Q_OBJECT

public:

    DatesJob(DatesDBJobInfo *jobInfo);
    ~DatesJob();

protected:

    void run();

private:

    DatesDBJobInfo *m_jobInfo;
};

// ----------------------------------------------

class GPSJob : public DBJob
{
    Q_OBJECT

public:

    GPSJob(GPSDBJobInfo *jobInfo);
    ~GPSJob();

protected:

    void run();

private:

    GPSDBJobInfo *m_jobInfo;
};

// ----------------------------------------------

class TagsJob : public DBJob
{
    Q_OBJECT

public:

    TagsJob(TagsDBJobInfo *jobInfo);
    ~TagsJob();

protected:

    void run();

private:

    TagsDBJobInfo *m_jobInfo;
};

// ----------------------------------------------

class SearchesJob : public DBJob
{
    Q_OBJECT

public:

    SearchesJob(SearchesDBJobInfo *jobInfo);
    ~SearchesJob();

Q_SIGNALS:

    void processedSize(int);
    void totalSize(int);

protected:

    void run();

private:

    SearchesDBJobInfo *m_jobInfo;
};


} // namespace Digikam

#endif // DBJOB_H
