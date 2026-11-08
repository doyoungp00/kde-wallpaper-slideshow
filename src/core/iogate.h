#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

// Lets the system sleep while the library is on a network share. Every read of the
// library folders runs inside a Scope; pause() makes new reads wait and returns
// once none is in flight. A thread blocked in a network read can't be frozen, but
// one waiting here can.
class IoGate
{
public:
    static IoGate &instance()
    {
        static IoGate gate;
        return gate;
    }

    class Scope
    {
    public:
        Scope()
        {
            instance().enter();
        }
        ~Scope()
        {
            instance().leave();
        }
        Scope(const Scope &) = delete;
        Scope &operator=(const Scope &) = delete;
    };

    // Returns false if reads were still in flight after timeout.
    bool pause(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(m_mutex);
        m_paused = true;
        return m_idle.wait_for(lock, timeout, [this] {
            return m_active == 0;
        });
    }

    void resume()
    {
        std::lock_guard lock(m_mutex);
        m_paused = false;
        m_open.notify_all();
    }

private:
    void enter()
    {
        std::unique_lock lock(m_mutex);
        m_open.wait(lock, [this] {
            return !m_paused;
        });
        ++m_active;
    }

    void leave()
    {
        std::lock_guard lock(m_mutex);
        if (--m_active == 0) {
            m_idle.notify_all();
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_open;
    std::condition_variable m_idle;
    int m_active = 0;
    bool m_paused = false;
};
