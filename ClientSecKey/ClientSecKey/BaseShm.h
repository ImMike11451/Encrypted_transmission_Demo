#pragma once
#include <iostream>
using namespace std;

// 职责：封装 System V 共享内存的创建、打开、映射、解除映射和删除。
// 边界：BaseShm 不关心共享内存中存放什么结构体，具体读写由派生类负责。
class BaseShm
{
public:
	// 通过 key 打开已有共享内存。
	BaseShm(int key);
	// 通过 key 创建或打开共享内存。
	BaseShm(int key, int size);
	// 通过路径生成 key，并打开已有共享内存。
	BaseShm(string name);
	// 通过路径生成 key，并创建或打开共享内存。
	BaseShm(string name, int size);
	// 将共享内存映射到当前进程地址空间。
	void* mapShm();
	// 解除当前进程与共享内存的映射关系。
	int unmapShm();
	// 删除共享内存段。
	int delShm();
	~BaseShm();

private:
	// 根据 key、大小和标志调用 shmget。
	int getShmID(key_t key, int shmSize, int flag);

private:
	int m_shmID;
protected:
	void* m_shmAddr = NULL;
};

