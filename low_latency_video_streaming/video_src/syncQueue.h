#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class DelayQueue {
private:
    std::queue<T> q_;
    mutable std::mutex _mux;
    size_t capacity_ = 1; // queue size
    std::condition_variable _fullQ;

private:
    // Disable copy and assign
    DelayQueue(const DelayQueue& rhs) = delete;
    DelayQueue(DelayQueue&& rhs) = delete;
    DelayQueue& operator= (const DelayQueue& rhs) = delete;
    DelayQueue& operator= (DelayQueue&& rhs) = delete;


public:

    DelayQueue() : _mux(), _fullQ(), capacity_() {}

    void push(const T& element) {

        std::unique_lock<std::mutex> lock(_mux);
        if (q_.size() == capacity_) {
            //_fullQ.wait(lock, [&] { return q_.size() < capacity_; });
            // if it's full, pop the oldest one
            q_.pop();
        }
        q_.push(element);
        lock.unlock();

        if (q_.size() == capacity_ ) _fullQ.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(_mux);
        while (q_.size() < capacity_) {
            _fullQ.wait(lock, [&] { return (q_.size() == capacity_) ; });
        }

        T element = q_.front();
        q_.pop();
        lock.unlock();

        return element;
    }

    int const getCapapcity() {
        return capacity_;
    }

    void setCapacity(const size_t& capacity) {

        std::unique_lock<std::mutex> lock(_mux);
        if (capacity < capacity_) {
            // if it shrinks down, clear it first
            while (!q_.empty()) {
                q_.pop();
            }
        }
        capacity_ = capacity;
        lock.unlock();
    }
};

template<typename T>
class SyncQueue {

    std::queue<T> q_;
    std::mutex _mux;
    std::condition_variable _fullQ;
    std::condition_variable _emptyQ;
    std::size_t capacity_ = 1;


public:

    // Constructor
    SyncQueue() : q_(), _mux(), _fullQ(), _emptyQ(), capacity_(1) {}

    // Move constructor
    SyncQueue(SyncQueue&& rhs) noexcept {
        std::lock_guard<std::mutex> lock(rhs._mux); // Lock the source object
        q_ = std::move(rhs.q_);
        capacity_ = rhs.capacity_;
    }

    // Move assignment operator
    SyncQueue& operator=(SyncQueue&& rhs) noexcept {
        if (this != &rhs) {
            std::lock_guard<std::mutex> lockThis(_mux);
            std::lock_guard<std::mutex> lockRhs(rhs._mux);

            q_ = std::move(rhs.q_);
            capacity_ = rhs.capacity_;
        }
        return *this;
    }

    // Disable copy constructor and copy assignment
    SyncQueue(const SyncQueue& rhs) = delete;
    SyncQueue& operator= (const SyncQueue& rhs) = delete;

    void push(T element) {
#if 0
        std::unique_lock<std::mutex> lock(_mux);
        while (q_.size() == capacity_) {
            _fullQ.wait(lock, [&] { return q_.size() < capacity_; });
        }
#else
        std::lock_guard<std::mutex> lock(_mux);
        if (q_.size() >= capacity_) {
            _emptyQ.notify_one();
            return;
        }
#endif
        q_.push(element);
        // lock.unlock();
        _emptyQ.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(_mux);
#if 0
        while (q_.empty()) {
            _emptyQ.wait(lock, [&] { return !q_.empty(); });
        }
#else
        _emptyQ.wait(lock, [this]() { return !q_.empty(); });
#endif
        T element = q_.front();
        q_.pop();
        // lock.unlock();
        // _fullQ.notify_one();
        return element;
    }
};
