#ifndef QTSTORAGE_EXPIRINGSTORAGE_H
#define QTSTORAGE_EXPIRINGSTORAGE_H

#include <QMap>
#include <QTimer>
#include <QSharedPointer>
#include <QReadWriteLock>


namespace qtstorage {

template <class K, class V>
class ExpiringStorage {
public:
    using Handler = void (*)(const K & key,
                             const V & value);

public:
    inline void insert(const K & key,
                       const V & value,
                       quint64 lifetimeMsec = 0,
                       Handler handler = nullptr);

    inline bool remove(const K & key);
    inline V take(const K & key);
    inline V value(const K & key);
    inline QList<V> values();

    inline bool contains(const K & key);

private:
    inline void watch(const K & key,
                      quint64 lifetimeMsec = 0,
                      Handler handler = nullptr);

    inline QTimer * createTimer(const K & key);

private:
    QObject ctx;
    QReadWriteLock mtx;

    QMap<K,V> items;
    QMap<K, Handler> handlers;
    QMap<K, QSharedPointer<QTimer> > timers;
};

template <class K, class V>
void ExpiringStorage<K, V>::insert(const K & key,
                                   const V & value,
                                   quint64 lifetimeMsec,
                                   Handler handler)
{
    QWriteLocker locker(&mtx);
    items[key] = value;

    if (lifetimeMsec > 0) {
        watch(key, lifetimeMsec, handler);
    }
}

template<class K, class V>
bool ExpiringStorage<K, V>::remove(const K & key)
{
    QWriteLocker locker(&mtx);

    handlers.remove(key);
    timers.remove(key);
    return (items.remove(key) > 0);
}

template<class K, class V>
V ExpiringStorage<K, V>::take(const K & key)
{
    QWriteLocker locker(&mtx);

    handlers.remove(key);
    timers.remove(key);
    return items.take(key);
}

template<class K, class V>
V ExpiringStorage<K, V>::value(const K & key)
{
    QReadLocker locker(&mtx);
    return items.value(key);
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
void ExpiringStorage<K, V>::watch(const K & key, quint64 lifetimeMsec, Handler handler)
{
    QTimer::singleShot(0, &ctx, [this, key, lifetimeMsec, handler]() -> void
    {
        if (items.contains(key))
        {
            handlers.insert(key, handler);

            auto timerIt = timers.find(key);
            if (timerIt == timers.end()) {
                timerIt = timers.insert(key, QSharedPointer<QTimer>( createTimer(key) ));
            }

            timerIt.value()->start(std::chrono::milliseconds(lifetimeMsec));
        }
    });
}

template<class K, class V>
QTimer * ExpiringStorage<K, V>::createTimer(const K & key)
{
    auto * timer = new QTimer;
    timer->setSingleShot(true);

    QObject::connect(timer, &QTimer::timeout, &ctx, [this, key]() -> void
    {
        QWriteLocker locker(&mtx);

        auto value = items.take(key);
        auto handler = handlers.take(key);

        if (handler) {
            (*handler)(key, value);
        }

        timers.remove(key);
    });

    return timer;
}

}

#endif // QTSTORAGE_EXPIRINGSTORAGE_H
