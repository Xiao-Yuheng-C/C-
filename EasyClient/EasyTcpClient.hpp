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

	// ��ʼ��socket
	int initSocket() {
		// ����Win Sock 2.x�Ļ��� 
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock) {
			printf("<socket=%d>�رվ�����...\n", _sock);
			Close();
		}
		this->_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("���󣬽���socketʧ��...\n");
			return -1;
		}
		// printf("����<socket=%d>�ɹ�...\n", _sock);
		return 0;
	}

	// ���ӷ�����
	int Connect(const char *ip, short port) {
		if (INVALID_SOCKET == _sock) {
			// printf("<socket=%d>δ��ʼ��,���Զ���ʼ��...\n", _sock);
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
			printf("<Socket=%d>�������ӷ�����<%s:%d>ʧ��...\n", _sock, ip, port);
		}
		else
		{
			_isConnect = true;
			// printf("<Socket=%d>���ӷ�����<%s:%d>�ɹ�...\n", _sock, ip, port);
		}
		return ret;
	}

	// �ر��׽���closesocket
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

	// ��ѯ������Ϣ
	bool OnRun() {
		if (!isRun()) return false;
		fd_set fdReads;
		FD_ZERO(&fdReads);
		FD_SET(_sock, &fdReads);
		timeval t = { 0, 0 };
		int ret = select(_sock + 1, &fdReads, 0, 0, &t);
		if (ret < 0)
		{
			printf("<socket=%d>select�������1\n", _sock);
			return false;
		}
		if (FD_ISSET(_sock, &fdReads))
		{
			FD_CLR(_sock, &fdReads);
			if (-1 == RecvData())
			{
				printf("<socket=%d>select�������2\n", _sock);
				return false;
			}
		}
		return true;
	}

	// �Ƿ�����
	bool isRun() {
		return _sock != INVALID_SOCKET && _isConnect;
	}



	// ��������С��Ԫ�Ĵ�С
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif
	// ���ջ�����
	char _szRecv[RECV_BUFF_SIZE] = {};
	// �ڶ������� ��Ϣ������
	char _szMsgBuf[RECV_BUFF_SIZE * 10] = {};
	int _lastPos = 0;
	// �������� ����ճ�� ��ְ�
	int RecvData()
	{
		// 5.���տͻ�������
		int nLen = (int)recv(_sock, (char *)&_szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0)
		{
			printf("<Socket=%d>��������Ͽ����ӣ����������\n", _sock);
			return -1;
		}
		memcpy(_szMsgBuf + _lastPos, _szRecv, nLen);
		_lastPos += nLen;

		while (_lastPos >= sizeof(DataHeader)) {
			DataHeader *header = (DataHeader *)_szMsgBuf;
			if (_lastPos >= header->dataLength) {
				int nSize = _lastPos - header->dataLength;
				OnNetMsg(header);
				// ��ѭ�����Ż�
				memcpy(_szMsgBuf, _szMsgBuf + header->dataLength, _lastPos - header->dataLength);
				_lastPos = nSize;
			}
			else break;
		}
		return 0;
	}

	// ��Ӧ������Ϣ
	virtual void OnNetMsg(DataHeader *header) {
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{
			LoginResult *loginresult = (LoginResult *)header;
			// printf("<Socket=%d>�յ��������Ϣ��CMD_LOGIN_RESULT�����ݳ���: %d\n", _sock, header->dataLength);
		}
		break;
		case CMD_LOGOUT_RESULT:
		{
			LogoutResult *logoutresult = (LogoutResult *)header;
			// printf("<Socket=%d>�յ��������Ϣ��CMD_LOGOUT_RESULT�����ݳ���: %d\n", _sock, header->dataLength);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			NewUserJion *userJoin = (NewUserJion *)header;
			// printf("<Socket=%d>�յ��������Ϣ��CMD_NEW_USER_JOIN�����ݳ���: %d\n", _sock, header->dataLength);
		}
		break;
		case CMD_ERROR:
		{
			printf("<Socket=%d>�յ��������Ϣ��CMD_ERROR�����ݳ���: %d\n", _sock, header->dataLength);
		}
		break;
		default:
		{
			printf("<Socket=%d>�յ�δ������Ϣ�����ݳ���: %d\n", _sock, header->dataLength);
		}
		break;
		}
	}

	// ��������
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