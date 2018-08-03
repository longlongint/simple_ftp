
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

#include "my_socket.h"
#include "my_signal.h"

const unsigned int maxConnectNum=1024;//记录最大连接数
const unsigned int maxMessageSize=1024;//允许发送的最大消息长度
unsigned int curConnectNum=0;//保存当前连接数

/**
 * @function:创建一个TCP服务
 * @return -1:创建失败
 * @return > 0:创建成功
 */
int creatTcpServer(int port){
    int sock_fd=-1;

    sock_fd=socket(AF_INET,SOCK_STREAM,0);
    assert(sock_fd!=-1);

    struct sockaddr_in addr;
    bzero(&addr,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(port);
    int err = bind(sock_fd,(struct sockaddr *)&addr,sizeof(addr));
    if(err==-1){
        perror("bind ");
        exit(0);
    }
    err = listen(sock_fd,5);
    if(err==-1){
        perror("listen ");
        exit(0);
    }
    signal(SIGCHLD,sig_chld);
    return sock_fd;
}
/**
 * @返回最大值
 * 
 */
int max(int a,int b){
    return a>b?a:b;
}
/**
 * @function:处理新的连接
 * @param cli_fd:客户端描述符
 * 
 */
void client_handle(int cli_fd){

    unsigned short cmd_num=0;
	unsigned int packet_len = 0;
    char buf[maxMessageSize];
    bzero(buf,maxMessageSize);
    int read_count=0;
    while(1){
        bzero(buf,maxMessageSize);
        read_count=read(cli_fd,buf,maxMessageSize);
        if(read_count<6){//消息长度最小是6字节
            close(cli_fd);
            exit(0);
        }
        cmd_num=(buf[0]&0xff) | ((buf[1]<<8)&0xff);
        packet_len=buf[2]|
                ((buf[3]<<8 )&0xff) |
                ((buf[4]<<16)&0xff) |
                ((buf[5]<<24)&0xff) ;
        printf("cmd_num=%d,packet_len=%d,strlen(buf+6)=%d\n",cmd_num,packet_len,strlen(buf+6));
        switch(cmd_num){
            case 0x0002:    //ls 命令
                ls(cli_fd,buf+6);
                break;
            case 0x0001:    //普通的消息
                printf("%s",buf+6);
                break;
        }
    }

}
/**
 * @function:读取目录下的内容，并发送到对端
 * 
 * @param dir :路径名
 */
void ls(int cli_fd,char *dir){

    char send_cmd[maxMessageSize];
    bzero(send_cmd,maxMessageSize);
    int i=0;
    while(dir[i]==' '){
        i++;
    }
    while(1){
        if(dir[strlen(dir)-1]=='\r'||dir[strlen(dir)-1]=='\n'){
            dir[strlen(dir)-1]=0;
        }else{
            break;
        }
    }
    DIR *d1=NULL;

    if(strlen(dir+i)==0){
        d1=opendir("./");
    }else{
        d1=opendir(dir);
    }
	if(d1==NULL){
        snprintf(send_cmd+6,maxMessageSize-7,"can not open %s\n",dir);
        package_head(send_cmd,0x0001,strlen(send_cmd+6)+1);
        write(cli_fd,send_cmd,strlen(send_cmd+6)+1+6);
		perror("DIR open error");
		return ;
	}
    struct dirent *d1_read=NULL;
    while(1){
        d1_read=readdir(d1);
		if(d1_read==NULL){
			perror("read_dir");
			break;
		}
        snprintf(send_cmd+6,maxMessageSize-7,"%s\n",d1_read->d_name);
        //printf("type:%d,len:%d\n",0x0001,strlen(send_cmd+6)+1);
        package_head(send_cmd,0x0001,strlen(send_cmd+6)+1);
        Write(cli_fd,send_cmd,strlen(send_cmd+6)+1+6);
        printf("name:%s\n",send_cmd+6);
    }
}

void  package_head(char *send_cmd,unsigned short cmd_num,unsigned int packet_len){
    //unsigned short cmd_num=0x0002;
	//unsigned int packet_len = sizeof(cmd_num)+sizeof(packet_len);
    assert(sizeof(send_cmd)>=6);
    int i=0;
    //cmd_num: 0x00000006
	//cmd_num => send_cmd[0] send_cmd[1] 
    //              0x02       0x00
	//packet_len: 0x00000006
	//packet_len => send_cmd[2] send_cmd[3] send_cmd[4] send_cmd[5]
	//					0x06		0x00		0x00	 0x00
	//命令长度 ，小端模式存放
    send_cmd[i++] = cmd_num & 0xff;
    send_cmd[i++] = (cmd_num >>8) & 0xff;
	send_cmd[i++] = packet_len & 0xff;
	send_cmd[i++] = (packet_len >> 8 ) & 0xff;
	send_cmd[i++] = (packet_len >> 16) & 0xff;
	send_cmd[i++] = (packet_len >> 24) & 0xff;
}

void Write(int fd, void *ptr, size_t nbytes)
{
	if (write(fd, ptr, nbytes) != nbytes)
		printf("write error");
}