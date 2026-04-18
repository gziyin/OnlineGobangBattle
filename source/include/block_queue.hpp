#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

namespace gobang {

template <class T>
class BlockQueue {
public:
    BlockQueue() : _shutdown(false) {}
    ~BlockQueue() {}

    bool timed_pop(T& out, int timeout_ms = 100);
    void push(const T& value);
    void shutdown();
    bool is_shutdown() const;
    size_t size() const;
    bool empty() const;

    BlockQueue(const BlockQueue&) = delete;
    BlockQueue& operator=(const BlockQueue&) = delete;

private:
    mutable std::mutex _mtx;
    std::condition_variable _cv;
    std::queue<T> _queue;
    std::atomic<bool> _shutdown;
};

template <class T>
bool BlockQueue<T>::timed_pop(T& out, int timeout_ms) {
    std::unique_lock<std::mutex> lock(_mtx);
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (_queue.empty() && !_shutdown.load()) {
        if (_cv.wait_until(lock, deadline) == std::cv_status::timeout) {
            return false;
        }
    }

    if (_queue.empty()) {
        return false;
    }

    out = _queue.front();
    _queue.pop();
    return true;
}

template <class T>
void BlockQueue<T>::push(const T& value) {
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_shutdown.load()) {
            return;
        }
        _queue.push(value);
    }
    _cv.notify_one();
}

template <class T>
void BlockQueue<T>::shutdown() {
    _shutdown.store(true);
    _cv.notify_all();
}

template <class T>
bool BlockQueue<T>::is_shutdown() const {
    return _shutdown.load();
}

template <class T>
size_t BlockQueue<T>::size() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _queue.size();
}

template <class T>
bool BlockQueue<T>::empty() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _queue.empty();
}

} // namespace gobang
