#include "common.h"
#define A 5
using namespace std;
int main()
{
        //用timeGetTime()来计时  毫秒  
    //用GetTickCount()来计时  毫秒  
    cout<<1111<<endl;
    int *a = new int();
    int *b = new int();
    *a = 10;
    b =a;
    cout<<*a<<endl;
    cout<< *a <<" "<<*b<<endl;
    // printf("%d, %d", *a, *b);
    *a = 20;
    // printf("%d, %d", *a, *b);
    // cout<< *a <<" "<<*b<<endl;
    cout<< *a <<" "<<*b<<endl;
    return 0;
}
