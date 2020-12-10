// #define _WINSOCK_DEPRECATED_NO_WARNINGS
// #define _CRT_SECURE_NO_WARNINGS
#include "define.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 8888	  //接收数据的端口号
#define SERVER_IP "127.0.0.1" //  服务器的 IP 地址

SOCKADDR_IN addrServer; //服务器地址
SOCKADDR_IN addrClient; //客户端地址

const int SEQNUMBER = 20; //接收端序列号个数，为 1~20
int totalpacket = 1618;
int totalrecv;
int expectedseqnum = 1;
SOCKET socketClient;

bool hasseqnum(DataPackage *data, int expectedseqnum)
{
	if (data->seqNum == expectedseqnum)
		return true;
	else
	{
		return false;
	}
}

void rdt_send(DataPackage *data)
{
	// cout<<data->checkSum<<endl;
	sendto(socketClient, "5555", strlen("5555") + 1, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
	sendto(socketClient, "0", strlen("0") + 1, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));

	sendto(socketClient, (char *)data, sizeof(DataPackage) + data->len, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
	expectedseqnum++;
	// cout<<222<<endl;
}

int main(int argc, char *argv[])
{
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
		return 1;
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

	socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	//接收缓冲区
	char buffer[BUFFER];
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	int waitCount = 0;							   //等待次数
	int stage = 0;								   // 初始化状态
	unsigned short seq;							   //包的序列号
	unsigned short recvSeq;						   //已确认的最大序列号
	unsigned short waitSeq;						   //等待的序列号 ，窗口大小为10，这个为最小的值
	char RetransmissionBuffer[WINDOWSIZE][BUFFER]; //接收到的缓冲区数据
	int i = 0;									   //接收缓存区状态
	for (i = 0; i < WINDOWSIZE; i++)
		ZeroMemory(RetransmissionBuffer[i], sizeof(RetransmissionBuffer[i]));

	BOOL ack_send[WINDOWSIZE]; //ack发送情况的记录，对应1-20的ack,刚开始全为false

	for (i = 0; i < WINDOWSIZE; i++)
		ack_send[i] = false; //记录哪一个成功接收了
	// write file 到当前的目录下
	// std::ofstream out_result("helloworld.txt", std::ios::out | std::ios::binary); // 	文件打开失败
	// if (!out_result.is_open())
	// {
	// 	printf("file open fail ！！！\n");
	// 	exit(1);
	// }
	int recvSize;
	ofstream outfile;
	File *recvFile;
	bool logout = false;
	bool runFlage = true;
	while (!logout)
	{
		//向服务器发送给0表示请求连接
		sendto(socketClient, "0", strlen("0") + 1, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
		while (runFlage)
		{
			//等待 server 回复
			recvSize = recvfrom(socketClient, buffer, BUFFER, 0, (SOCKADDR *)&addrServer, &len);
			switch (stage)
			{
			case 0: //等待握手阶段
				if ((unsigned char)buffer[0] == S1)
				{
					cout << "get S1 form server!" << endl;
					buffer[0] = S2;
					buffer[1] = '\0';
					cout << "send S2 to server ..." << endl;
					sendto(socketClient, buffer, 2, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
					stage = 1;
				}
				break;
			case 1: //选择下载位置阶段
			{
				if ((unsigned char)buffer[0] == S3)
				{
					cout << "server has select file to transport!!!" << endl;
					cout << "please select path to download this file ..." << endl;
					string filePath = "helloworld.txt";
					cin >> filePath;
					File *f = new File(false, filePath);
					f->WriteFile();
					recvFile = f;
					buffer[0] = S4;
					buffer[1] = '\0';
					sendto(socketClient, buffer, 2, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
					stage = 2;
				}
				break;
			}
			case 2:
			{
				if (recvSize >= 0)
				{
					cout << "get pkg from server ..." << endl;
					DataPackage *tmp = extract_pkt(buffer);

					// 如果是所期望的包的话
					cout << "flag: " << tmp->flag << endl;
					cout << "seq: " << tmp->seqNum << endl;
					cout << "msg: " << tmp->message << endl;
					if ((tmp->flag == 0) & hasseqnum(tmp, expectedseqnum) & (!corrupt(tmp)))
					{
						cout << "pkg not bad!" << endl;
						DataPackage *data = (DataPackage *)malloc(sizeof(DataPackage));

						data->ackNum = data->seqNum;
						data->make_pkt(SERVER_PORT, SERVER_PORT, 0, WINDOWSIZE);
						data->CheckSum((unsigned short *)data->message);
						data->ackflag = 1;
						data->len = 0;
						cout << "cks:" << data->checkSum << endl;
						// TODO 这样写为啥报错
						// rdt_send(data);
						sendto(socketClient, (char *)data, sizeof(DataPackage) + data->len, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
						expectedseqnum++;

						cout << "begin write to file: " << recvFile->filePath << endl;
						outfile.open(recvFile->filePath, ios::out | ios::binary);
						outfile.write(tmp->message, tmp->len + 1);
						cout << "write file success!" << endl;
						runFlage = false;
						logout = true;
						stage = 10;
					}
				}
				break;
			}
			case 10:
			{
				cout << "file transport end!" << endl;
				cout << "if you what to get other file input [1] else [0]\n";
				bool tmp;
				if (tmp)
					stage = 1;
				else
				{
					runFlage = false;
				}
				break;
			}
			}
		}
	}
	cout<<"client logout ...\n";
	outfile.close();
	//关闭套接字
	closesocket(socketClient);
	WSACleanup();
	system("pause");
	return 0;
}
