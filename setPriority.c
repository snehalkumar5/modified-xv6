#include "types.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if(argc!=3)
    {
        printf(2,"Invalid number of arguments!\n");
        exit();
    }

    int newpr = atoi(argv[1]);
    int pid = atoi(argv[2]);
    // int oldpr =12; 
    // printf(1,"%d %d",newpr,pid);
    // if(oldpr<0)

    if(set_priority(newpr,pid)==-69)
    {
        printf(2,"Error in set priority\n");
        exit();
    }
    return 0;
}