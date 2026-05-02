#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <iostream>

// =======================================================
// 线程池类
// =======================================================
class ThreadPool
{
public:
	// 构造函数，创建指定数量的线程
	explicit ThreadPool(size_t threadCount = 4);

	// 析构函数，停止所有线程并清理资源
	~ThreadPool();

	//提交任务
	void enqueue(const std::function<void()>& task);

private:
	void worker(); // 工作线程函数

private:
	std::vector<std::thread> m_workers; // 工作线程列表
	std::queue<std::function<void()>> m_tasks; // 任务队列
	std::mutex m_mutex; // 互斥锁
	std::condition_variable m_cond; // 条件变量
	std::atomic<bool> m_stop; // 停止标志
};

