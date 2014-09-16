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

JobComposer::JobComposer(QObject *parent)
    :KJob(parent),
    mCurrentJob(0)
{

}

JobComposer::~JobComposer()
{

}

void JobComposer::start()
{
    processNext(0);
}

void JobComposer::processNext(KJob *job)
{
    const std::function<void(JobComposer&, KJob*)> jobContinuation = mContinuationQueue.dequeue();
    jobContinuation(*this, job);
    //No job was started during the continuation, we assume that means we're done
    if (!mCurrentJob) {
        emitResult();
    }
}

void JobComposer::add(const std::function<void(JobComposer&, KJob*)> &jobContinuation)
{
    mContinuationQueue.enqueue(jobContinuation);
}

void JobComposer::run(KJob *job)
{
    mCurrentJob = job;
    connect(job, SIGNAL(result(KJob*)), this, SLOT(onResult(KJob*)));
    job->start();
}

void JobComposer::run(KJob *job, const std::function<bool(JobComposer&, KJob*)> &errorHandler)
{
    mErrorHandler = errorHandler;
    run(job);
}

void JobComposer::onResult(KJob *job)
{
    mCurrentJob = 0;
    if (job->error()) {
        if (mErrorHandler) {
            setError(KJob::UserDefinedError);
            if (!mErrorHandler(*this, job)) {
                emitResult();
                return;
            }
        }
    }
    processNext(job);
}


ParallelCompositeJob::ParallelCompositeJob(QObject *parent)
    :KCompositeJob(parent)
{

}

ParallelCompositeJob::~ParallelCompositeJob()
{

}

void ParallelCompositeJob::start()
{
    Q_FOREACH (KJob *job, subjobs()) {
        job->start();
    }
}

bool ParallelCompositeJob::addSubjob(KJob *job)
{
    return KCompositeJob::addSubjob(job);
}

void ParallelCompositeJob::slotResult(KJob *job)
{
    KCompositeJob::slotResult(job);
    if (subjobs().isEmpty()) {
        emitResult();
    }
}

