#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <iostream>

// 职责：维护一组工作线程，异步执行 ServerOP 投递的客户端请求任务。
// 边界：线程池只负责调度任务，不关心任务里的 socket、protobuf 或业务细节。
class ThreadPool
{
public:
	// 构造时创建固定数量的工作线程。
	explicit ThreadPool(size_t threadCount = 4);

	// 析构时停止接收新任务，唤醒并回收所有工作线程。
	~ThreadPool();

	// 提交一个待执行任务。
	void enqueue(const std::function<void()>& task);

private:
	// 工作线程主循环：等待任务、取出任务、执行任务。
	void worker();

private:
	std::vector<std::thread> m_workers;
	std::queue<std::function<void()>> m_tasks;
	std::mutex m_mutex;
	std::condition_variable m_cond;
	std::atomic<bool> m_stop;
};

