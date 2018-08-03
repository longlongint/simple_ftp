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


int del_input(int sock,char *buf);
void ftp_cmd_ls(int sock, char *cmd);
int clientDelRecv(int sock);
thread_pool *pool=NULL;
int main(int argc,char *argv[]){

    if(argc<3){
        printf("./client [port]\n");
        exit(0);
    }
    pool = malloc(sizeof(thread_pool));
	init_pool(pool,200);
    
    char buf[256];
    int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1){
		perror("socket error:");
		return -1;
	}
    struct sockaddr_in lo;
	memset(&lo, 0, sizeof(lo));
	lo.sin_family = AF_INET;
	lo.sin_port =  htons(  atoi(argv[2]) );

	lo.sin_addr.s_addr = htonl(INADDR_ANY);
    int r = bind(sock,	(struct sockaddr*)&lo, sizeof(lo));
	if (r == -1){
		perror("bind error:");
		return -1;
	}

    struct sockaddr_in server;
	memset(&server,0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons( atoi(argv[1]) );
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	
    int err =  connect(sock, (struct sockaddr*)&server, sizeof(server));
	if (err == -1){
		perror("connect error:");
		return -1;
	}
    int maxfdp1;
    fd_set rset;
    FD_ZERO(&rset);
    int fd=-1;
    setbuf(stdout,NULL);
    while(1){
        printf("$");
        bzero(buf,256);
        FD_SET(fileno(stdin),&rset);
        FD_SET(sock,&rset);

        maxfdp1=max(fileno(stdin),sock)+1;
        select(maxfdp1,&rset,NULL,NULL,NULL);

        if(FD_ISSET(fileno(stdin),&rset)){
            fgets(buf,512,(stdin));
            //printf("get command: %s\n",buf);
            del_input(sock,buf);
        }

        if(FD_ISSET(sock,&rset)){
            clientDelRecv(sock);
        }
    }
    destroy_pool(pool);
}

int del_input(int sock,char *cmd){

    if (strncmp(cmd, "ls", 2) == 0){
			ftp_cmd_ls(sock, cmd);
	}
    return 0;
}
void ftp_cmd_ls(int sock, char *cmd){
	unsigned char send_cmd[512];
	int i = 0;

    unsigned short cmd_num=0x0002;
	unsigned int packet_len = sizeof(cmd_num)+sizeof(packet_len);

    send_cmd[i++] = cmd_num & 0xff;
    send_cmd[i++] = (cmd_num >>8) & 0xff;
	send_cmd[i++] = packet_len & 0xff;
	send_cmd[i++] = (packet_len >> 8 ) & 0xff;
	send_cmd[i++] = (packet_len >> 16) & 0xff;
	send_cmd[i++] = (packet_len >> 24) & 0xff;

    snprintf((char *)send_cmd+6,240,"%s",cmd+2);
	//2.发送
	write(sock, send_cmd, strlen(cmd+2)+6+1);//发送结束符

}
int clientDelRecv(int sock){
    unsigned short cmd_num=0;
	unsigned int packet_len = 0;
    char buf[maxMessageSize];
    char *p=NULL;
    bzero(buf,maxMessageSize);
    int read_count=0;

    read_count=read(sock,buf,maxMessageSize);
    p=buf;
    if(read_count<6){//消息长度最小是6字节
        close(sock);
        exit(0);
    }
    while(1){
        cmd_num=(p[0]&0xff) | ((p[1]<<8)&0xff);
        packet_len=p[2]|
                ((p[3]<<8 )&0xff) |
                ((p[4]<<16)&0xff) |
                ((p[5]<<24)&0xff) ;
        //printf("read:%d,cmd=%d,pa=%d,strlen(p+6)=%d\n",read_count,cmd_num,packet_len,strlen(p+6));
        switch(cmd_num){
            case 0x0002:    //ls 命令
                ls(sock,p+6);
                break;
            case 0x0001:    //普通的消息
                add_task(pool,print,p+6);
                break;
            default:
                //printf("");
                break;
        }
        p=p+6+packet_len;
        //printf("read:%d,now:%d\n",read_count,p-buf);
        if((p-buf)>=read_count)break;
    }
    return 0;
}
