#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#ifdef _WIN32
    #define FD_SETSIZE		2505
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
	#include <Windows.h>
	#include <WinSock2.h>
	#pragma comment(lib, "ws2_32.lib")
#else
	#include <unistd.h> // unix std
	#include <arpa/inet.h>
	#include <sys/socket.h>
	#include <string.h>

	#define SOCKET int
	#define INVALID_SOCKET (SOCKET)(~0)
	#define SOCKET_ERROR           (-1)
#endif

#include <stdio.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

#include "MessageHeader.hpp"
#include "CELLTimestamp.hpp"

	// 缓冲区最小单元的大小
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif

class ClientSocket {
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET) : _sockfd(sockfd) , 
					_szMsgBuf(new char[RECV_BUFF_SIZE * 10]) , _lastPos(0) {
		memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
	}

	~ClientSocket() {
		delete[] _szMsgBuf;
	}

	SOCKET getSockfd() { return _sockfd; }

	char *getSzMsgBuf() { return _szMsgBuf; }

	int getLastPos() { return _lastPos; }

	void setLastPos(int pos) {
		_lastPos = pos;
	}

	// 发送数据
	virtual int SendData(DataHeader *header) {
		if (!header) return SOCKET_ERROR;
		return send(_sockfd, (const char *)header, header->dataLength, 0);
	}

private:
	SOCKET _sockfd;
	// 第二缓冲区 消息缓冲区
	char *_szMsgBuf;
	int _lastPos;
};

class INetEvent
{
public:
	virtual void OnNetJoin(ClientSocket *pClient) = 0;
	virtual void OnNetLeave(ClientSocket *pClient) = 0;
	virtual void OnNetMsg(ClientSocket *cSock, DataHeader *header) = 0;
private: 

};


class CellServer
{
public:
	CellServer(SOCKET sock = INVALID_SOCKET) : _sock(sock), _pThread(nullptr), 
												_pNetEvent(nullptr) {}
	~CellServer() {
		delete _pThread;
		Close();
		_sock = INVALID_SOCKET;
	}

	void setEventObj(INetEvent *event) {
		_pNetEvent = event;
	}

	bool __OnRun() {
		if (!isRun()) return false;

		if (_clientsBuff.size() > 0) {
			std::lock_guard<std::mutex> lock(_mutex);
			for (auto pClient : _clientsBuff) {
				_clients.push_back(pClient);
			}
			_clientsBuff.clear();
		}
		if (_clients.empty()) {
			//std::chrono::milliseconds t(1);
			//std::this_thread::sleep_for(1);
			return true;
		}

		fd_set fdRead;

		FD_ZERO(&fdRead);

		SOCKET maxSock = _clients[0]->getSockfd();
		for (size_t i = 0; i < _clients.size(); i++)
		{
			FD_SET(_clients[i]->getSockfd(), &fdRead);
			maxSock = maxSock > _clients[i]->getSockfd() ? maxSock : _clients[i]->getSockfd();
		}

		timeval t = { 0, 0 };
		int ret = select(maxSock + 1, &fdRead, nullptr, nullptr, &t);
		if (ret < 0) {
			printf("selece任务结束。\n");
			Close();
			return false;
		}

		for (int i = (int)_clients.size() - 1; i >= 0; --i)
		{
			if (FD_ISSET(_clients[i]->getSockfd(), &fdRead))
			{
				if (-1 == RecvData(_clients[i]))
				{
					auto iter = _clients.begin() + i;
					if (iter != _clients.end())
					{
						std::lock_guard<std::mutex> lock(_mutex);
						_pNetEvent->OnNetLeave(_clients[i]);
						delete _clients[i];
						_clients.erase(iter);
					}
				}
			}
		}

		return true;
	} 

	void OnRun() {
		while (isRun()) {
			__OnRun();
		}
		return;
	}

	// 是否在在工作中
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}

	// 关闭Socket
	void Close() {
		if (_sock == INVALID_SOCKET) return;
		if (_pThread) {
			delete _pThread;
			_pThread = nullptr;
		}
			
#ifdef _WIN32
		for (size_t i = 0; i < (int)_clients.size(); i++)
		{
			closesocket(_clients[i]->getSockfd());
		}
		// 8.关闭套接字
		// closesocket(_sock);

#else 
		for (size_t i = 0; i < (int)_clients.size(); i++)
		{
			close(_clients[i]->getSockfd());
		}
		close(_sock);
#endif	
		_clients.clear();
	}

	char _szRecv[RECV_BUFF_SIZE] = {};
	// 接收数据 处理粘包 拆分包
	int RecvData(ClientSocket *pClient)
	{
		// 5.接收客户端数据
		int nLen = (int)recv(pClient->getSockfd(), (char *)&_szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0)
		{
			// printf("客户端已退出，任务结束。\n");
			return -1;
		}
		memcpy(pClient->getSzMsgBuf() + pClient->getLastPos(), _szRecv, nLen);
		pClient->setLastPos(pClient->getLastPos() + nLen);

		while (pClient->getLastPos() >= sizeof(DataHeader)) {
			DataHeader *header = (DataHeader *)pClient->getSzMsgBuf();
			if (pClient->getLastPos() >= header->dataLength) {
				int nSize = pClient->getLastPos() - header->dataLength;
				OnNetMsg(pClient, header);
				// 用循环来优化
				memcpy(pClient->getSzMsgBuf(), pClient->getSzMsgBuf() + header->dataLength, 
						pClient->getLastPos() - header->dataLength);
				pClient->setLastPos(nSize); 
			}
			else break;
		}
		return 0;
	}

	virtual void OnNetMsg(ClientSocket *pClient, DataHeader *header) {
		_pNetEvent->OnNetMsg(pClient, header);
	}

	void addClient(ClientSocket *pClient) {
		std::lock_guard<std::mutex> lock(_mutex);
		_clientsBuff.push_back(pClient);
	}

	void Start() {
		_pThread = new std::thread(std::mem_fn(&CellServer::OnRun), this);
	}

	size_t getClientCount() { return _clients.size() + _clientsBuff.size(); }

	size_t getClientsSize() { return _clients.size(); }

	size_t getClientsBuffSize() { return _clientsBuff.size(); }

private:
	SOCKET _sock;
	std::vector<ClientSocket *> _clients;
	std::vector<ClientSocket *> _clientsBuff;
	std::thread *_pThread;
	std::mutex _mutex;
	INetEvent *_pNetEvent;
};


class EasyTcpServer : public INetEvent {
private:
	SOCKET _sock;
	std::vector<CellServer *> _cellServers;
	CELLTimestamp _tTime;
	std::mutex _mutex;

protected:
	std::atomic_int _msgCount;
	std::atomic_int _clientCount;

public:
	EasyTcpServer() : _sock(INVALID_SOCKET), _msgCount(0), _clientCount(0) {}

	~EasyTcpServer() {
		for (int i = 0; i < _cellServers.size(); ++i) {
			delete _cellServers[i];
		}
		_cellServers.clear();
		Close();
	}

	// 初始化Socket
	int initSocket() {
		// 启动Win Sock 2.x的环境 
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock) {
			printf("<socket=%d>关闭旧连接...\n", (int)_sock);
			Close();
		}
		this->_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("错误，建立socket失败...\n");
			return -1;
		}
		printf("建立<socket=%d>成功...\n", (int)_sock);
		return 0;
	}

	// 绑定端口
	int Bing(const char *ip, unsigned short port) {
		if (INVALID_SOCKET == _sock) {
			printf("socket未初始化,已自动初始化...\n");
			initSocket();
		}
		// 2.bing 绑定用于接受客户端链接的网络端口
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);
#ifdef _WIN32
		if(ip) 
			_sin.sin_addr.S_un.S_addr = inet_addr("ip");
		else 
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;
#else	
		if (ip)
			_sin.sin_addr.s_addr = inet_addr("ip");
		else
			_sin.sin_addr.s_addr = INADDR_ANY;
#endif
		if (bind(_sock, (sockaddr *)&_sin, sizeof(sockaddr_in)) == SOCKET_ERROR)
		{
			printf("ERROR, 绑定网络端口<%d>失败...\n", port);
			return -1;
		}
		else {
			printf("绑定网络端口<%d>成功...\n", port);
		}
		return 0;
	}

	// 监听端口号
	int Listen(int n) {
		// 3.listrn 监听网络端口
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)
		{
			printf("Socket<%d> ERROR, 监听网络端口失败...\n", (int)_sock);
			return -1;
		}
		else {
			printf("Socket<%d> 监听网络端口成功...\n", (int)_sock);
		}
		return 0;
	}

	// 接受客户端连接
	SOCKET Accept() {
		// 4.accept 等待接收客户端连接
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr *)&clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr *)&clientAddr, (socklen_t *)&nAddrLen);
#endif
		if (INVALID_SOCKET == cSock)
		{
			printf("socket=<%d>错误，接收到无效客户端SOCKET...\n", (int)_sock);
		}
		else {
			// NewUserJion userJoin;
			// SendDataToAll(&userJoin);
			addClientToCellServer(new ClientSocket(cSock));
		}
		return cSock;
	}

	void addClientToCellServer(ClientSocket *pClient) {
		auto pMinServer = _cellServers[0];
		for (auto pCellServer : _cellServers) {
			if (pMinServer->getClientCount() > pCellServer->getClientCount()) {
				pMinServer = pCellServer;
			}
		}
		pMinServer->addClient(pClient);
		OnNetJoin(pClient);
	}

	void Start(int nCellServer) {
		for (int i = 0; i < nCellServer; i++) {
			auto ser = new CellServer(_sock);
			_cellServers.push_back(ser);
			ser->setEventObj(this);
			ser->Start();
		}
	}

	// 关闭Socket
	void Close() {
		if (_sock == INVALID_SOCKET) return;
#ifdef _WIN32
		// 8.关闭套接字
		closesocket(_sock);
		//
		// 清除Windows socket环境
		WSACleanup();
#else 
		close(_sock);
#endif	
	}

	// 处理网络消息
	bool OnRun() {
		if (!isRun()) return false;
		fd_set fdRead;
		//fd_set fdWrite;
		//fd_set fdExp;

		FD_ZERO(&fdRead);
		//FD_ZERO(&fdWrite);
		//FD_ZERO(&fdExp);

		FD_SET(_sock, &fdRead);
		//FD_SET(_sock, &fdWrite);
		//FD_SET(_sock, &fdExp);
		
		timeval t = { 0, 0 };
		int ret = select(_sock + 1, &fdRead, nullptr, nullptr, &t);
		if (ret < 0) {
			printf("selece任务结束。\n");
			Close();
			return false;
		}
		if (FD_ISSET(_sock, &fdRead))
		{
			FD_CLR(_sock, &fdRead);
			Accept();
			return true;
		}
		time4msg();
		return true;
	}

	// 是否在在工作中
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}

	void time4msg() {
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0) {
			printf("thread<%d>, time<%lf>, socket<%d>, clients<%d>, _recvCount<%d>\n", (int)_cellServers.size(), t1, _sock, _clientCount, _msgCount);
			_tTime.update();
			_msgCount = 0;
		}
		return;
	}

	virtual void OnNetJoin(ClientSocket *pClient) override {
		++_clientCount;
	}

	virtual void OnNetLeave(ClientSocket *pClient) override {
		// delete pClient;
		--_clientCount;
	}

	virtual void OnNetMsg(ClientSocket *cSock, DataHeader *header) override {
		++_msgCount;
	}


};

#endif // !_EasyTcpServer_hpp_
