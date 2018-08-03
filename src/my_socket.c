
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
unsigned int maxFileNameLen=256;//最大文件长度
unsigned int curConnectNum=0;//保存当前连接数
int serv_fd=-1;
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
    //unsigned char fileConfirm=1;//文件确认标志，1表示对端可以处理
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
        packet_len= (buf[2]&0xff)     | 
            ( (buf[3]<<8)&0xff00  )   |
          ((buf[4]<<16)&0xff0000  )   |
        ((buf[5]<<24)&0xff000000);
        //printf("cmd_num=%d,packet_len=%d,strlen(buf+6)=%d\n",cmd_num,packet_len,strlen(buf+6));
        switch(cmd_num){
            case 0x0002:    //ls 命令
                ls(cli_fd,buf+6);
                break;
            case 0x0001:    //普通的消息
                printf("%s",buf+6);
                break;
            case 0x0003:    //
                //printf("recive file requst:%s\n",buf+6);
                handle_get(cli_fd,buf+6);
                break;
            case 0x0004:
                creat_file(&serv_fd,cli_fd,buf+6,packet_len);
                break;
            case 0x0008:    //文件不存在
                printf("the file is not exit!\n");
            case 0x0005:    //结束文件传输
                if(serv_fd==-1){
                    close(serv_fd);
                    serv_fd=-1;
                }
                printf("file transfer completed!\n");
                break;
            case 0x0006:
                handle_put(cli_fd,buf+6);
                break;    
            case 0x0007:
                //fileConfirm=1;
                break;
            case 0x0009:
                handle_pwd(cli_fd);
                break;
            case 0x000a:
                handle_cd(cli_fd,buf+6);
                break;
        }
    }
}
/**
 * @function:读取目录下的内容，并发送到对端
 * 
 * @param dir :路径名
 */
void ls(int cli_fd,char *dirName){

    char send_cmd[maxMessageSize];
    bzero(send_cmd,maxMessageSize);
    char *dir=correctName(dirName);
    
    DIR *d1=NULL;

    if(strlen(dir)==0){
        d1=opendir("./");
    }else{
        d1=opendir(dir);
    }
	if(d1==NULL){
        snprintf(send_cmd+6,maxMessageSize-7,"can not open %s",dir);
        package_head(send_cmd,0x0001,strlen(send_cmd+6)+1);
        write(cli_fd,send_cmd,strlen(send_cmd+6)+1+6);
		perror("DIR open error");
		return ;
	}
    struct dirent *d1_read=NULL;
    while(1){
        d1_read=readdir(d1);
		if(d1_read==NULL){
			//perror("read_dir");
			break;
		}
        bzero(send_cmd,maxMessageSize);
        snprintf(send_cmd+6,maxMessageSize-7,"%s",d1_read->d_name);
        //printf("type:%d,len:%d\n",0x0001,strlen(send_cmd+6)+1);
        package_head(send_cmd,0x0001,strlen(send_cmd+6)+1);
        Write(cli_fd,send_cmd,strlen(send_cmd+6)+1+6);
        //printf("name:%s\n",send_cmd+6);
    }
}
/**
 * @function:处理get请求
 * 
 */
void handle_get(int cli_fd,char *filename){
    int fd=-1;
    char send_cmd[maxMessageSize];
    char *fileName=correctName(filename);

    printf("request file:%s\n",fileName);
    fd=open(fileName,O_RDONLY);
    if(fd==-1){
        printf("oped faild||||\n");
        perror("open ");
        bzero(send_cmd,maxMessageSize);
        package_head(send_cmd,0x0008,0);
        Write(cli_fd,send_cmd,6);
        return ;
    }
    while(1){
        bzero(send_cmd,maxMessageSize);
        int r=read(fd,send_cmd+6,maxMessageSize-6);
        if(r<=0){//读完了,就结束文件传输
            bzero(send_cmd,maxMessageSize);
            package_head(send_cmd,0x0005,0);
            Write(cli_fd,send_cmd,6);
            return ;
        }
        //printf("r=%d\n",r);
        package_head(send_cmd,0x0004,r);
        Write(cli_fd,send_cmd,6+r);
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

void Write(int fd, void *ptr, size_t nbytes){
	int r=write(fd, ptr, nbytes);
    if ( r!= nbytes){
		printf("write error: %d %d\n",r,(int)nbytes);
    }
}
void handle_pwd(int cli_fd){
    //printf("get pwd request\n");
    char send_cmd[maxMessageSize];
    bzero(send_cmd,maxMessageSize-6);
    getcwd(send_cmd+6,maxMessageSize-6);
    package_head(send_cmd,0x0001,strlen(send_cmd+6)+1);
    write(cli_fd,send_cmd,6+strlen(send_cmd+6)+1);
}
void handle_cd(int cli_fd,char *pathName){
    char *p=correctName(pathName);
    int err = chdir(p);
    if(err==0){//打开失败
        perror("change path");
        char send_cmd[maxMessageSize];
        bzero(send_cmd,maxMessageSize-6);
        snprintf(send_cmd+6,maxMessageSize-6,"no such path name :%s",p);
        package_head(send_cmd,0x0001,strlen(send_cmd+6)+1);
        write(cli_fd,send_cmd,6+strlen(send_cmd+6)+1);
    }else{
        handle_pwd(cli_fd);
    }
}
char *correctName(char *pathName){
    int i=0;
    while(pathName[i]==' '){
        i++;
    }
    while(1){
        if(pathName[strlen(pathName)-1]=='\r'||pathName[strlen(pathName)-1]=='\n'||pathName[strlen(pathName)-1]==' '){
            pathName[strlen(pathName)-1]=0;
        }else{
            break;
        }
    }
   return  pathName+i;
}
void handle_put(int sock,char *fileName){
    char *p=correctName(fileName);
    char send_cmd[maxMessageSize];
        printf("%d\n",__LINE__);

    serv_fd=open(p, O_WRONLY|O_CREAT|O_EXCL,FILE_MODE);
    if(serv_fd==-1){
        if(errno==EEXIST){            
            bzero(send_cmd,maxMessageSize-6);
            snprintf(send_cmd+6,maxMessageSize-6,"file %s has allready exist",p);
            package_head(send_cmd,0x0001,strlen(send_cmd+6)+1);
            Write(sock,send_cmd,6+strlen(send_cmd+6)+1);
        }else{
            perror("open");
        }
        return ;
    }
    printf("%d\n",__LINE__);
    bzero(send_cmd,maxMessageSize-6);
    snprintf(send_cmd+6,maxMessageSize-6,"%s",p);
    package_head(send_cmd,0x0007,strlen(send_cmd+6)+1);
    Write(sock,send_cmd,6+strlen(send_cmd+6)+1);
}
int creat_file(int *file_fd,int sock,char *buf,int len){
    char send_cmd[maxMessageSize];
    bzero(send_cmd,maxMessageSize);
    if(*file_fd==-1){
        printf("file is not open!!!\n");
        return -1;
    }
    lseek(*file_fd,0,SEEK_END);
    Write(*file_fd,buf,len);
    return 0;
}