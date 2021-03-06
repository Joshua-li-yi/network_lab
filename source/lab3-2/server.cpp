/**********************************
说明：该部分完成了本次作业要求中的3-1&3-2部分，实现了
面向连接、差错检测、确认重传、停等机制、滑动窗口(GBN方法)、累积确认等功能
详细说明见实验报告
***********************************/
#include "define.h"
#pragma comment(lib, "ws2_32.lib")
using namespace std;

#define SERVER_IP "127.0.0.1" //服务器IP地址
#define SERVER_PORT 8888	  //服务器端口号

SOCKADDR_IN addrServer; //服务器地址
SOCKADDR_IN addrClient; //客户端地址
SOCKET sockServer;

int length = sizeof(SOCKADDR);
char buffer[BUFFER];					  //数据发送接收缓冲区
Timer *ack_timer = new Timer[WINDOWSIZE]; // 标志每一个包的Timer

char **sndpkt;		// 缓冲区
int nextseqnum = 0; // 下一个seq的num
// 在接收到ACK之后才变化的值
int base = 0;		  // 窗口的第一个序号
int lastACKnum = -1;  // 用于检测重复ACK，如果是重复的ACK就会被忽略
int expectACKnum = 0; // 期望的ACKnum
// 期望的ACK num = base
// 只有getACKnum == expecACKnum，窗口才发生移动，窗口内的数据才被发送，定时器才开始计时
streampos pos;				   //文件光标位置，用于记录上一次光标的位置
unsigned short offset = 0;	   // 发送文件offset
ifstream *is = new ifstream(); // 读文件
File *sendFile = new File();   // 文件操作
int min_size = 0;			   // 初始化中要发送的包的个数

void send_data(char *data, Timer *t = ack_timer, int wndsize = WINDOWSIZE)
{
	/*
     * char *data: 要发送的数据
     * Timer *t 对应的acktimer数组
     * wndsize 当前的windsize大熊
     */
	DataPackage *tmp = (DataPackage *)malloc(
		sizeof(DataPackage) + (BUFFER - sizeof(DataPackage)) * sizeof(char));
	// extrack package将char*转为DataPackage
	extract_pkt(data, *tmp);
	// 判断是不是空数据
	if (tmp->destPort != 0)
	{
		// cout << "send: " << tmp->seqNum << endl;
		sendto(sockServer, (char *)tmp, sizeof(DataPackage) + tmp->len, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
		// 开启对应的计时器
		t[tmp->seqNum % wndsize].Start();
	}
	delete tmp;
}
// 超时重传处理函数 重传现在缓冲区中所有的数据包
void IfTimeout(Timer &t)
{
	// 如果超时
	if (t.TimeOut())
	{
		//        printf("Timer out error.\n");
		// 重启计时器
		t.Start();
		// 重传现在缓冲区中所有的数据包
		for (int i = 0; i < WINDOWSIZE; ++i)
			send_data(sndpkt[i]);
	}
}
// 从文件中读取数据
char *read_data_from_file()
{

	char *out_data = new char[BUFFER];
	DataPackage *data = (DataPackage *)malloc(
		sizeof(DataPackage) + (BUFFER - sizeof(DataPackage)) * sizeof(char));
	char *tmp = new char[BUFFER - sizeof(DataPackage)];
	// 打开文件
	is->open(sendFile->filePath, ifstream::in | ios::binary);
	// 定位到上次读取的位置
	is->seekg(pos);
	data->make_pkt(SERVER_PORT, SERVER_PORT, nextseqnum, WINDOWSIZE);
	data->ackNum = 0;
	data->ackflag = 0;
	nextseqnum++;
	// 标志文件分段发送
	data->flag = sendFile->packageSum;
	// 现在是第几段
	data->offset = offset;
	offset++;
	//    cout << "read data: " << data->seqNum << endl;
	// 还没有读到文件的最后一部分
	if (offset != sendFile->packageSum - 1)
	{
		is->read(tmp, BUFFER - sizeof(DataPackage) - 1);
		tmp[BUFFER - sizeof(DataPackage) - 1] = '\0';
		Strcpyn(data->message, tmp, BUFFER - sizeof(DataPackage));

		data->len = BUFFER - sizeof(DataPackage);
		// 计算checksum
		data->CheckSum((unsigned short *)data->message);
		// 拷贝在缓冲区中
		Strcpyn(out_data, (char *)data, BUFFER);
	}
	else
	{ // 如果是文件的最后一部分
		is->read(tmp, sendFile->fileLenRemain);
		tmp[sendFile->fileLenRemain] = '\0';
		Strcpyn(data->message, tmp, sendFile->fileLenRemain + 1);
		data->len = sendFile->fileLenRemain + 1;
		// 计算checksum
		data->CheckSum((unsigned short *)data->message);
		// 拷贝在缓冲区中
		Strcpyn(out_data, (char *)data,
				sizeof(DataPackage) + (sendFile->fileLenRemain + 1) * sizeof(char));
	}

	// 获取现在的位置
	pos = is->tellg();
	// 关闭文件
	is->close();
	delete tmp;
	delete data;
	// 返回读取出来的数据
	return out_data;
}
// 窗口移动
void window_shift()
{
	// 缓存区中往前移一个
	for (int i = 0; i < WINDOWSIZE - 1; i++)
		Strcpyn(sndpkt[i], sndpkt[i + 1], BUFFER);
	// 窗口移动时需要新增加数据
	if (offset < sendFile->packageSum)
	{
		sndpkt[WINDOWSIZE - 1] = read_data_from_file();
	}
	else
	{
		// 不需要增加新数据时
		ZeroMemory(sndpkt[WINDOWSIZE - 1], BUFFER);
	}
	send_data(sndpkt[WINDOWSIZE - 1]);
}

// 窗口初始化
void window_init()
{
	// 如果文件比较小，不能填满所有窗口
	if (sendFile->packageSum <= WINDOWSIZE)
	{
		// 将数据包加入到缓存当中
		for (int i = 0; i < sendFile->packageSum - 1; i++)
			sndpkt[i] = read_data_from_file();
	}
	else
	{
		for (int i = 0; i < WINDOWSIZE; i++)
			sndpkt[i] = read_data_from_file();
	}
	// 开始发送现在的数据包
	for (int i = 0; i <= min_size; i++)
		send_data(sndpkt[i]);
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
	int server_port = -1;
	string server_ip = "-1";
	cout << "please input ip addr: ";
	// cin >> server_ip;
	if (server_ip == "-1")
	{
		cout << "\tdefault IP:  " << SERVER_IP << "\n";
		server_ip = SERVER_IP;
	}

	cout << "please input sever port: ";
	// cin >> server_port;
	if (server_port == -1)
	{
		cout << "\tdefault port: " << SERVER_PORT << "\n";
		server_port = SERVER_PORT;
	}
	sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//设置套接字为非阻塞模式
	int iMode = 1;											//1：非阻塞，0：阻塞
	ioctlsocket(sockServer, FIONBIO, (u_long FAR *)&iMode); //非阻塞设置
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(server_port);
	err = bind(sockServer, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
	if (err)
	{
		err = GetLastError();
		printf("Could  not  bind  the  port  %d  for  socket.Error  code is %d\n", SERVER_PORT, err);
		WSACleanup();
		return -1;
	}

	int recvSize;
	bool is_connect = false; // 判断是否连接
	Timer *t = new Timer();	 // 计时器
	Timer *totalT = new Timer();
	sndpkt = new char *[WINDOWSIZE];
	for (int i = 0; i < WINDOWSIZE; i++)
		sndpkt[i] = new char[BUFFER];
	bool logout = false;		 // 是否登出
	bool runFlag = true;		 //是否运行
	bool first_open_file = true; // 是否是第一次打开该文件

	while (!logout)
	{
		if (!is_connect)
			cout << "wait connect with client ..." << endl;
		// Sleep(200);
		ZeroMemory(buffer, sizeof(buffer));
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
		//服务器向客户端发送一个 S1（ASCII） 大小的状态码，表示服务器准备好了，可以发送数据
		//客户端收到 S1 之后回复一个 S2 大小的状态码，表示客户端准备好了，可以接收数据了
		//服务器收到 S2 状态码之后，就开始发送数据了
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
					string filePath="-1";
					cout << "Please input file path which needs to be send: " << endl;
					cin >> filePath;
					if (filePath == "-1")
					{
						filePath = ".\\test\\helloworld.txt";
						cout << "the default file path is " << filePath << endl;
					}

					sendFile->initFile(true, filePath);
					sendFile->ReadFile();
					first_open_file = true;

					buffer[0] = S3;
					sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
					stage = 10;
					break;
				}
				case 10:
				{
					string filename="-1";
					cout << "Please input file name: " << endl;
					cin >> filename;
					if (filename == "-1")
					{
						filename = "helloworld.txt";
						cout << "the default file name is " << filename << endl;
					}

					for (int i = 0; i < filename.length(); i++)
						buffer[i] = filename[i];
					buffer[filename.length()] = '\0';
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
					// cout << "begin transport file ..." << endl;
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
						Strcpyn(data->message, tmp, sendFile->fileLen + 1);

						data->make_pkt(SERVER_PORT, SERVER_PORT, nextseqnum, WINDOWSIZE);
						data->CheckSum((unsigned short *)data->message);
						data->len = sendFile->fileLen + 1;
						data->flag = 0;
						data->offset = 0;
						// cout << "msg: " << data->message << endl;
						Strcpyn(sndpkt[0], (char *)data, sizeof(DataPackage) + data->len);
						// rdt_send(data, t);
						sendto(sockServer, (char *)data, sizeof(DataPackage) + data->len, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
						t->Start();
						nextseqnum++;

						delete data;
						cout << "send success!!!" << endl;
						while (true)
						{
							ZeroMemory(buffer, sizeof(buffer));
							recvSize = recvfrom(sockServer, buffer, BUFFER, 0, ((SOCKADDR *)&addrClient), &length);
							if (recvSize < 0)
							{
								IfTimeout(*t);
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
									base = nextseqnum;
									break;
								}
								else
								{
									cout << "something error" << endl;
									IfTimeout(*t);
								}
								delete tmpData;
							}
						}
					}
					else // 文件需要多次传输
					{

						// 如果是第一次打开文件
						if (first_open_file)
						{
							// 总的计时器打开
							totalT->Start();
							first_open_file = false;
							is->open(sendFile->filePath, ifstream::in | ios::binary);
							pos = is->tellg();
							is->close();
							// 窗口初始化参数，mini_size赋值
							if (sendFile->packageSum - 1 <= WINDOWSIZE - 1)
								min_size = sendFile->packageSum - 1;
							else
							{
								min_size = WINDOWSIZE - 1;
							}
						}
						// 窗口初始化
						window_init();

						// 发完现在所有窗口中的内容了，等待接收ACK
						while (true)
						{
							// 如果文件发完了就退出循环
							if (lastACKnum == sendFile->packageSum - 1)
							{
								totalT->Show();
								ShowPerformance(*sendFile, *totalT);
								stage = 5;
								offset = 0;
								break;
							}
							ZeroMemory(buffer, sizeof(buffer));
							recvSize = recvfrom(sockServer, buffer, BUFFER, 0, ((SOCKADDR *)&addrClient), &length);
							if (recvSize < 0)
							{
								IfTimeout(ack_timer[expectACKnum % WINDOWSIZE]);
							}
							else
							{
								// cout << "get ack!!!" << endl;
								DataPackage *tmpData = new DataPackage();
								extract_pkt(buffer, *tmpData);
								// 接收到ACK，ACK不是重复的ACK，且包没有损坏, 且收到时没有超时
								if (tmpData->ackflag & (tmpData->ackNum != lastACKnum) & (!corrupt(tmpData)))
								{
									// cout << "ack success!!! ACK: " << tmpData->ackNum << endl;
									// 累计确认
									for (int i = base; i <= tmpData->ackNum; i++)
										// 窗口移动
										window_shift();
									// base等于接收的ACK+1
									base = tmpData->ackNum + 1;
									expectACKnum = tmpData->ackNum + 1;
									lastACKnum = tmpData->ackNum;
								}
								else
								{
									// cout << "something error" << endl;
									IfTimeout(ack_timer[expectACKnum % WINDOWSIZE]);
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