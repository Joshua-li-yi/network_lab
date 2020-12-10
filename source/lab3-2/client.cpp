#include "define.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 8888	  //接收数据的端口号
#define SERVER_IP "127.0.0.1" //  服务器的 IP 地址

SOCKADDR_IN addrServer; //服务器地址
SOCKADDR_IN addrClient; //客户端地址

const int SEQNUMBER = 20; //接收端序列号个数，为 1~20
int expectedseqnum = 1;
SOCKET socketClient;
// 判断是否是期望的序列号
bool hasseqnum(DataPackage data, int expectedseqnum)
{
	if (data.seqNum == expectedseqnum)
		return true;
	else
	{
		return false;
	}
}
// TODO 有问题
// void rdt_send(DataPackage *data)
// {

// 	sendto(socketClient, (char *)data, sizeof(DataPackage) + data->len, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
// 	expectedseqnum++;
// }

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

	int recvSize;
	ofstream *outfile = new ofstream(); // 写文件
	File *recvFile = new File();		// 文件操作
	bool logout = false;				// 登出
	bool runFlag = true;				// 是否run
	unsigned int curACKnum = 0;
	while (!logout)
	{
		//向服务器发送给0表示请求连接
		sendto(socketClient, "0", strlen("0") + 1, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
		while (runFlag)
		{
			//等待 server 回复
			// Sleep(200);
			ZeroMemory(buffer, sizeof(buffer));
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
					recvFile->initFile(false, filePath);
					recvFile->WriteFile();
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

					DataPackage recvData;
					extract_pkt(buffer, recvData);
					// 如果是所期望的包的话
					if ((recvData.flag == 0) & hasseqnum(recvData, expectedseqnum) & (!corrupt(&recvData)))
					{ // 文件不分段的接收
						cout << "pkg not bad!" << endl;
						DataPackage *data = (DataPackage *)malloc(sizeof(DataPackage));
						data->ackNum = recvData.seqNum;
						data->make_pkt(SERVER_PORT, SERVER_PORT, 0, WINDOWSIZE);
						data->CheckSum((unsigned short *)data->message);
						data->ackflag = 1;
						data->len = 0;
						// TODO 这样写为啥报错
						// rdt_send(data);
						sendto(socketClient, (char *)data, sizeof(DataPackage) + data->len, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
						expectedseqnum++;

						cout << "begin write to file: " << recvFile->filePath << endl;
						// 打开文件开始写入
						outfile->open(recvFile->filePath, ios::out | ios::binary | ios::app);
						outfile->write(recvData.message, recvData.len - 1);
						outfile->close();
						//关闭套接字
						cout << "write file success!" << endl;
						runFlag = false;
						logout = true;
						stage = 10;
					}
					else if ((recvData.flag > 0) & hasseqnum(recvData, expectedseqnum) & (!corrupt(&recvData)))
					{
						// 文件分段接收
						cout << "pkg " << recvData.seqNum << " not bad!" << endl;
						// 发送ACK
						// TODO 这样报错
						// DataPackage* sendData = (DataPackage *)malloc(28);
						DataPackage sendData;

						sendData.ackNum = recvData.seqNum;
						sendData.make_pkt(SERVER_PORT, SERVER_PORT, 0, WINDOWSIZE);
						sendData.CheckSum((unsigned short *)sendData.message);

						sendData.ackflag = 1;
						sendData.len = 0;
						// 当前acknum
						curACKnum = sendData.ackNum;
						sendto(socketClient, (char *)(&sendData), sizeof(DataPackage) + sendData.len, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
						expectedseqnum++;

						cout << "begin write to file: " << recvFile->filePath << endl;
						printf("begin write %d segment ...\n", recvData.offset);
						if (recvData.offset < recvData.flag) // 直到所有分段发完
						{
							// 打开文件在文件最后写入
							outfile->open(recvFile->filePath, ios::out | ios::binary | ios::app);
							outfile->write(recvData.message, recvData.len - 1);
							outfile->close();

							if (recvData.offset == recvData.flag - 1)
							{
								cout << "write file success!" << endl;
								stage = 10;
								runFlag = false;
								logout = true;
							}
						}
						stage = 2;
					}
					else if ((recvData.flag > 0) & (!hasseqnum(recvData, expectedseqnum)) & (!corrupt(&recvData)))
					{
						// 如果收到的不是对应期望的分组
						cout<<"get "<<recvData.ackNum<<" not get expectedseqnum "<<expectedseqnum<<endl;
						DataPackage sendData;
						sendData.ackNum = curACKnum;
						sendData.make_pkt(SERVER_PORT, SERVER_PORT, 0, WINDOWSIZE);
						sendData.CheckSum((unsigned short *)sendData.message);
						sendData.ackflag = 1;
						sendData.len = 0;
						sendto(socketClient, (char *)(&sendData), sizeof(DataPackage) + sendData.len, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
					}
				}

				break;
			}
			case 10:
			{
				cout << "file transport end!" << endl;
				cout << "if you what to get other file input [1] else [0]\n";
				bool multiRev;
				if (multiRev)
					stage = 1;
				else
				{
					runFlag = false;
				}
				break;
			}
			}
		}
	}
	cout << "client logout ...\n";
	//关闭套接字
	closesocket(socketClient);
	WSACleanup();
	system("pause");
	return 0;
}
