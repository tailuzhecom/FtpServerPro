#ifndef WIN32_LEAN_AND_MEAN  
#define WIN32_LEAN_AND_MEAN  
#endif  

#include <string>
#include <Windows.h>  
#include <WinSock2.h>  
#include <WS2tcpip.h>  
#include <IPHlpApi.h>  
#include <cstdio>  
#include <vector>
#include <fstream>
#include <direct.h>
#include <iostream>
#include <map>
#include <thread>
#include <set>
#include <iomanip>
#include <sstream>
#include <io.h>
#include <mutex>
#include <string.h>

#pragma comment (lib, "Ws2_32.lib")  

#define bzero(a, b) memset(a, 0, b)
#define DEFAULT_PORT "10086"  
#define DEFAULT_BUFLEN 512  

using namespace std;

//记录在线用户
set<string> onlineUser;

mutex mtx;

//command help
map<string, string> help = { {"get", " get + [filename]     function: download file[filename] in the current path"},
							{"user", " user + [username]        function: user login"},
							{"send", " send + [filename]       function: upload file[filename] in the current path"},
							{"list", " function: list the file in the current in the current path"},
							{ "dir", " function: list the file in the current in the current path" },
							{"quit", " function: close the ftp client you are running"},
							{ "exit", " function: close the ftp client you are running" },
							{ "bye", " function: close the ftp client you are running" },
							{"register", " register + [username]      function: create a new account"},
							{"cwd", " cwd + [targetdir]     function: change the current path to target directory"},
							{"pwd", " function: print the current path"},
							{"all", " all commands:\ncd exit getfile list login pwd quit register upload"},
							{"rmd", " rmd + [dirname]       function: remove directory"},
							{"dele", " dele + [filename]       function: remove file"} };


//返回当前时间字符串
string GetCurrentTime_() {
	time_t tm;
	time(&tm);
	char tmp[128] = { NULL };
	strcpy(tmp, ctime(&tm));
	return static_cast<string>(tmp);
}


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

//日志
void Log(string info) {
	info += "       " + GetCurrentTime_();
	cout << info << endl;
	ofstream ofs;
	//mtx.lock();
	ofs.open("log.txt", ios::app);
	ofs << info << endl;
	ofs.close();
	//mtx.unlock();
}

//接受文件
bool RecvFile(SOCKET &connectSocket, const char fileName[DEFAULT_BUFLEN], string userName) {
	//创建文件
	FILE *fp = fopen(fileName, "wb");
	char temp[DEFAULT_BUFLEN];
	int num;
	if (fp == NULL) {
		Log(userName + " failed to upload the file " + string(fileName));
		printf("create file %s failed\n", fileName);
		return false;
	}
	while (true) {
		num = recv(connectSocket, temp, DEFAULT_BUFLEN, 0);
		fwrite(temp, 1, num, fp);
		if (num < DEFAULT_BUFLEN) {
			fclose(fp);
			Log(userName + " upload the file " + string(fileName) + " successfully");
			return true;
		}

	}
	fclose(fp);
}

//登录
inline bool UserLogin(const string &user, string passwd, map<string, string> &userList, SOCKET &clientSocket, string &currentPath) {
	passwd = passwd.substr(0, passwd.size() - 1);
	if (userList[user] == passwd || user == "anonymous") {
		if (user != "anonymous" && onlineUser.count(user)) {
			send(clientSocket, "please don't repeat loging in", 100, 0);
			return false;
		}
		Log(user + " login");
		currentPath = user + "\\";
		send(clientSocket, "230 User logged in, proceed.", 100, 0);
		onlineUser.insert(user);
		return true;
	}
	else {
		send(clientSocket, "login failed", sizeof("login failed") + 1, 0);
		return false;
	}
}

//发送文件
void SendFile(SOCKET &clientSocket, string &fileName) {
	FILE *fp = fopen(fileName.data(), "rb");
	char temp[DEFAULT_BUFLEN];
	int num = 0;
	string realFileName = split(fileName, "\\").back();
	if (fp == NULL)
	{
		printf("open file %s failed\n", realFileName.data());
		Log(split(fileName, "\\")[0] + " failed to download the file " + fileName);
		send(clientSocket, "transmission failed", sizeof("transmission failed") + 1, 0);
		return;
	}
	char str[100] = "file ";
	strcat(str, realFileName.data());
	send(clientSocket, str, 100, 0);
	while (!feof(fp))
	{
		num = fread(temp, 1, DEFAULT_BUFLEN, fp);
		send(clientSocket, temp, num, 0);
	}
	this_thread::sleep_for(std::chrono::milliseconds(100));
	send(clientSocket, "226 Transfer complete.", 100, 0);  //延迟防止因为消息和文件发送间隔过短而导致的缓冲区尚未发送出去等待过程中消息与文件信息合并一起send出去
	Log(split(fileName, "\\")[0] + " download the file " + fileName + " successfully");
	fclose(fp);
}

//载入用户登录资料
inline void LoadUserList(map<string, string> &userList) {
	ifstream fs("login.txt");
	string user, passwd;
	while (fs >> user >> passwd) {
		userList[user] = passwd;
	}
	fs.close();
}


//用户注册
inline void CreateUser(const string &userName, const string &passwd, map<string, string> &userList, SOCKET clientSocket) {
	if (userList[userName].empty()) {
		userList[userName] = passwd.substr(0, passwd.size() - 1);
		//创建用户文件夹
		_mkdir(userName.data());
		//login文件加入新用户信息
		ofstream fs;
		fs.open("login.txt", ios::app);
		fs << "\n" << userName << " " << passwd;
		fs.close();
		Log("create a new account " + userName);
		send(clientSocket, "220 Service ready for newuser.", 100, 0);
	}
	else
		send(clientSocket, "454 This username has been registered!", 100, 0);
}

//time_t to string
int TimeToString(string &strDateStr, const time_t &timeData)
{
	char chTmp[15];
	bzero(chTmp, sizeof(chTmp));

	struct tm *p;
	p = localtime(&timeData);
	p->tm_year = p->tm_year + 1900;
	p->tm_mon = p->tm_mon + 1;
	snprintf(chTmp, sizeof(chTmp), "%04d-%02d-%02d",
		p->tm_year, p->tm_mon, p->tm_mday);

	strDateStr = chTmp;
	return 0;
}

//遍历一个文件夹下的文件
bool TraverseFiles(string path, vector<string> &vec, int flag) {
	_finddata_t file_info;
	string current_path = path + "/*.*";
	int handle = _findfirst(current_path.c_str(), &file_info);
	if (-1 == handle)
		return false;
	do {
		string attribute;
		if (file_info.attrib == _A_SUBDIR)
			attribute = "dir";
		else
			attribute = "file";
		cout.width(30);
		cout.setf(ios::left);
		stringstream sst;
		string modifyTime;
		TimeToString(modifyTime, static_cast<time_t>(file_info.time_write));
		if (strcmp(file_info.name, ".") && strcmp(file_info.name, "..")) {     //其中文件中的. 和 ..
			if (flag == 0) {
				sst << attribute << ' ' << file_info.name << ' ' << modifyTime << ' ' << file_info.size << " bytes" << endl;
				vec.push_back(sst.str());
			}
			else if (flag == 1 && attribute == "dir") {    //当flag为1时只返回文件夹
				vec.push_back(file_info.name);
			}
		}
	} while (!_findnext(handle, &file_info));
	_findclose(handle);
	return true;
}

//向用户发送文件列表
void GetFileList(SOCKET clientSocket, string currentPath) {
	vector<string> fileListVec;
	TraverseFiles(currentPath, fileListVec, 0);
	send(clientSocket, ("list " + to_string(static_cast<int>(fileListVec.size()))).data(), 100, 0);
	for (auto i : fileListVec)
		send(clientSocket, i.data(), 100, 0);
	send(clientSocket, "226 Directory send OK.", 100, 0);
	Log(split(currentPath, "\\")[0] + " get the file list in the path: " + currentPath);
}

//改变用户当前路径
void ChangeDirectory(string &currentPath, string &targetDir, SOCKET clientSocket) {
	if (targetDir == "..") {  //返回上一层目录
		vector<string> str;
		str = split(currentPath, "\\");
		if (str.size() > 2) {
			currentPath = "";
			for (int i = 0; i < str.size() - 2; i++)
				currentPath += str[i] + "\\";
			send(clientSocket, ("200 Directory changed to " + currentPath).data(), 100, 0);
		}
		else {
			send(clientSocket, ("200 Directory changed to " + currentPath).data(), 100, 0);
		}
	}
	else if (targetDir == "~") {  //返回根目录
		currentPath = split(currentPath, "\\")[0] + "\\";
		send(clientSocket, ("200 Directory changed to " + currentPath).data(), 100, 0);
	}
	else {     //进入target目录
		vector<string> vec;
		TraverseFiles(currentPath, vec, 1);
		bool flag = false;
		for (auto i : vec)
			if (i == targetDir)
				flag = true;
		if (flag) {
			currentPath += targetDir + "\\";
			send(clientSocket, ("200 Directory changed to " + currentPath).data(), 100, 0);
		}
		else
			send(clientSocket, "no such directory", 100, 0);
	}
	Log(split(currentPath, "\\")[0] + " cd to the path : " + currentPath);
}


//socket connection
void Conn(SOCKET clientSocket, map<string, string> userList) {
	int loginStatus = 0;
	string userName = "";
	string currentPath = "";
	if (clientSocket == INVALID_SOCKET)
	{
		printf("server accept failed: %ld\n", WSAGetLastError());
		return;
	}
	char ss[100];
	send(clientSocket, "220 Ftp Server ready...\r\n", 100, 0);
	char command[100];
	vector<string> info;
	while (true) {
		try {
			recv(clientSocket, command, sizeof(command), 0);
		}
		catch (exception) {
			break;
		}
		string comStr(command);
		info = split(command, " ");
		if (info[0] == "user" && (info.size() == 2 || info[1] == "anonymous" && info.size() == 2)) {       //登录
			send(clientSocket, "200 PORT command successful.", 100, 0);
			if (!loginStatus) {
				if (info.size() == 2)
					info.push_back("");
				char pass[100];
				if (userList.count(info[1]) || info[1] == "anonymous") {
					send(clientSocket, "331 User name okay, need password.", 100, 0);
					recv(clientSocket, pass, 100, 0);
					if (UserLogin(info[1], pass, userList, clientSocket, currentPath)) {
						userName = info[1];
						loginStatus = 1;
					}
				}
				else
					send(clientSocket, "220 This account isn't exist.", 100, 0);
			}
			else
				send(clientSocket, "don't attempt to login more than one account in one ftp client program", 100, 0);
		}
		else if (info[0] == "register" && info.size() == 2) {     //用户注册
			send(clientSocket, "200 PORT command successful.", 100, 0);
			if (!loginStatus) {
				char pass[100];
				if (!userList.count(info[1]) && info[1] != "anonymous") {
					send(clientSocket, "331 User name okay, need password.", 100, 0);
					recv(clientSocket, pass, 100, 0);
					CreateUser(info[1], pass, userList, clientSocket);
				}
				else
					send(clientSocket, "220 User name was used.", 100, 0);
			}
			else
				send(clientSocket, "don't attempt to create a new account after logging in", 100, 0);
		}
		else if ((info[0] == "exit" || info[0] == "quit" || info[0] == "bye") && info.size() == 1) {   //用户退出
			send(clientSocket, "200 PORT command successful.", 100, 0);
			send(clientSocket, "221 Goodbye.", 100, 0);
			if (userName != "")
				Log(userName + " log out");
			onlineUser.erase(userName);
			break;
		}
		else {
			if (loginStatus) {   //是否已登录
				if (info[0] == "get" && info.size() == 2) {    //下载
					send(clientSocket, "200 PORT command successful.", 100, 0);
					SendFile(clientSocket, currentPath + info[1]);
				}
				else if (info[0] == "send" && info.size() == 2) {    //上传
					send(clientSocket, "200 PORT command successful.", 100, 0);
					send(clientSocket, "150 Ok to send data.", 100, 0);
					char temp[105];
					recv(clientSocket, temp, 100, 0);
					if (strcmp(temp, "ok") == 0 && RecvFile(clientSocket, (userName + "\\" + info[1]).data(), userName))
						send(clientSocket, "226 Transfer complete.", 100, 0);
					else
						send(clientSocket, "upload failed", 100, 0);
				}
				else if ((info[0] == "list" || info[0] == "dir") && info.size() == 1) {   //获取文件列表
					send(clientSocket, "200 PORT command successful.", 100, 0);
					GetFileList(clientSocket, currentPath);
				}
				else if (info[0] == "pwd" && info.size() == 1) {  //打印当前目录
					send(clientSocket, "200 PORT command successful.", 100, 0);
					send(clientSocket, currentPath.data(), 100, 0);
				}
				else if (info[0] == "cwd" && info.size() == 2) {   //改变当前目录
					send(clientSocket, "200 PORT command successful.", 100, 0);
					ChangeDirectory(currentPath, info[1], clientSocket);
				}
				else if (info[0] == "help" && info.size() == 2) {  //帮助
					send(clientSocket, "200 PORT command successful.", 100, 0);
					if (!help[info[1]].empty())
						send(clientSocket, help[info[1]].data(), 100, 0);
					else
						send(clientSocket, "This command isn't exist", 100, 0);
				}
				else if (info[0] == "all" && info.size() == 1) {
					send(clientSocket, "200 PORT command successful.", 100, 0);
					send(clientSocket, "USER GET SEND PWD CWD HELP ALL LIST DIR REGISTER MKD RMD DELE BYE QUIT EXIT", 100, 0);
				}
				else if (info[0] == "mkd" && info.size() == 2) {   //创建文件夹
					send(clientSocket, "200 PORT command successful.", 100, 0);
					if (!_mkdir((currentPath + info[1]).data())) {
						Log(userName + " makes directory " + currentPath + " " + info[1]);
						send(clientSocket, ("521 \"" + currentPath + info[1] + "\" directory created").data(), 100, 0);
					}
					else
						send(clientSocket, "521 taking no action.", 100, 0);
				}
				else if (info[0] == "dele" && info.size() == 2) {       //删除文件
					send(clientSocket, "200 PORT command successful.", 100, 0);
					if (!remove((currentPath + info[1]).data())) {
						Log(userName + " remove file " + currentPath + " " + info[1]);
						send(clientSocket, "250 Delete file success.", 100, 0);
					}
					else
						send(clientSocket, "500 No such file.", 100, 0);
				}
				else if (info[0] == "rmd" && info.size() == 2) {     //删除文件夹  _rmdir 和 remove 返回0时代表删除成功
					send(clientSocket, "200 PORT command successful.", 100, 0);
					if (!_rmdir((currentPath + info[1]).data())) {
						Log(userName + " remove directory " + currentPath + " " + info[1]);
						send(clientSocket, "250 Remove directory success.", 100, 0);
					}
					else
						send(clientSocket, "500 No such directory.", 100, 0);
				}
				else if (help[info[0]] == "") {  //不存在该命令
					send(clientSocket, "No this command.", 100, 0);
				}
			}
			else if (info[0] != "user" && info[0] != "register" && !help[info[0]].empty()) {
				send(clientSocket, "You are not logged in.", 100, 0);
			}
			else if (help[info[0]] == "") {  //不存在该命令
				send(clientSocket, "No this command.", 100, 0);
			}
		}
	}
	int iResult = shutdown(clientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR)
	{
		printf("server shutdown failed %ld\n", WSAGetLastError());
		closesocket(clientSocket);
		WSACleanup();
		return;
	}
}


int main() {
	map<string, string> userList;
	LoadUserList(userList);
	int iResult = 0, iSendResult = 0;
	WSADATA wsaData;
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	char temp[DEFAULT_BUFLEN] = "";
	int loginStatus = 0;
	vector<SOCKET> socketList;
	vector<thread> threads;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		printf("server WSAStartup failed: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE; // caller to bind  

	// resolve the local address and port to be used by user  
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		printf("server getaddrinfo faild: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// create a socket for server  
	SOCKET ListenSocket = INVALID_SOCKET;
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET)
	{
		printf("server failed at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// bind a socket  
	iResult = ::bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		printf("server bind faild: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// once bind, no longer needed  
	freeaddrinfo(result);

	// listen on a socket  
	if (listen(ListenSocket, SOMAXCONN))
	{
		printf("server listen socket failed %ld\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	// accept a connection  
	sockaddr_in client_addr;
	int nlen = 0;
	while (true) {
		SOCKET ClientSocket = INVALID_SOCKET;
		ClientSocket = accept(ListenSocket, NULL, NULL);
		threads.push_back(thread(Conn, ClientSocket, userList));
	}
	for (int i = 0; i < threads.size(); i++)
		threads[i].join();
	closesocket(ListenSocket);
	WSACleanup();

	return 0;
}
