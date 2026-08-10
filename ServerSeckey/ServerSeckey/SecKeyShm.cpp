#include "SecKeyShm.h"
#include "Logger.h"
#include <string.h>
#include <iostream>
using namespace std;

SecKeyShm::SecKeyShm(int key, int maxNode)
	: BaseShm(key, maxNode * sizeof(NodeSHMInfo))
	, m_maxNode(maxNode)
{
}

SecKeyShm::SecKeyShm(string pathName, int maxNode)
	: BaseShm(pathName, maxNode * sizeof(NodeSHMInfo))
	, m_maxNode(maxNode)
{
}

SecKeyShm::~SecKeyShm()
{
}

void SecKeyShm::shmInit()
{
	if (m_shmAddr != NULL)
	{
		memset(m_shmAddr, 0, m_maxNode * sizeof(NodeSHMInfo));
	}
}

int SecKeyShm::shmWrite(NodeSHMInfo * pNodeInfo)
{
	int ret = -1;
	// 每次操作前临时映射共享内存，操作结束后解除映射，避免长期持有裸指针。
	NodeSHMInfo* pAddr = static_cast<NodeSHMInfo*>(mapShm());
	if (pAddr == NULL)
	{
		return ret;
	}

	// 先查找同一 clientID + serverID 的节点，命中则覆盖旧密钥。
	NodeSHMInfo	*pNode = NULL;
	for (int i = 0; i < m_maxNode; i++)
	{
		pNode = pAddr + i;
		if (strcmp(pNode->clientID, pNodeInfo->clientID) == 0 &&
			strcmp(pNode->serverID, pNodeInfo->serverID) == 0)
		{
			memcpy(pNode, pNodeInfo, sizeof(NodeSHMInfo));
			unmapShm();
			return 0;
		}
	}

	// 如果没有旧节点，就找一个空槽位写入新密钥。
	int i = 0;
	NodeSHMInfo tmpNodeInfo{};
	for (i = 0; i < m_maxNode; i++)
	{
		pNode = pAddr + i;
		if (memcmp(&tmpNodeInfo, pNode, sizeof(NodeSHMInfo)) == 0)
		{
			ret = 0;
			memcpy(pNode, pNodeInfo, sizeof(NodeSHMInfo));
			break;
		}
	}
	if (i == m_maxNode)
	{
		ret = -1;
	}

	unmapShm();
	return ret;
}

NodeSHMInfo SecKeyShm::shmRead(string clientID, string serverID)
{
	int ret = 0;
	NodeSHMInfo *pAddr = NULL;
	pAddr = static_cast<NodeSHMInfo*>(mapShm());
	if (pAddr == NULL)
	{
		Logger::error("共享内存关联失败...");
		return NodeSHMInfo();
	}

	int i = 0;
	NodeSHMInfo info;
	NodeSHMInfo	*pNode = NULL;
	// 通过 clientID + serverID 精确定位一条会话密钥。
	for (i = 0; i < m_maxNode; i++)
	{
		pNode = pAddr + i;
		if (strcmp(pNode->clientID, clientID.data()) == 0 &&
			strcmp(pNode->serverID, serverID.data()) == 0)
		{
			info = *pNode;
			break;
		}
	}

	unmapShm();
	return info;
}

int SecKeyShm::shmUpdateStatus(string clientID, string serverID, int status)
{
	NodeSHMInfo* pAddr = static_cast<NodeSHMInfo*>(mapShm());

	if (pAddr == NULL)
	{
		Logger::error("共享内初关联失败...");
		return -1;
	}

	NodeSHMInfo* pNode = NULL;
	// 注销密钥时只更新状态，不清空 key 本体，便于保留缓存结构和历史排查信息。
	for(int i = 0; i < m_maxNode; i++)
	{
		pNode = pAddr + i;
		if (strcmp(pNode->clientID, clientID.data()) == 0 &&
			strcmp(pNode->serverID, serverID.data()) == 0)
		{
			pNode->status = status;
			unmapShm();
			return 0;
		}
	}
	unmapShm();
	Logger::error("共享内存中未找到对应节点...");
	return -1;
}
