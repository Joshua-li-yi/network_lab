/**********************************
说明：该部分完成了本次作业要求中的3-1&3-2部分，实现了
面向连接、差错检测、确认重传、停等机制、滑动窗口(SR方法)、累积确认等功能
详细说明见实验报告
***********************************/
// #define _CRT_SECURE_NO_WARNINGS
#include "define.h"
#pragma comment(lib, "ws2_32.lib")
using namespace std;

#define SERVER_IP "127.0.0.1" //服务器IP地址
#define SERVER_PORT 8888	  //服务器端口号

SOCKADDR_IN addrServer; //服务器地址
SOCKADDR_IN addrClient; //客户端地址
SOCKET sockServer;

const int SEQNUMBER = 2; //序列号的个数

int length = sizeof(SOCKADDR);
char buffer[BUFFER];						  //数据发送接收缓冲区
char RetransmissionBuffer[SEQNUMBER][BUFFER]; //选择重传缓冲区
int totalpacket;							  //总的数据包个数
int totalseq;								  //收到的包的个数
int ack[SEQNUMBER];							  //用数组存储收到ack的情况
int totalsend = 0;							  // 总发送量
int totalrecv = 0;							  // 总接受量

char sndpkt[BUFFER];
int base = 1;
int nextseqnum = 1;

//超时重传处理函数 重传没有收到ack的数据包
void IfTimeout(Timer *t)
{
	if (t->TimeOut())
	{
		printf("Timer out error.\n");
		t->Start();
		// for (int i = 0; i < nextseqnum - base; ++i)
		sendto(sockServer, sndpkt, strlen(sndpkt), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
	}
}
// 发送DataPackage
void rdt_send(DataPackage *data, Timer *t)
{
	if (nextseqnum < base + WINDOWSIZE)
	{
		sendto(sockServer, (char *)data, sizeof(DataPackage) + data->len, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
		t->Start();
		nextseqnum++;
	}
}
int main(int argc, char *argv[])
{

	// 解决乱码现象
	setlocale(LC_CTYPE, "");
	//加载套接字库（必须）
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		//找不到 winsock.dll
		printf("WSAStartup failed with error: %d\n", err);
		return -1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else
	{
		printf("The Winsock 2.2 dll was found okay\n");
	}

	sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//设置套接字为非阻塞模式
	int iMode = 1;											//1：非阻塞，0：阻塞
	ioctlsocket(sockServer, FIONBIO, (u_long FAR *)&iMode); //非阻塞设置
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	err = bind(sockServer, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
	if (err)
	{
		err = GetLastError();
		printf("Could  not  bind  the  port  %d  for  socket.Error  code is %d\n", SERVER_PORT, err);
		WSACleanup();
		return -1;
	}
	// 清空内存
	ZeroMemory(buffer, sizeof(buffer));
	string schema = "test";
	cout << "please input schema [test] or transport [data]: " << endl;
	// cin >> schema;
	if (schema == "data")
	{
	}

	for (int i = 0; i < SEQNUMBER; i++)
		ack[i] = 0; //初始都标记为1

	int recvSize;
	bool is_connect = false; // 判断是否连接
	Timer *t = new Timer();	 // 计时器
	Timer *totalT = new Timer();
	ifstream *is = new ifstream(); // 读文件
	File *sendFile = new File();   // 文件操作
	unsigned short offset = 0;	   // 发送文件offset
	bool logout = false;		   // 是否登出
	bool runFlag = true;		   //是否运行
	streampos pos;				   //文件光标位置，用于记录上一次光标的位置
	bool first_open_file = true;   // 是否是第一次打开该文件
	while (!logout)
	{
		if (!is_connect)
			cout << "wait connect with client ..." << endl;
		// Sleep(200);
		recvSize = recvfrom(sockServer, buffer, BUFFER, 0, ((SOCKADDR *)&addrClient), &length);

		// 等待连接
		int i = 0;
		if (recvSize < 0)
		{
			i++;
			Sleep(500);
		}
		if (i > 20)
		{
			printf("can't connect\n exit program ...");
			exit(1);
		}
		int waitCount = 0;
		//握手建立连接阶段
		//服务器收到客户端发来的TAG=0的数据报，标识请求连接
		//服务器向客户端发送一个 100（ASCII） 大小的状态码，表示服务器准备好了，可以发送数据
		//客户端收到 100 之后回复一个 200 大小的状态码，表示客户端准备好了，可以接收数据了
		//服务器收到 200 状态码之后，就开始发送数据了
		if (strcmp(buffer, "0") == 0)
		{
			ZeroMemory(buffer, sizeof(buffer));
			cout << "stage 0: find client, begin connect ..." << endl;
			int stage = 0; //初始化状态

			while (runFlag)
			{
				switch (stage)
				{
				case 0: //发送 S1 阶段
				{
					cout << "send S1 to client ..." << endl;
					buffer[0] = S1;
					sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
					Sleep(100);
					stage = 1;
					break;
				}
				case 1: //等待接收 S2 阶段，没有收到则计数器+1，超时则放弃此次“连接”，等待从第一步开始
				{
					cout << "stage 1: wait S2 from client ..." << endl;
					recvSize = recvfrom(sockServer, buffer, BUFFER, 0, ((SOCKADDR *)&addrClient), &length);
					if (recvSize < 0)
					{
						waitCount++;
						Sleep(100);
						if (waitCount > 50)
						{
							printf("connect fail! try reconnect ...\n");
							break;
						}
					}
					else if ((unsigned char)buffer[0] == S2)
					{
						printf("get S2 from client, connect with client !!!\n");
						is_connect = true; // 客户端与服务器已经连接
						stage = 2;
					}
					break;
				}
				case 2: //文件选择阶段
				{
					cout << "==========file transport===========" << endl;
					string filePath;
					cout << "Please input file path which needs to be send: " << endl;
					cin >> filePath;
					if (filePath == "-1")
					{
						filePath = ".\\bin\\1.txt";
						cout << "the default file path is " << filePath << endl;
					}
					sendFile->initFile(true, filePath);
					sendFile->ReadFile();
					first_open_file = true;

					buffer[0] = S3;
					sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));

					stage = 3;
					break;
				}

				case 3:
				{
					recvSize = recvfrom(sockServer, buffer, BUFFER, 0, ((SOCKADDR *)&addrClient), &length);
					if ((unsigned char)buffer[0] == S4)
					{
						printf("get S4 from client !!!\n");
						printf("this client has select download path begin tansport this file !!!\n");
						stage = 4;
					}
					break;
				}
				case 4:
				{
					// 文件小，直接一个包发过去，文件大需要多次发
					// 文件长度只有package时
					cout << "begin transport file ..." << endl;
					if (sendFile->packageSum == 1)
					{
						totalT->Start();
						// 读取文件中内容到 message里， dataPackage发过去，然后检测是否会收到ACK，如果有就说明，发送完毕，
						// 如果没有就超时重传
						char *tmp = new char[sendFile->fileLen + 1];
						// 打开文件
						is->open(sendFile->filePath, ifstream::in | ios::binary);
						// 读取文件内容
						is->read(tmp, sendFile->fileLen);
						// 关闭文件
						is->close();

						tmp[sendFile->fileLen] = '\0';

						DataPackage *data = (DataPackage *)malloc(sizeof(DataPackage) + (sendFile->fileLen + 1) * sizeof(char));
						Strcpy(data->message, tmp);

						data->make_pkt(SERVER_PORT, SERVER_PORT, nextseqnum, WINDOWSIZE);
						data->CheckSum((unsigned short *)data->message);
						data->len = sendFile->fileLen + 1;
						data->flag = 0;
						data->offset = 0;
						cout << "msg: " << data->message << endl;
						Strcpy(sndpkt, (char *)data);
						rdt_send(data, t);
						delete data;
						cout << "send success!!!" << endl;
						while (true)
						{
							ZeroMemory(buffer, sizeof(buffer));
							recvSize = recvfrom(sockServer, buffer, BUFFER, 0, ((SOCKADDR *)&addrClient), &length);
							if (recvSize < 0)
							{
								IfTimeout(t);
							}
							else
							{
								cout << "get ack!!!" << endl;
								DataPackage *tmpData = new DataPackage();
								extract_pkt(buffer, *tmpData);
								if (tmpData->ackflag & (tmpData->ackNum == nextseqnum - 1) & (!corrupt(tmpData)))
								{
									cout << "ack success!!!" << endl;
									totalT->Show();
									stage = 5;
									ack[nextseqnum - 1 - base] = 1;
									base = nextseqnum;
									break;
								}
								else
								{
									cout << "something error" << endl;
									IfTimeout(t);
								}
								delete tmpData;
							}
						}
					}
					else // 文件需要多次传输
					{
						// 如果文件发完了就退出循环
						if (offset == sendFile->packageSum)
						{
							totalT->Show();
							stage = 5;
							offset = 0;
							break;
						}
						// 打开文件
						is->open(sendFile->filePath, ifstream::in | ios::binary);
						if (first_open_file) // 如果是第一次打开文件
						{
							first_open_file = false;
							totalT->Start();
						}
						else
						{ // 如果不是第一次就跳到上一次的位置
							is->seekg(pos);
						}
						DataPackage *data = (DataPackage *)malloc(sizeof(DataPackage) + (BUFFER - sizeof(DataPackage)) * sizeof(char));
						// 读取文件内容
						if (offset <= sendFile->packageSum - 2) // 如果文件不是最后一次读取
						{
							char *tmp = new char[BUFFER - sizeof(DataPackage)];
							is->read(tmp, BUFFER - sizeof(DataPackage) - 1);
							tmp[BUFFER - sizeof(DataPackage) - 1] = '\0';
							Strcpy(data->message, tmp);
							delete tmp;
							data->len = BUFFER - sizeof(DataPackage);
						}
						else // 如果文件是最后一次读取
						{
							char *tmp = new char[sendFile->fileLenRemain + 1];
							is->read(tmp, sendFile->fileLenRemain);
							tmp[sendFile->fileLenRemain] = '\0';
							Strcpy(data->message, tmp);
							delete tmp;
							data->len = sendFile->fileLenRemain + 1;
						}
						pos = is->tellg(); // 保存文件当前位置
						is->close();
						// cout << "msg: " << tmp << endl;
						data->make_pkt(SERVER_PORT, SERVER_PORT, nextseqnum, WINDOWSIZE);
						data->CheckSum((unsigned short *)data->message);
						data->flag = sendFile->packageSum;
						data->offset = offset;
						offset++;
						cout << data->offset << endl;
						// 复制到缓冲区
						Strcpy(sndpkt, (char *)data);

						cout << "file need send " << sendFile->packageSum << " times" << endl;
						printf("send %d segment file ...\n", data->offset);
						rdt_send(data, t);
						delete data;
						cout << "send success!!!" << endl;
						while (true)
						{
							ZeroMemory(buffer, sizeof(buffer));
							recvSize = recvfrom(sockServer, buffer, BUFFER, 0, ((SOCKADDR *)&addrClient), &length);
							if (recvSize < 0)
							{
								IfTimeout(t);
							}
							else
							{
								cout << "get ack!!!" << endl;
								DataPackage *tmpData = new DataPackage();
								extract_pkt(buffer, *tmpData);
								if (tmpData->ackflag & (tmpData->ackNum == nextseqnum - 1) & (!corrupt(tmpData)))
								{
									cout << "ack success!!! ACK: " << tmpData->ackNum << endl;
									stage = 4;
									base = nextseqnum;
									break;
								}
								else
								{
									cout << "something error" << endl;
									IfTimeout(t);
								}
								delete tmpData;
							}
						}
					}
					break;
				}
				case 5: // 多个文件传输部分
				{
					bool multifile = false;
					cout << "this file has send success!!!\n If you want to transport other file cin [1] else [0]: \n";
					cin >> multifile;
					if (multifile)
						stage = 2;
					else
					{
						stage = 6;
					}

					break;
				}
				case 6: // 单项发送消息，作为测试
				{
					cout << "the server will log out ...\n";
					runFlag = false;
					logout = true;
					break;
				}
				}
			}
		}
	}
	//关闭套接字
	closesocket(sockServer);
	WSACleanup();
	system("pause");
	return 0;
}
