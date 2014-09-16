
#ifndef JOBCOMPOSER_H
#define JOBCOMPOSER_H

#include <KDebug>
#include <KJob>
#include <KCompositeJob>
#include <QString>
#include <QQueue>


class JobHandler : public QObject {
    Q_OBJECT
public Q_SLOTS:
    void onResult(KJob*);
};

class AsyncTask : public QObject {
    Q_OBJECT
public:
    void install(KJob *job, const std::function<void()> &handler);
    void install(KJob *job, const std::function<void(KJob*)> &handler);
    void install(KJob *job, const char *signal, const std::function<void(KJob*)> &handler);

private Q_SLOTS:
    void onResult(KJob*);

private:
    std::function<void()> mHandler;
    std::function<void(KJob*)> mHandlerWithJob;
};

template<typename T, typename Arg1>
AsyncTask* make_async(const Arg1& arg1, const std::function<void(KJob*)> &handler)
{
    auto task = new AsyncTask;
    task->install(new T(arg1), handler);
    return task;
}

template<typename T>
AsyncTask* make_async(T *job, const std::function<void(KJob*)> &handler)
{
    auto task = new AsyncTask;
    task->install(job, handler);
    return task;
}

class ResultCollector : public QObject {
    Q_OBJECT
public Q_SLOTS:
    void onResult(const QString &result) {
        kDebug() << result;
    }
};

class JobComposer : public KJob
{
    Q_OBJECT
public:
    explicit JobComposer(QObject *parent = 0);
    virtual ~JobComposer();

    void start();
    void add(const std::function<void(JobComposer&, KJob*)> &jobContinuation);
    void run(KJob*);
    void run(KJob*, const std::function<bool(JobComposer&, KJob*)> &errorHandler);

private Q_SLOTS:
    void onResult(KJob*);

private:
    void processNext(KJob*);
    QQueue<std::function<void(JobComposer&, KJob*)> > mContinuationQueue;
    std::function<bool(JobComposer&, KJob*)> mErrorHandler;
    KJob *mCurrentJob;
};

class ParallelCompositeJob : public KCompositeJob
{
    Q_OBJECT
public:
    explicit ParallelCompositeJob(QObject *parent = 0);
    virtual ~ParallelCompositeJob();

    void start();
    bool addSubjob(KJob*);

protected Q_SLOTS:
    virtual void slotResult(KJob *job);

};

#endif
