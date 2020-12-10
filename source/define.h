#ifndef __DEFINE_H__
#define __DEFINE_H__
#include "common.h"
using namespace std;
#define BUFFER 512   // int类型 //缓冲区大小（以太网中 UDP 的数据帧中包长度应小于 1480 字节）
#define WINDOWSIZE 10 //滑动窗口大小为 10，当改为1时即为停等协议
#define TIMEOUT 5   // 超时
#define S1 1
#define S2 2
#define S3 3
#define S4 4                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    
#define HEADER_LEN 23 // 19B
// 数据包结构
struct 
DataPackage
{
    // int version=1; //协议版本
    // short 部分必须为偶数
    unsigned short sourcePort; // 2B 16bits
    unsigned short destPort;   // 2B 16bits
    unsigned short window;     // 窗口大小，字节数2B
    unsigned short checkSum;   // 校验和 2B
    unsigned short len;        // 数据部分长度
    unsigned short flag;       // 2B是否需要分段传输, 不需要则为0，否则为分段传输的数量
    unsigned short offset;
    unsigned short timeLive;
    unsigned int seqNum;  // 序列号 4B
    unsigned int ackNum;  // ACK号 4B
    bool ackflag = false; // 1B
    char message[0];      // 消息部分
    void CheckSum(unsigned short *buf)
    {
        int count = sizeof(this->message) / 2;
        register unsigned long sum = 0;
        while (count--)
        {
            sum += *buf++;
            if (sum & 0xFFFF0000)
            {
                sum &= 0xFFFF;
                sum++;
            }
        }
        this->checkSum = ~(sum & 0xFFFF);
    }
    void make_pkt(unsigned short sPort, unsigned short dPort, unsigned int nextseqnum, unsigned short windows)
    {
        this->sourcePort = sPort;
        this->destPort = dPort;
        this->seqNum = nextseqnum;
        this->window = windows;
    }
};

unsigned int getacknum(DataPackage *data)
{
    return data->ackNum;
}
// 判断包是否损坏
bool corrupt(DataPackage *data)
{
    int count = sizeof(data->message) / 2;
    register unsigned long sum = 0;
    unsigned short *buf = (unsigned short *)(data->message);
    while (count--)
    {
        sum += *buf++;
        if (sum & 0xFFFF0000)
        {
            sum &= 0xFFFF;
            sum++;
        }
    }
    if (data->checkSum == ~(sum & 0xFFFF))
        return true;
    return false;
}

void Strcpyn(char *dest, char *src, unsigned short n)
{
    for (int i = 0; i < n; i++)
    {
        dest[i] = *(src + i);
        // cout << "s[i]" << i << " " << *(src + i) << endl;
        // cout << "d[i]" << i << " " << dest[i] << endl;
    }
}
void Strcpy(char *dest, char *src)
{
    if (dest == NULL || src == NULL)
        return;
    if (dest == src)
        return;
    int i = 0;
    while (src[i] != '\0')
    {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}
DataPackage *extract_pkt(char *message)
{
    DataPackage *data = new DataPackage();
    data->sourcePort = *((unsigned short *)&(message[0]));
    data->destPort = *((unsigned short *)&(message[2]));
    data->window = *((unsigned short *)&(message[4]));
    data->checkSum = *((unsigned short *)&(message[6]));
    // cout << "check: " << data->checkSum << endl;

    data->len = *((unsigned short *)&(message[8]));
    // cout << "len: " << data->len << endl;
    data->flag = *((unsigned short *)&(message[10]));

    data->offset = *((unsigned short *)&(message[12]));
    data->timeLive = *((unsigned short *)&(message[14]));

    data->seqNum = *((unsigned int *)&(message[16]));
    // cout << "seq: " << data->seqNum << endl;
    data->ackNum = *((unsigned int *)&(message[20]));
    // cout << "ack: " << data->ackNum << endl;
    data->ackflag = *((bool *)&(message[24]));
    Strcpyn(data->message, (char *)&(message[25]), data->len);
    // cout << "msg: " << data->message << endl;
    return data;
}

// 文件处理
class File
{
public:
    string filePath; // 文件的路径
    int fileLen;     // 文件大小 bits
    int packageSum;  // 需要分多少次发
    // std::ifstream *readis;
    bool read;
    // std::ofstream *writeos;
    File(bool read, string fp)
    {
        this->read = read;
        this->filePath = fp;
    }
    void WriteFile()
    {
        if (!this->read)
        {
            ofstream writeos(this->filePath, std::ios::out | std::ios::binary);
            if (!writeos.is_open())
            {
                printf("file open fail ！！！\n");
                exit(1);
            }
        }
    }
    void ReadFile()
    {
        if (this->read)
        {
            ifstream readis(this->filePath, ifstream::in | ios::binary); //以二进制方式打开文件
            // 如果文件没有打开
            cout << this->filePath << endl;
            if (!readis.is_open())
                cout << "this file can't be opened!" << endl;

            streampos pos = readis.tellg(); // 获得文件开始的位置
            readis.seekg(0, ios::end);      //将文件流指针定位到流的末尾
            this->fileLen = readis.tellg(); // 文件的总长度
            readis.seekg(pos);              //将文件流指针重新定位到流的开始
            if (this->fileLen % (BUFFER - sizeof(DataPackage) - 1))
                this->packageSum = int(this->fileLen / (BUFFER - sizeof(DataPackage) - 1)) + 1;
            else
            {
                this->packageSum = int(this->fileLen / (BUFFER - sizeof(DataPackage) - 1));
            }
            cout << "file pk: " << this->packageSum << endl;
        }
    }
};

// 计时操作
class Timer
{
public:
    clock_t start_time;
    clock_t pause_time;
    //两个bool值标记四种状态
    bool is_pause; //记录计时器的状态 （是否处于暂停状态）
    bool is_stop;  //是否处于停止状态
    double timeout;

public:
    Timer();
    //计时器的三种动作（功能）
    void Start();
    void Pause();
    void Stop();
    // clock_t GetStartTime() { return this->start_time; }
    double GetTime() { return (double)(clock() - this->start_time) / CLOCKS_PER_SEC; }
    void Show();
    // 判断是否超时
    bool TimeOut()
    {
        if (this->GetTime() >= this->timeout)
            return true;
        else
            return false;
    }
};

Timer::Timer()
{
    this->is_pause = false; //初始化计时器状态
    this->is_stop = true;
    // 初始化超时间隔
    this->timeout = TIMEOUT;
}

void Timer::Start() //开始
{
    this->start_time = clock();
    this->is_stop = false;
}
void Timer::Pause() //暂停
{
    if (this->is_stop || this->is_pause) //如果处于停止/暂停状态,此动作不做任何处理，直接返回
        return;
    else //否则调制为暂停状态
    {
        this->is_pause = true;
        this->pause_time = clock(); //获取暂停时间
    }
}
void Timer::Stop() //停止
{
    if (this->is_stop) //如果正处于停止状态（不是暂停状态），不做任何处理
        return;
    else if (this->is_pause) //改变计时器状态
    {
        this->is_pause = false;
        this->is_stop = true;
    }
    else if (!this->is_stop)
    {
        this->is_stop = true;
    }
}

void Timer::Show()
{
    double t = clock() - this->start_time;
    cout << "t: " << t << "ms" << endl;
    // cout<<setw(2)<<setfill('0')<<t/60/60<<":"
    //     <<setw(2)<<setfill('0')<<t/60%20<<":"
    // 	<<setw(2)<<setfill('0')<<t%60<<endl;
}

#endif