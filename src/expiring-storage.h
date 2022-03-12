#pragma once

#include <QDebug>
#include <QMap>
#include <QTimer>
#include <QSharedPointer>
#include <QReadWriteLock>
#include <functional>


namespace qtstorage {

template <class K, class V>
class ExpiringStorage {
public:
    using Handler = std::function<void(K,V)>;

public:
    inline void insert(const K & key,
                       const V & value,
                       qint64 lifetimeMsec = 0);

    inline bool remove(const K & key);
    inline V take(const K & key);
    inline V value(const K & key, const V & defaultValue = V());
    inline QList<V> values();

    inline bool contains(const K & key);
    inline int size();
    inline void clear();

    inline typename QMap<K,V>::const_iterator find(const K & key) const;

    inline typename QMap<K,V>::const_iterator begin() const;
    inline typename QMap<K,V>::const_iterator end() const;

    inline void installExpirationHandler(Handler handler);

private:
    inline void watch(const K & key,
                      qint64 lifetimeMsec = 0);

    inline QTimer * createTimer(const K & key);
    inline void removeTimer(const K & key);

private:
    QObject ctx;
    QReadWriteLock mtx;
    Handler expirationHandler = nullptr;

    QMap<K,V> items;
    QMap<K, QTimer*> timers;
};

template <class K, class V>
void ExpiringStorage<K, V>::insert(const K & key,
                                   const V & value,
                                   qint64 lifetimeMsec)
{
    QWriteLocker locker(&mtx);
    items.insert(key, value);

    if (lifetimeMsec > 0) {
        watch(key, lifetimeMsec);
    }
}

template<class K, class V>
bool ExpiringStorage<K, V>::remove(const K & key)
{
    QWriteLocker locker(&mtx);

    removeTimer(key);
    return (items.remove(key) > 0);
}

template<class K, class V>
V ExpiringStorage<K, V>::take(const K & key)
{
    QWriteLocker locker(&mtx);

    removeTimer(key);
    return items.take(key);
}

template<class K, class V>
V ExpiringStorage<K, V>::value(const K & key, const V & defaultValue)
{
    QReadLocker locker(&mtx);
    return items.value(key, defaultValue);
}

template<class K, class V>
QList<V> ExpiringStorage<K, V>::values()
{
    QReadLocker locker(&mtx);
    return items.values();
}

template<class K, class V>
bool ExpiringStorage<K, V>::contains(const K & key)
{
    QReadLocker locker(&mtx);
    return items.contains(key);
}

template<class K, class V>
int ExpiringStorage<K, V>::size()
{
    QReadLocker locker(&mtx);
    return items.size();
}

template<class K, class V>
void ExpiringStorage<K, V>::clear()
{
    QWriteLocker locker(&mtx);

    for (const auto & key : timers.keys()) {
        removeTimer(key);
    }

    items.clear();
}

template<class K, class V>
typename QMap<K,V>::const_iterator ExpiringStorage<K,V>::find(const K & key) const
{
    return items.find(key);
}

template<class K, class V>
typename QMap<K,V>::const_iterator ExpiringStorage<K,V>::begin() const
{
    return items.begin();
}

template<class K, class V>
typename QMap<K,V>::const_iterator ExpiringStorage<K,V>::end() const
{
    return items.end();
}

template<class K, class V>
void ExpiringStorage<K, V>::installExpirationHandler(Handler handler)
{
    QWriteLocker locker(&mtx);
    expirationHandler = handler;
}

template<class K, class V>
void ExpiringStorage<K, V>::watch(const K & key, qint64 lifetimeMsec)
{
    auto timerIt = timers.find(key);
    if (timerIt == timers.end()) {
        timerIt = timers.insert(key, createTimer(key));
    }

    QMetaObject::invokeMethod(timerIt.value(),
                              "start",
                              Qt::QueuedConnection,
                              Q_ARG(int, int(lifetimeMsec)));
}

template<class K, class V>
QTimer * ExpiringStorage<K, V>::createTimer(const K & key)
{
    auto * timer = new QTimer;
    timer->moveToThread(ctx.thread());
    timer->setSingleShot(true);

    QObject::connect(timer, &QTimer::timeout, &ctx, [this, key]() -> void
    {
        mtx.lockForWrite();

        const auto & value = items.take(key);
        removeTimer(key);

        mtx.unlock();

        if (expirationHandler) {
            expirationHandler(key, value);
        }
    });

    return timer;
}

template<class K, class V>
void ExpiringStorage<K,V>::removeTimer(const K & key)
{
    auto * timerObject = timers.take(key);
    if (timerObject)
    {
        QMetaObject::invokeMethod(timerObject,
                                  "deleteLater",
                                  Qt::QueuedConnection);
    }
}

}
