#include "jobcomposer.h"

void JobHandler::onResult(KJob *job)
{

}

void AsyncTask::install(KJob *job, const std::function<void()> &handler)
{
    // auto self = jobHandlerInstance();
    // QObject::connect(job, SIGNAL(result(KJob*)), self, SLOT(handleJobResult(KJob*)), Qt::UniqueConnection);
    // self->m_handlers[job] << handler;
    // job->start();
    //
    mHandler = handler;
    // auto jobHandler = new JobHandler;
    QObject::connect(job, SIGNAL(result(KJob*)), this, SLOT(onResult(KJob*)), Qt::UniqueConnection);
    job->start();
}

void AsyncTask::install(KJob *job, const std::function<void(KJob*)> &handler)
{
    mHandlerWithJob = handler;
    QObject::connect(job, SIGNAL(result(KJob*)), this, SLOT(onResult(KJob*)), Qt::UniqueConnection);
    job->start();
}

void AsyncTask::install(KJob *job, const char *signal, const std::function<void(KJob*)> &handler)
{
    // mHandlerWithJob = handler;
    QObject::connect(job, signal, this, SLOT(onResult(KJob*)), Qt::UniqueConnection);
    job->start();

}

void AsyncTask::onResult(KJob *job)
{
    if (mHandler) {
        mHandler();
    }
    if (mHandlerWithJob) {
        mHandlerWithJob(job);
    }
}

void JobComposer::start()
{
    processNext(0);
}

void JobComposer::processNext(KJob *job)
{
    const std::function<void(JobComposer&, KJob*)> jobContinuation = mContinuationQueue.dequeue();
    jobContinuation(*this, job);
}

void JobComposer::add(const std::function<void(JobComposer&, KJob*)> &jobContinuation)
{
    mContinuationQueue.enqueue(jobContinuation);
}

void JobComposer::run(KJob *job)
{
    connect(job, SIGNAL(result(KJob*)), this, SLOT(onResult(KJob*)));
    job->start();
}

void JobComposer::onResult(KJob *job)
{
    //TODO error handling
    processNext(job);
}
