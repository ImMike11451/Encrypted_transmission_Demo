#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threadCount)
	:m_stop(false)
{
	for (size_t i = 0; i < threadCount; ++i)
	{
		//创建线程
		m_workers.emplace_back(&ThreadPool::worker, this);
	}
}

ThreadPool::~ThreadPool()
{
	// 设置停止标志
	m_stop = true;
	// 通知所有线程
	m_cond.notify_all();
	// 等待所有线程完成
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
		// 添加任务到队列
		m_tasks.push(task);
	}
	// 通知一个线程有新任务
	m_cond.notify_one();
}

void ThreadPool::worker()
{
	while (true)
	{
		// 定义任务
		std::function<void()> task;

		{
			// 获取锁
			std::unique_lock<std::mutex> lock(m_mutex);

			// 等待任务或停止信号
			m_cond.wait(lock, [this]() {
				return m_stop || !m_tasks.empty(); 
			});

			//如果停止并且没有任务了，退出线程
			if(m_stop && m_tasks.empty())
				return; // 停止线程

			// 获取任务
			task = m_tasks.front();
			m_tasks.pop();
		}

		// 执行任务
		task();
	}
}
