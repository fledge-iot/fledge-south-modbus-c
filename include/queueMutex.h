#ifndef _QUEUEMUTEX_H
#define _QUEUEMUTEX_H
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

/**
 * A simple implementation of a queued mutex.
 *
 * We want to make sure that the threads competing for the mutex
 * are treated fairly. This is done by creating a queue of threads waiting
 * for the mutex when they block. The mutex is then granted to the
 * threads in the order in which they requested it.
 *
 * This is to solve an issue that was seeing mutex starvation to a thread that
 * was seen because of the way the threads where being scheduled.
 */
class QueueMutex {
	public:
		QueueMutex() : m_locked(false) {};
		~QueueMutex() {};
		/**
		 * Lock the mutex. If the mutex is already taken then
		 * we join the queue of threads waiting for the mutex.
		 */
		void	lock()
			{
				std::unique_lock<std::mutex> guard(m_guard);
				if (m_locked == false)
				{
					m_locked = true;
					return;
				}
				// Need to queue to wait for mutex
				m_queue.push(std::this_thread::get_id());
				bool myLock = false;
				do {
					m_cv.wait(guard);
					if (m_queue.front() == std::this_thread::get_id())
					{
						myLock = true;
						m_queue.pop();
					}
				} while (myLock == false);
				m_locked = true;

			};
		/**
		 * Unlock the mutex and notify any waiting threads the mutex is
		 * available.
		 */
		void	unlock()
			{
				std::lock_guard<std::mutex> guard(m_guard);
				m_locked = false;
				m_cv.notify_all();
			}
	private:
		std::mutex		m_guard;
		std::condition_variable	m_cv;
		bool			m_locked;
		std::queue<std::thread::id>
					m_queue;

};
#endif
