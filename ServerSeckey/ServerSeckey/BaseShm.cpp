#include "BaseShm.h"
#include "Logger.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

const char RandX = 'x';
BaseShm::BaseShm(int key)
{
	getShmID(key, 0, 0);
}

BaseShm::BaseShm(int key, int size)
{
	getShmID(key, size, IPC_CREAT | 0664);
}

BaseShm::BaseShm(string name)
{
	// ftok 要求路径存在；配置里的 ShmKey 路径错误时，后续 shmget 会失败。
	key_t key = ftok(name.data(), RandX);
	getShmID(key, 0, 0);
}

BaseShm::BaseShm(string name, int size)
{
	// 使用固定项目标识 RandX，让同一路径在不同进程中得到同一个 key。
	key_t key = ftok(name.data(), RandX);
	getShmID(key, size, IPC_CREAT | 0664);
}

void * BaseShm::mapShm()
{
	// shmat 返回的是进程内虚拟地址，调用方使用完成后必须 shmdt。
	m_shmAddr = shmat(m_shmID, NULL, 0);
	if (m_shmAddr == (void*)-1)
	{
		return NULL;
	}
	return m_shmAddr;
}

int BaseShm::unmapShm()
{
	int ret = shmdt(m_shmAddr);
	return ret;
}

int BaseShm::delShm()
{
	int ret = shmctl(m_shmID, IPC_RMID, NULL);
	return ret;
}

BaseShm::~BaseShm()
{
}

int BaseShm::getShmID(key_t key, int shmSize, int flag)
{
	m_shmID = shmget(key, shmSize, flag);
	if (m_shmID == -1)
	{
		Logger::error("shmget 失败");
	}
	return m_shmID;
}
