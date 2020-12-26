#include "define.h"

#pragma comment(lib, "ws2_32.lib")

// #define CLIENT_PORT 8888	  //接收数据的端口号
#define CLIENT_PORT 8889	  //接收数据的端口号
#define CLIENT_IP "127.0.0.1" //  服务器的 IP 地址

SOCKADDR_IN addrServer; //服务器地址
SOCKADDR_IN addrClient; //客户端地址

int expectedseqnum = 1; // 期望的seqnum
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

	int client_port = 11332;
	string client_ip = "127.0.0.1";
	cout << "please input client ip addr: ";
	cin >> client_ip;
	if (client_ip == "-1")
	{
		std ::cout << "\tdefault IP: " << CLIENT_IP << "\n";
		client_ip = CLIENT_IP;
	}
	const char *client_ip_const = client_ip.c_str();
	cout << "please input client port: ";
	cin >> client_port;
	if (client_port == -1)
	{
		cout << "\tdefault port: " << CLIENT_PORT << "\n";
		client_port = CLIENT_PORT;
	}

	socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(client_ip_const);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(client_port);
	int len = sizeof(SOCKADDR);

	char buffer[BUFFER]; //接收缓冲区
	ZeroMemory(buffer, sizeof(buffer));
	int waitCount = 0;					//等待次数
	int stage = 0;						// 初始化状态
	int recvSize;						//接收的状态
	ofstream *outfile = new ofstream(); // 写文件
	File *recvFile = new File();		// 文件操作
	bool logout = false;				// 登出
	bool runFlag = true;				// 是否run
	string filePath = "";				// 文件下载路径
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
			{
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
			}
			case 1: //选择下载位置阶段
			{
				if ((unsigned char)buffer[0] == S3)
				{
					cout << "server has select file to transport!!!" << endl;
					cout << "please select path to download this file ..." << endl;
					filePath = "./recv/";
					cin >> filePath;
					if (filePath == "-1")
					{
						filePath = "./recv/";
						cout << "the default file path is: " << filePath << endl;
					}
					stage = 50;
				}
				break;
			}
			case 50:
			{
				if (recvSize >= 0)
				{
					string filename = buffer;
					recvFile->initFile(false, filePath + filename);
					recvFile->WriteFile();
					ZeroMemory(buffer, sizeof(buffer));
					buffer[0] = S4;
					buffer[1] = '\0';
					sendto(socketClient, buffer, 2, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
					stage = 2;
					break;
				}
			}
			case 2:
			{
				if (recvSize >= 0)
				{
					// cout << "get pkg from server ..." << endl;

					DataPackage recvData;
					extract_pkt(buffer, recvData);
					// 如果是不分段的文件，所期望的包，且包中的数据没有损坏的话
					if ((recvData.flag == 0) & hasseqnum(recvData, expectedseqnum) & (!corrupt(&recvData)))
					{ // 文件不分段的接收
						cout << "pkg not bad!" << endl;
						DataPackage *data = (DataPackage *)malloc(sizeof(DataPackage));
						// acknum等于接收的seqnum
						data->ackNum = recvData.seqNum;
						data->make_pkt(CLIENT_PORT, CLIENT_PORT, 0, WINDOWSIZE);
						// 计算校验和
						data->CheckSum((unsigned short *)data->message);
						// 标志该数据包为ack包
						data->ackflag = 1;
						// 数据部分长度为0
						data->len = 0;
						//发送数据包
						sendto(socketClient, (char *)data, sizeof(DataPackage) + data->len, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
						// 期望的seq++
						expectedseqnum++;

						cout << "begin write to file: " << recvFile->filePath << endl;
						// 打开文件开始写入
						outfile->open(recvFile->filePath, ios::out | ios::binary | ios::app);
						outfile->write(recvData.message, recvData.len - 1);
						outfile->close();
						//关闭套接字
						cout << "write file success!" << endl;
						// 结束文件传输
						runFlag = false;
						logout = true;
						stage = 10;
					} // 如果是分段的文件，所期望的包，且包中的数据没有损坏的话
					else if ((recvData.flag > 0) & hasseqnum(recvData, expectedseqnum) & (!corrupt(&recvData)))
					{
						// 文件分段接收
						cout << "pkg " << recvData.seqNum << " not bad!" << endl;
						// 发送ACK
						DataPackage sendData;
						// acknum等于接收的seqnum
						sendData.ackNum = recvData.seqNum;
						sendData.make_pkt(CLIENT_PORT, CLIENT_PORT, 0, WINDOWSIZE);
						// 计算校验和
						sendData.CheckSum((unsigned short *)sendData.message);
						// 标志该数据包为ack包
						sendData.ackflag = 1;
						// 数据部分长度为0
						sendData.len = 0;
						//发送数据包
						sendto(socketClient, (char *)(&sendData), sizeof(DataPackage) + sendData.len, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
						
						// 期望的seq++
						expectedseqnum++;

						// cout << "begin write to file: " << recvFile->filePath << endl;
						printf("begin write %d segment ...\n", recvData.offset);

						if (recvData.offset < recvData.flag) // 直到所有分段发完
						{
							// 打开文件在文件最后写入
							outfile->open(recvFile->filePath, ios::out | ios::binary | ios::app);
							outfile->write(recvData.message, recvData.len - 1);
							// 关闭文件
							outfile->close();
							// 如果文件已经写入完毕，通过offset来判断
							if (recvData.offset == recvData.flag - 1)
							{
								cout << "write file success!" << endl;
								stage = 3;
								runFlag = false;
								logout = true;
								break;
							}
						}
						// 继续接收
						stage = 2;
					}
					// else if((recvData.flag > 0) & (!hasseqnum(recvData, expectedseqnum)) & (!corrupt(&recvData))){
					// 	// 如果收到的不是对应期望的分组
					// 	cout << "get " << recvData.ackNum << " not get expectedseqnum " << expectedseqnum << endl;
					// 	DataPackage sendData;
					// 	sendData.ackNum = expectedseqnum - 1;
					// 	sendData.make_pkt(CLIENT_PORT, CLIENT_PORT, 0, WINDOWSIZE);
					// 	sendData.CheckSum((unsigned short *)sendData.message);
					// 	sendData.ackflag = 1;
					// 	sendData.len = 0;
					// 	sendto(socketClient, (char *)(&sendData), sizeof(DataPackage) + sendData.len, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
					// }
				}
				break;
			}
			case 3:
			{
				cout << "file transport end!" << endl;
				cout << "if you what to get other file input [1] else [0]\n";
				bool multiRev = 0;
				cin >> multiRev;
				// 如果是多次接收
				if (multiRev)
					stage = 1;
				else
				{ // 关闭连接
					buffer[0] = DISCONNECT;
					buffer[1] = '\0';
					sendto(socketClient, buffer, 2, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
					runFlag = false;
					logout = true;
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
