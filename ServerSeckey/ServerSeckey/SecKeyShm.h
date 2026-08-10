#pragma once
#include "BaseShm.h"
#include "SeckKeyNodeInfo.h"
#include <string.h>

// 职责：在 System V 共享内存中缓存活跃会话密钥。
// 边界：只负责 NodeSHMInfo 的读写和状态更新，不负责密钥生成、加密或数据库持久化。
class SecKeyShm : public BaseShm
{
public:
	SecKeyShm(int key, int maxNode);
	SecKeyShm(string pathName, int maxNode);
	~SecKeyShm();

	// 初始化共享内存，将所有节点清零。
	void shmInit();
	// 写入节点；如果 clientID + serverID 已存在，则覆盖旧 key。
	int shmWrite(NodeSHMInfo* pNodeInfo);
	// 根据 clientID + serverID 读取会话密钥。
	NodeSHMInfo shmRead(string clientID, string serverID);
	// 更新密钥状态，主要用于注销后同步本地缓存。
	int shmUpdateStatus(string clientID, string serverID, int status);

private:
	int m_maxNode;
};

