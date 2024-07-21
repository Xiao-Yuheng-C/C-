#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_

#ifdef _WIN32
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
#include "MessageHeader.hpp"

class EasyTcpClient
{
public:
	EasyTcpClient() : _sock(INVALID_SOCKET), _isConnect(false) {}

	virtual ~EasyTcpClient() {
		Close();
	}

	// 初始化socket
	int initSocket() {
		// 启动Win Sock 2.x的环境 
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock) {
			printf("<socket=%d>关闭旧连接...\n", _sock);
			Close();
		}
		this->_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("错误，建立socket失败...\n");
			return -1;
		}
		// printf("建立<socket=%d>成功...\n", _sock);
		return 0;
	}

	// 连接服务器
	int Connect(const char *ip, short port) {
		if (INVALID_SOCKET == _sock) {
			// printf("<socket=%d>未初始化,已自动初始化...\n", _sock);
			initSocket();
		}

		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);
#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif
		int ret = connect(_sock, (sockaddr *)(&_sin), sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret)
		{
			printf("<Socket=%d>错误，连接服务器<%s:%d>失败...\n", _sock, ip, port);
		}
		else
		{
			_isConnect = true;
			// printf("<Socket=%d>连接服务器<%s:%d>成功...\n", _sock, ip, port);
		}
		return ret;
	}

	// 关闭套接字closesocket
	void Close() {
		if (_sock == INVALID_SOCKET) {
			return;
		}
#ifdef _WIN32
		closesocket(_sock);
		WSACleanup();
#else
		close(_sock);
#endif
		_sock = INVALID_SOCKET;
		_isConnect = false;
		return;
	}

	// 查询网络消息
	bool OnRun() {
		if (!isRun()) return false;
		fd_set fdReads;
		FD_ZERO(&fdReads);
		FD_SET(_sock, &fdReads);
		timeval t = { 0, 0 };
		int ret = select(_sock + 1, &fdReads, 0, 0, &t);
		if (ret < 0)
		{
			printf("<socket=%d>select任务结束1\n", _sock);
			return false;
		}
		if (FD_ISSET(_sock, &fdReads))
		{
			FD_CLR(_sock, &fdReads);
			if (-1 == RecvData())
			{
				printf("<socket=%d>select任务结束2\n", _sock);
				return false;
			}
		}
		return true;
	}

	// 是否工作中
	bool isRun() {
		return _sock != INVALID_SOCKET && _isConnect;
	}



	// 缓冲区最小单元的大小
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif
	// 接收缓冲区
	char _szRecv[RECV_BUFF_SIZE] = {};
	// 第二缓冲区 消息缓冲区
	char _szMsgBuf[RECV_BUFF_SIZE * 10] = {};
	int _lastPos = 0;
	// 接收数据 处理粘包 拆分包
	int RecvData()
	{
		// 5.接收客户端数据
		int nLen = (int)recv(_sock, (char *)&_szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0)
		{
			printf("<Socket=%d>与服务器断开连接，任务结束。\n", _sock);
			return -1;
		}
		memcpy(_szMsgBuf + _lastPos, _szRecv, nLen);
		_lastPos += nLen;

		while (_lastPos >= sizeof(DataHeader)) {
			DataHeader *header = (DataHeader *)_szMsgBuf;
			if (_lastPos >= header->dataLength) {
				int nSize = _lastPos - header->dataLength;
				OnNetMsg(header);
				// 用循环来优化
				memcpy(_szMsgBuf, _szMsgBuf + header->dataLength, _lastPos - header->dataLength);
				_lastPos = nSize;
			}
			else break;
		}
		return 0;
	}

	// 响应网络消息
	virtual void OnNetMsg(DataHeader *header) {
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{
			LoginResult *loginresult = (LoginResult *)header;
			// printf("<Socket=%d>收到服务端消息：CMD_LOGIN_RESULT，数据长度: %d\n", _sock, header->dataLength);
		}
		break;
		case CMD_LOGOUT_RESULT:
		{
			LogoutResult *logoutresult = (LogoutResult *)header;
			// printf("<Socket=%d>收到服务端消息：CMD_LOGOUT_RESULT，数据长度: %d\n", _sock, header->dataLength);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			NewUserJion *userJoin = (NewUserJion *)header;
			// printf("<Socket=%d>收到服务端消息：CMD_NEW_USER_JOIN，数据长度: %d\n", _sock, header->dataLength);
		}
		break;
		case CMD_ERROR:
		{
			printf("<Socket=%d>收到服务端消息：CMD_ERROR，数据长度: %d\n", _sock, header->dataLength);
		}
		break;
		default:
		{
			printf("<Socket=%d>收到未定义消息，数据长度: %d\n", _sock, header->dataLength);
		}
		break;
		}
	}

	// 发送数据
	virtual int SendData(DataHeader *header) {
		if (!isRun() || !header) return SOCKET_ERROR;
		return send(_sock, (const char *)header, header->dataLength * 1, 0); 
	}

	SOCKET getSocket() { return _sock;  }

private:
	SOCKET _sock;
	bool _isConnect;
};


#endif