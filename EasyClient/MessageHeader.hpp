enum CMD {
	CMD_LOGIN,
	CMD_LOGIN_RESULT,
	CMD_LOGOUT,
	CMD_LOGOUT_RESULT,
	CMD_NEW_USER_JOIN,
	CMD_ERROR
};

class DataHeader {
public:
	DataHeader(short _dataLenth = sizeof(DataHeader), short _cmd = CMD_ERROR) : dataLength(_dataLenth), cmd(_cmd) {}

	short dataLength;
	short cmd;
};

class Login : public DataHeader {
public:
	Login() : DataHeader(sizeof(Login), CMD_LOGIN) {}

	char userName[32];
	char userWord[32];
	char data[32];
};

class LoginResult : public DataHeader {
public:
	LoginResult(int _result = 0) : DataHeader(sizeof(LoginResult), CMD_LOGIN_RESULT)
		, result(_result) {}

	int result;
	char data[92];
};

class Logout : public DataHeader {
public:
	Logout() : DataHeader(sizeof(Logout), CMD_LOGOUT) {}

	char userName[32];
};

class LogoutResult : public DataHeader {
public:
	LogoutResult(int _result = 0) : DataHeader(sizeof(LogoutResult), CMD_LOGOUT_RESULT)
		, result(_result) {}

	int result;
};

class NewUserJion : public DataHeader {
public:
	NewUserJion(int _sock = 0) : DataHeader(sizeof(NewUserJion), CMD_NEW_USER_JOIN)
		, sock(_sock) {}

	int sock;
};
