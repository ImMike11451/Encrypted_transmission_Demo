#include <iostream>
#include "ClientOP.h"

void InitUI()
{
	std::cout << "==========================================" << "\n";
	std::cout << "---------- 1. 秘钥协商 ----------" << "\n";
	std::cout << "---------- 2. 秘钥校验 ----------" << "\n";
	std::cout << "---------- 3. 秘钥注销 ----------" << "\n";
	std::cout << "---------- 4. 发送加密消息 ----------" << "\n";
	std::cout << "---------- 5. 退    出 ----------" << "\n";
	std::cout << "==========================================" << "\n";

}


int main()
{

	int nChoice = 0;

	ClientOP clientOP("client.json");

	while (nChoice != 5)
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
			clientOP.keyLogout();
			break;
		case 4:
			clientOP.sendEncryptedMessage();
			break;
		default:
			break;
		}
	}

    return 0;
}