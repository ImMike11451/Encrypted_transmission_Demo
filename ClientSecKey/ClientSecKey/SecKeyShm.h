#pragma once
#include "BaseShm.h"
#include "SeckKeyNodeInfo.h"
#include <string.h>

class SecKeyShm : public BaseShm
{
public:
	SecKeyShm(int key, int maxNode);
	SecKeyShm(string pathName, int maxNode);
	~SecKeyShm();

	// 初始化共享内存
	void shmInit();
	// 将节点信息写入共享内存, 如果已经存在相同clientID和serverID的节点信息, 则覆盖原有数据
	int shmWrite(NodeSHMInfo* pNodeInfo);
	// 从共享内存中读取秘钥信息
	NodeSHMInfo shmRead(string clientID, string serverID);
	// 更新共享内存中秘钥的状态
	int shmUpdateStatus(string clientID, string serverID, int status);

private:
	int m_maxNode;
};

