#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threadCount)
	:m_stop(false)
{
	for (size_t i = 0; i < threadCount; ++i)
	{
		m_workers.emplace_back(&ThreadPool::worker, this);
	}
}

ThreadPool::~ThreadPool()
{
	// 先设置停止标志，再唤醒所有 worker。
	// worker 会处理完队列中已有任务后退出。
	m_stop = true;
	m_cond.notify_all();
	for (auto& th : m_workers)
	{
		if (th.joinable())
			th.join();
	}
}

void ThreadPool::enqueue(const std::function<void()>& task)
{

	if (m_stop)
	{
		throw std::runtime_error("ThreadPool has been stopped, cannot enqueue new tasks.");
	}

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_tasks.push(task);
	}
	m_cond.notify_one();
}

void ThreadPool::worker()
{
	while (true)
	{
		std::function<void()> task;

		{
			std::unique_lock<std::mutex> lock(m_mutex);

			// 条件变量避免 worker 空转；收到任务或停止信号才继续。
			m_cond.wait(lock, [this]() {
				return m_stop || !m_tasks.empty(); 
			});

			// 停止且没有待处理任务时退出，确保析构能平滑收尾。
			if(m_stop && m_tasks.empty())
				return;

			task = m_tasks.front();
			m_tasks.pop();
		}

		// 在锁外执行任务，避免长业务处理阻塞其他 worker 取任务。
		task();
	}
}
