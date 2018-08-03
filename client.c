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
void send_cmd_ls(int sock, char *cmd);
void send_cmd_quit(int sock);
void send_cmd_get(int sock, char *fileName);
void send_cmd_put(int sock, char *fileName);
void send_cmd_pwd(int sock);
void send_cmd_cd(int sock,char *pathName);
int clientDelRecv(int sock);
static thread_pool *pool=NULL;
static int cli_fd=-1;
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
    setbuf(stdout,NULL);
    while(1){
        printf("\r$ ");
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
		send_cmd_ls(sock, cmd);
	}else if(strncmp(cmd, "get", 3) == 0){
        send_cmd_get(sock,cmd+3);
    }else if(strncmp(cmd, "put", 3) == 0){
        send_cmd_put(sock,cmd+3);
    }else if(strncmp(cmd, "pwd", 3) == 0){
        send_cmd_pwd(sock);
    }else if(strncmp(cmd, "cd", 2) == 0){
        send_cmd_cd(sock,cmd+2);
    }else if(strncmp(cmd, "quit", 4) == 0){
        send_cmd_quit(sock);
    }else{
        //先什么都不做，丢弃
    }
    return 0;
}
void send_cmd_ls(int sock, char *cmd){
	char send_cmd[512];
    package_head(send_cmd,0x0002,6);
    snprintf((char *)send_cmd+6,240,"%s",cmd+2);
	write(sock, send_cmd, strlen(cmd+2)+6+1);//发送结束符
}
void send_cmd_get(int sock, char *fileName){
    int i=0;
    while(fileName[i]==' '&& i<maxFileNameLen){
        i++;
    }
    while(1){
        if(fileName[strlen(fileName)-1]=='\r'||fileName[strlen(fileName)-1]=='\n'){
            fileName[strlen(fileName)-1]=0;
        }else{
            break;
        }
    }
    if(cli_fd!=-1){
        close(cli_fd);
        cli_fd=-1;
    }
    char ch[maxFileNameLen];
    bzero(ch,maxFileNameLen);
    strcpy(ch,fileName+i);
    while(1){
        cli_fd=open(ch, O_WRONLY|O_CREAT|O_EXCL,FILE_MODE);
        if(cli_fd==-1){//打开失败
            if(errno==EEXIST){
                perror("can not creat");
                printf("input new file name: ");
                bzero(ch,maxFileNameLen-10);
                fgets(ch,maxFileNameLen-10, stdin);
                ch[strlen(ch)-1]=0;
            }else{
                perror("open failed!\n");
                return ;
            }
        }else{
            break;
        }
    }
    char send_cmd[512];
    package_head(send_cmd,0x0003,6);
    snprintf((char *)send_cmd+6,240,"%s",fileName+i);
	write(sock, send_cmd, strlen(fileName+i)+6+1);//发送结束符
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
        packet_len= (p[2]&0xff)     | 
            ( (p[3]<<8)&0xff00  )   |
          ((p[4]<<16)&0xff0000  )   |
        ((p[5]<<24)&0xff000000);
        //printf("read:%d,cmd=%d,pa=%u,strlen(p+6)=%d\n",read_count,cmd_num,packet_len,strlen(p+6));
        //printf("%d %d %d %d\n",p[2] , (p[3]<<8 ) , (p[4]<<16) , (p[5]<<24));
        switch(cmd_num){
            case 0x0002:    //ls 命令
                ls(sock,p+6);
                break;
            case 0x0001:    //普通的消息
                add_task(pool,print,p+6);
                break;
            case 0x0004:    //传输的是文件
                //add_task(pool,creat_file,p+6);
                creat_file(&cli_fd,sock,p+6,packet_len);
                break;
            case 0x0008:    //文件不存在
                printf("the file is not exit!\n");
            case 0x0005:    //结束文件传输
                if(cli_fd==-1){
                    close(cli_fd);
                    cli_fd=-1;
                }
                printf("file transfer completed!\n");
                break;
            case 0x0007:
                handle_get(sock,p+6);
                break;
            case 0x0009:
                
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

void send_cmd_put(int sock, char *fileName){
    char send_cmd[maxMessageSize];
    bzero(send_cmd,maxMessageSize);
    snprintf(send_cmd+6,maxMessageSize-6,"%s",fileName);
    package_head(send_cmd,0x0006,strlen(send_cmd+6)+1);
    Write(sock,send_cmd,strlen(send_cmd+6)+1+6);
    printf("send put :%s\n",send_cmd+6);
}
void send_cmd_pwd(int sock){
    char send_cmd[maxMessageSize];
    bzero(send_cmd,maxMessageSize);
    package_head(send_cmd,0x0009,0);
    write(sock,send_cmd,6);
}

void send_cmd_cd(int sock,char *pathName){
    char send_cmd[maxMessageSize];
    bzero(send_cmd,maxMessageSize);
    snprintf(send_cmd+6,maxMessageSize-7,"%s",pathName);
    package_head(send_cmd,0x000a,strlen(send_cmd+6)+1);
    write(sock,send_cmd,6+strlen(send_cmd+6)+1);
}


void send_cmd_quit(int sock){
    close(sock);
    exit(0);
}
