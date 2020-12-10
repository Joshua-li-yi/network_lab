#include "common.h"
#define A 5
using namespace std;
int main()
{
        //用timeGetTime()来计时  毫秒  
    //用GetTickCount()来计时  毫秒  
    DWORD  dwGTCBegin, dwGTCEnd;  
    dwGTCBegin = GetTickCount();  
    Sleep(800);
    dwGTCEnd = GetTickCount();  
    printf("%d\n", dwGTCEnd - dwGTCBegin);  

    clock_t start,end;
    start=clock();
    int m=0;
    for(int i=0;i<1e3;i++)
    {
        for(int j=0;j<1e3;j++)
        {
            m++;
        }
    }
    end=clock();
    double n=(double)(end-start)/CLOCKS_PER_SEC;
    std::cout<<n<<endl;//0.002865
    cout<<n<<endl;//0.002865
    cout<<m<<endl;//1000000
}
