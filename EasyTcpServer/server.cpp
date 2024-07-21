#include "EasyTcpServer.hpp"
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
			printf("�˳�cmdThread�߳�\n");
			break;
		}
		else {
			printf("��֧�ֵ�����\n");
		}
	}
	return;
}

class MyServer : public EasyTcpServer
{
public:
	virtual void OnNetJoin(ClientSocket *pClient) override {
		++_clientCount;
		printf("client<%d> join\n", pClient->getSockfd());
	}

	virtual void OnNetLeave(ClientSocket *pClient) override {
		--_clientCount;
		printf("client<%d> leave\n", pClient->getSockfd());
	}

	virtual void OnNetMsg(ClientSocket *cSock, DataHeader *header) override {
		
	}

private:

};

int main()
{
	EasyTcpServer server;
	server.initSocket();
	server.Bing(nullptr, 4567);
	server.Listen(5);
	server.Start(4);

	std::thread t1(cmdThread); 
	t1.detach();

	while (g_bRun)
	{
		server.OnRun();
	}
	
	printf("���˳������������\n");
	getchar();
	return 0;
}