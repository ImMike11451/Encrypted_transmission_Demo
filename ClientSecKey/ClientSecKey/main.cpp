#include <iostream>
#include "ClientOP.h"

void InitUI()
{
	std::cout << "==========================================" << "\n";
	std::cout << "---------- 1. 秘钥协商 ----------" << "\n";
	std::cout << "---------- 2. 秘钥校验 ----------" << "\n";
	std::cout << "---------- 3. 发送加密消息 ------" << "\n";
	std::cout << "---------- 4. 查询消息 ----------" << "\n";
	std::cout << "---------- 5. 查询消息列表 ------" << "\n";
	std::cout << "---------- 6. 秘钥注销 ----------" << "\n";
	std::cout << "---------- 7. 退    出 ----------" << "\n";
	std::cout << "==========================================" << "\n";
}


int main(int argc, char* argv[])
{
    // 默认读取 client.json。
    // 如果启动时传入参数，就读取参数指定的配置文件。
    std::string configFile = "client.json";

    if (argc >= 2)
    {
        configFile = argv[1];
    }

    std::cout << "当前客户端配置文件: " << configFile << std::endl;

    int nChoice = 0;

    ClientOP clientOP(configFile);

    while (nChoice != 7)
    {
        InitUI();

        std::cout << "请输入您的选择：";
        std::cin >> nChoice;

        switch (nChoice)
        {
        case 1:
            clientOP.keyAgreement();
            break;
        case 2:
            clientOP.keyVerification();
            break;
        case 3:
            clientOP.sendEncryptedMessage();
            
            break;
        case 4:
            clientOP.queryMessage();
            break;
        case 5:
            clientOP.queryRecentMessages();
            break;
        case 6:
            clientOP.keyLogout();
            break;
        default:
            break;
        }
    }

    return 0;
}