/*************************************************************************
    > File Name: test.c
    > Author: hzq
    > Mail: 1593409937@qq.com 
    > Created Time: Sat 04 Aug 2018 04:11:36 PM CST
    > function:
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>


int main(){
    char array[100];
    char *p=array;
    printf("sizeof(array)=%d\n",sizeof(array));
    printf("sizeof(p)=%d\n",sizeof(p));
    printf("sizeof(*p)=%d\n",sizeof(*p));
    return 0;
}











