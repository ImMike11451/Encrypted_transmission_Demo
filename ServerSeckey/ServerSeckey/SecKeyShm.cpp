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
	// 关联共享内存
	NodeSHMInfo* pAddr = static_cast<NodeSHMInfo*>(mapShm());
	if (pAddr == NULL)
	{
		return ret;
	}

	// 判断传入的网点密钥是否已经存在
	NodeSHMInfo	*pNode = NULL;
	for (int i = 0; i < m_maxNode; i++)
	{
		// pNode依次指向每个节点的首地址
		pNode = pAddr + i;
		Logger::info("clientID 比较: " + std::string(pNode->clientID) + ", " + std::string(pNodeInfo->clientID));
		Logger::info("serverID 比较: " + std::string(pNode->serverID) + ", " + std::string(pNodeInfo->serverID));
		if (strcmp(pNode->clientID, pNodeInfo->clientID) == 0 &&
			strcmp(pNode->serverID, pNodeInfo->serverID) == 0)
		{
			// 如果找到了该网点秘钥已经存在, 使用新秘钥覆盖旧的值
			memcpy(pNode, pNodeInfo, sizeof(NodeSHMInfo));
			unmapShm();
			Logger::info("写数据成功: 原数据被覆盖!");
			return 0;
		}
	}

	// 若没有找到对应的信息, 找一个空节点将秘钥信息写入
	int i = 0;
	NodeSHMInfo tmpNodeInfo; //空结点
	for (i = 0; i < m_maxNode; i++)
	{
		pNode = pAddr + i;
		if (memcmp(&tmpNodeInfo, pNode, sizeof(NodeSHMInfo)) == 0)
		{
			ret = 0;
			memcpy(pNode, pNodeInfo, sizeof(NodeSHMInfo));
			Logger::info("写数据成功: 在新的节点上添加数据!");
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
	// 关联共享内存
	NodeSHMInfo *pAddr = NULL;
	pAddr = static_cast<NodeSHMInfo*>(mapShm());
	if (pAddr == NULL)
	{
		Logger::error("共享内存关联失败...");
		return NodeSHMInfo();
	}
	Logger::info("共享内存关联成功...");

	//遍历网点信息
	int i = 0;
	NodeSHMInfo info;
	NodeSHMInfo	*pNode = NULL;
	// 通过clientID和serverID查找节点
	Logger::info("maxNode: " + std::to_string(m_maxNode));
	for (i = 0; i < m_maxNode; i++)
	{
		pNode = pAddr + i;
		Logger::info("clientID 比较: " + std::string(pNode->clientID) + ", " + clientID);
		Logger::info("serverID 比较: " + std::string(pNode->serverID) + ", " + serverID);
		if (strcmp(pNode->clientID, clientID.data()) == 0 &&
			strcmp(pNode->serverID, serverID.data()) == 0)
		{
			// 找到的节点信息, 拷贝到传出参数
			info = *pNode;
			Logger::info("秘钥ID: " + std::to_string(info.seckeyID));
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
	for(int i = 0; i < m_maxNode; i++)
	{
		pNode = pAddr + i;
		if (strcmp(pNode->clientID, clientID.data()) == 0 &&
			strcmp(pNode->serverID, serverID.data()) == 0)
		{
			pNode->status = status;
			Logger::info("更新状态成功! 当前status = " + std::to_string(pNode->status));
			unmapShm();
			return 0;
		}
	}
	unmapShm();
	Logger::error("共享内存中未找到对应节点...");
	return -1;
}
