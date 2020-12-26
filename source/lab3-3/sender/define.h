#ifndef __DEFINE_H__
#define __DEFINE_H__

#include "common.h"

using namespace std;
#define BUFFER 1024    // int类型 //缓冲区大小（以太网中 UDP 的数据帧中包长度应小于 1480 字节）

#define TIMEOUT 5     // 超时,单位S, 代表着一组中所有的ACK都已经正确收到
#define S1 1
#define S2 2
#define S3 3
#define S4 4
#define S5 5

// TODO 超时没有检测到
// TODO seq1收不到
// 数据包结构
struct
DataPackage {
    // short 部分必须为偶数
    unsigned short sourcePort; // 2B 16bits
    unsigned short destPort;   // 2B 16bits
    unsigned short window;     // 窗口大小，字节数2B
    unsigned short checkSum;   // 校验和 2B
    unsigned short len;        // 数据部分长度
    unsigned short flag;       // 2B是否需要分段传输, 不需要则为0，否则为分段传输的数量
    unsigned short offset; // 分段传输时的偏移号
    unsigned short timeLive; // 生存期
    unsigned int seqNum;  // 序列号 4B
    unsigned int ackNum;  // ACK号 4B
    bool ackflag = false; // 1B
    char message[0];      // 消息部分
    // 计算checksum
    void CheckSum(unsigned short *buf) {
        int count = sizeof(this->message) / 2;
        register unsigned long sum = 0;
        while (count--) {
            sum += *buf++;
            if (sum & 0xFFFF0000) {
                sum &= 0xFFFF;
                sum++;
            }
        }
        this->checkSum = ~(sum & 0xFFFF);
    }

    void make_pkt(unsigned short sPort, unsigned short dPort, unsigned int nextseqnum, unsigned short windows) {
        this->sourcePort = sPort;
        this->destPort = dPort;
        this->seqNum = nextseqnum;
        this->window = windows;
    }
};

// 调试测试函数
void test(int i) {
    printf("this is a test: [%d]\n", i);
}

template<typename T>
void test2(T i) {
    cout << "======" << i << endl;
}

// 获得acknum
unsigned int getacknum(DataPackage *data) {
    return data->ackNum;
}

// 判断包是否损坏
bool corrupt(DataPackage *data) {
    int count = sizeof(data->message) / 2;
    register unsigned long sum = 0;
    unsigned short *buf = (unsigned short *) (data->message);
    while (count--) {
        sum += *buf++;
        if (sum & 0xFFFF0000) {
            sum &= 0xFFFF;
            sum++;
        }
    }
    if (data->checkSum == ~(sum & 0xFFFF))
        return true;
    return false;
}

void Strcpyn(char *dest, char *src, unsigned short n) {
    for (unsigned short i = 0; i < n; i++)
        dest[i] = *(src + i);
}

void show_package(DataPackage data, bool show_msg= false) {
    printf("========%d, %d========\n", data.seqNum, data.ackNum);
    printf("sourcePort: %d\n", data.sourcePort);
    printf("destPort: %d\n", data.destPort);
    printf("window: %d\n", data.window);
    printf("flag: %d\n", data.flag);
    printf("offset: %d\n", data.offset);
    printf("len: %d\n", data.len);
    printf("checksum: %d\n", data.checkSum);
    printf("ackflag: %d\n", data.ackflag);
    if(show_msg)
        cout<<data.message<<endl;
//        printf("msg: %s\n", data.message);

    printf("=======================\n");
}

// 将char*转为DataPackage
void extract_pkt(char *message, DataPackage &resultdata) {
    resultdata.sourcePort = *((unsigned short *) &(message[0]));
    resultdata.destPort = *((unsigned short *) &(message[2]));
    resultdata.window = *((unsigned short *) &(message[4]));
    resultdata.checkSum = *((unsigned short *) &(message[6]));
    resultdata.len = *((unsigned short *) &(message[8]));
    resultdata.flag = *((unsigned short *) &(message[10]));
    resultdata.offset = *((unsigned short *) &(message[12]));
    resultdata.timeLive = *((unsigned short *) &(message[14]));
    resultdata.seqNum = *((unsigned int *) &(message[16]));
    resultdata.ackNum = *((unsigned int *) &(message[20]));
    resultdata.ackflag = *((bool *) &(message[24]));

    Strcpyn(resultdata.message, (char *) &(message[25]), resultdata.len);
}

// 文件处理
class File {
public:
    string filePath; // 文件的路径
    int fileLen;     // 文件大小 B
    int packageSum;  // 需要分多少次发
    bool read;
    int fileLenRemain;

    // 初始化文件
    void initFile(bool read, string fp) {
        this->read = read;
        this->filePath = fp;
    }

    void WriteFile() {
        if (!this->read) {
            ofstream writeos(this->filePath, std::ios::out | std::ios::binary);
            if (!writeos.is_open()) {
                printf("file open fail ！！！\n");
                exit(1);
            }
        }
    }

    void ReadFile() {
        if (this->read) {
            ifstream readis(this->filePath, ifstream::in | ios::binary); //以二进制方式打开文件
            // 如果文件没有打开
            cout << this->filePath << endl;
            if (!readis.is_open())
                cout << "this file can't be opened!" << endl;

            streampos pos = readis.tellg(); // 获得文件开始的位置
            readis.seekg(0, ios::end);      //将文件流指针定位到流的末尾
            this->fileLen = readis.tellg(); // 文件的总长度
            readis.seekg(pos);              //将文件流指针重新定位到流的开始
            this->fileLenRemain = this->fileLen % (BUFFER - sizeof(DataPackage) - 1);
            if (this->fileLenRemain)
                this->packageSum = int(this->fileLen / (BUFFER - sizeof(DataPackage) - 1)) + 1;
            else {
                this->packageSum = int(this->fileLen / (BUFFER - sizeof(DataPackage) - 1));
            }
            cout << "file pk: " << this->packageSum << endl;
        }
    }
};

// 计时操作
class Timer {
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

    double GetTime() { return (double) (clock() - this->start_time) / CLOCKS_PER_SEC; }

    void Show();

    // 判断是否超时
    bool TimeOut() {
        if (this->is_stop == true)
            return false;
        if (this->GetTime() >= this->timeout)
            return true;
        else
            return false;
    }
};

Timer::Timer() {
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

void Timer::Show() {
    cout << "===================" << endl;
    cout << "t: " << this->GetTime() << "s" << endl;
    cout << "===================" << endl;

}

void ShowPerformance(File f, Timer t){
    cout << "===================" << endl;
    double rate =  f.fileLen/(t.GetTime()*1000);
    cout<<"the rate is: "<<rate<<"KB/s, "<<rate/1000<<" M/s"<<endl;
    cout << "===================" << endl;
}
#endif