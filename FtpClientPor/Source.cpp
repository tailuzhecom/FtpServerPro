//prevent winsock.h (version 1.1)from being included by windows.h  
#ifndef WIN32_LEAN_AND_MEAN  
#define WIN32_LEAN_AND_MEAN  
#endif  
#include <Windows.h>  
#include <winsock2.h>  
#include <ws2tcpip.h>  
#include <iphlpapi.h> // after winsock2.h  
#include <cstdio>  
#include <direct.h>
#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include <vector>
#include <set>
#include <sstream>

#pragma comment (lib, "Ws2_32.lib")  
#pragma comment (lib, "Mswsock.lib")  
#pragma comment (lib, "AdvApi32.lib")  

#define DEFAULT_PORT "27015"  
#define DEFAULT_BUFLEN 512  

using namespace std;


//字符串分割
vector<string> split(string str, string pattern) {
	string::size_type pos;
	vector<string> result;
	str += pattern;
	int size = str.size();

	for (int i = 0; i < size; i++) {
		pos = str.find(pattern, i);
		if (pos < size) {
			string s = str.substr(i, pos - i);
			result.push_back(s);
			i = pos + pattern.size() - 1;
		}
	}
	return result;
}

//接受文件
void RecvFile(SOCKET &connectSocket, const char fileName[DEFAULT_BUFLEN]) {
	//创建文件
	FILE *fp = fopen(fileName, "wb");
	char temp[DEFAULT_BUFLEN];
	int num;
	if (fp == NULL) {
		printf("create file %s failed\n", fileName);
		return;
	}
	while (true) {
		num = recv(connectSocket, temp, DEFAULT_BUFLEN, 0);
		fwrite(temp, 1, num, fp);
		if (num < DEFAULT_BUFLEN) {
			cout << "finished\n" << endl;
			fclose(fp);
			return;
		}
		
	}
	fclose(fp);
}

//发送文件
void SendFile(SOCKET &clientSocket, string &fileName) {
	FILE *fp = fopen(fileName.data(), "rb");
	char temp[DEFAULT_BUFLEN];
	int num = 0;
	if (fp == NULL)
	{
		printf("open file %s failed\n", fileName.data());
		send(clientSocket, "transmission failed\n", sizeof("transmission failed\n") + 1, 0);
		return;
	}
	send(clientSocket, "ok", 100, 0);
	while (!feof(fp))
	{
		num = fread(temp, 1, DEFAULT_BUFLEN, fp);
		send(clientSocket, temp, num, 0);
	}
	fclose(fp);
}


int main()
{
	set<string> commands = { "getfile", "upload", "login", "help", "cd", "pwd", "list", "getfile", "rmfile", "exit", "quit", "rmdir" };
	int iResult = 0;
	WSADATA wsaData;
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	char sendbuf[] = "this is a test for client";
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	char temp[DEFAULT_BUFLEN], file_name[DEFAULT_BUFLEN];
	cout << "please input the ip of the ftpserver you want to connect" << endl;
	cin >> temp;
	// initialize  
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		printf("client WSAStartup failed: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// resolve the server address and port, argv[1] is server name  
	iResult = getaddrinfo(temp, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		printf("client get addrinfor fail: %d\n", iResult);
		WSACleanup(); // terminate use of WS2_32.dll  
		return 1;
	}


	SOCKET ConnectSocket = INVALID_SOCKET;
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		// create a socket for client  
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET)
		{
			printf("client socket failed with error %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// connect to server  
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR)
		{
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;	    // if fail try next address returned by getaddrinfo  
			continue;
		}
		break;
	}

	freeaddrinfo(result);
	if (ConnectSocket == INVALID_SOCKET)
	{
		printf("client unable to connect to server\n");
		WSACleanup();
		return 1;
	}

	int num = 0;
	char str[100];

	recv(ConnectSocket, str, 100, 0);
	cout << str << endl;
	
	vector<string> info;
	string command;
	cin.ignore(1, '\n');
	while (true) {
		cout << "ftp>";
		getline(cin, command);
		send(ConnectSocket, command.data(), sizeof(command), 0);
		recv(ConnectSocket, str, sizeof(str), 0);
		info = split(str, " ");
		if (info[0] == "file") {
			RecvFile(ConnectSocket, info[1].data());
		}
		else if (info[0] == "exit") {
			break;
		}
		else if (info[0] == "upload") {
			SendFile(ConnectSocket, info[1]);
			recv(ConnectSocket, str, 100, 0);
			cout << str << "\n" << endl;
		}
		else if (info[0] == "list") {
			int num = atoi(info[1].data());
			for (int i = 0; i < num; i++) {
				recv(ConnectSocket, str, 100, 0);
				cout << str << endl;
			}
			if (num == 0)
				cout << "empty directory\n" << endl;
		}
		else {
			cout << str << "\n" << endl;
		}
	}
	
	closesocket(ConnectSocket);
	WSACleanup();

	return 0;
}
