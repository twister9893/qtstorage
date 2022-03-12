#pragma once

#include <QQueue>
#include <QReadWriteLock>
#include <QSemaphore>


namespace qtstorage {

template <class T>
class BlockingQueue {
public:
    inline void enqueue(const T & item);
    inline T dequeue(qint64 timeout = -1, bool * ok = nullptr);
    inline int size() const;

private:
    QQueue<T> items;
    QSemaphore semaphore;
    QReadWriteLock mtx;
};

template<class T>
void BlockingQueue<T>::enqueue(const T & item)
{
    QWriteLocker locker(&mtx);
    items.enqueue(item);

    semaphore.release(1);
}

template<class T>
T BlockingQueue<T>::dequeue(qint64 timeout, bool * ok)
{
    if (!semaphore.tryAcquire(1, int(timeout))) {
        if (ok) { *ok = false; }
        return T();
    }

    QWriteLocker locker(&mtx);
    if (ok) { *ok = true; }
    return items.dequeue();
}

template<class T>
int BlockingQueue<T>::size() const
{
    QReadLocker locker(&mtx);
    return items.size();
}

}
