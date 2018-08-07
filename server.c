#include "my_thread_pool.h"
#include "my_socket.h"
#include "my_signal.h"

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

int main(int argc,char *argv[]){
    if(argc<2){
        printf("./server [port]\n");
        exit(0);
    }
    pid_t pid;
    struct sockaddr_in cliAddr;
    bzero(&cliAddr,sizeof(cliAddr));
    socklen_t cliAddrlen=sizeof(cliAddr);

    int serv_fd=creatTcpServer(atoi(argv[1]));
    if(serv_fd==-1){
        perror("creatTcpServer failed");
        exit(0);
    }
    int err=listen(serv_fd,5);
    if(err==-1){
        perror("listen");
        exit(0);
    }
    while(1){
        int cli_fd=accept(serv_fd,(struct sockaddr *)&cliAddr,&cliAddrlen);
        printf("connection from %s,port:%d\n",inet_ntoa(((struct sockaddr_in)cliAddr).sin_addr),
        ntohs(((struct sockaddr_in)cliAddr).sin_port));
        pid=fork();
        if(pid==0){//child
            client_handle(cli_fd);
        }else if(pid>0){
            close(cli_fd);
        }else{
            perror("fork");
            exit(0);
        }
    }
    return 0;
}