#include "EasyTcpClient.hpp"
#include <thread>

bool g_bRun = true;
void cmdThread()
{
	while (true)
	{
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		if (0 == strcmp(cmdBuf, "exit"))
		{
			g_bRun = false;
			printf("退出cmdThread线程\n");
			break;
		}
		else {
			printf("不支持的命令\n");
		}
	}
	return;
}

void sendThread(int id, int cCout) {
	// EasyTcpClient **client = (EasyTcpClient **)malloc(sizeof(EasyTcpClient *) * cCout);
	EasyTcpClient **client = new EasyTcpClient *[cCout];
	int begin = (id - 1) * cCout;

	for (int i = 0; i < cCout; i++) {
		client[i] = new EasyTcpClient();
	}

	for (int i = 0; i < cCout; i++) {
		client[i]->Connect("127.0.0.1", 4567);
		// printf("Connect = %d\n", i + begin + 1);
	}
	printf("id=%d链接成功成功\n", id);
	Login login[1];
	for (int i = 0; i < 1; ++i) {
		strcpy(login[i].userName, "xiao");
		strcpy(login[i].userWord, "xiaomm");
	}

	while (g_bRun)
	{
		for (int i = 0; i < cCout; i++) {
			client[i]->SendData(login);
			// client[i]->OnRun();
		}
	}
	for (int i = 0; i < cCout; i++) {
		client[i]->Close();
	}
	for (int i = 0; i < cCout; i++) {
		delete client[i];
	}
	delete[] client;
	return;
}

int main()
{
	const int cCout = 10000;
	const int tCout = 4;

	std::thread t1(cmdThread);
	t1.detach();
	/*
	for (int i = 0; i < tCout; i++) {
		std::thread *t1 = new std::thread(sendThread, i + 1, cCout / 4);
		t1->detach();
	}
	*/
	std::thread *t[tCout];
	for (int i = 0; i < tCout; i++) {
		t[i] = new std::thread(sendThread, i + 1, cCout / tCout);
//		t[i]->detach();
	}
	for (int i = 0; i < tCout; i++) {
		t[i]->join();
	}
	for (int i = 0; i < tCout; i++) {
		delete t[i];
	}
	printf("已退出。\n");
	return 0;
}
