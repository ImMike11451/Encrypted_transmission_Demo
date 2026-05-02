#pragma once
// =======================================================
// 文件名: ErrorCode.h
// 作用:   定义项目统一错误码
// 说明:
//   1. 以后不要再到处 return false / return -1 / return 3002
//   2. 统一用错误码，便于日志记录和问题定位
// =======================================================

enum ErrorCode
{
	//通用
	ERR_OK = 0,   //成功
	ERR_PARAM = 1001, //参数错误
	ERR_NULLPTR = 1002, //空指针
	ERR_PARSE = 1003, //解析错误
	ERR_UNKNOWN = 1004, //未知错误

	//网络模块
	ERR_CONNECT = 2001, //连接错误
	ERR_SEND = 2002, //发送错误
	ERR_RECV = 2003, //接收错误
	ERR_TIMEOUT = 2004, //超时错误
	ERR_PEER_CLOSED = 2005, //对端关闭连接

	//密码学模块
	ERR_RSA_SIGN = 3001, //RSA签名错误
	ERR_RSA_VERIFY = 3002, //RSA验签错误
	ERR_RSA_ENCRYPT = 3003, //RSA加密错误
	ERR_RSA_DECRYPT = 3004, //RSA解密错误
	ERR_AES = 3005,      //AES加解密错误

	//数据库模块
	ERR_DB_CONNECT = 4001, //数据库连接错误
	ERR_DB_QUERY = 4002, //SQL执行失败
	ERR_DB_NOT_FOUND = 4003, //查询无结果
	ERR_DB_UPDATE = 4004, //更新失败

	//共享内存模块
	ERR_SHM_MAP = 5001, //共享内存映射错误
	ERR_SHM_NOT_FOUND = 5002, //共享内存中节点未找到
	ERR_SHM_WRITE = 5003, //共享内存写入错误
	ERR_SHM_UPDATE = 5004, //共享内存更新错误

};