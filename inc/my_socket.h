#ifndef __MY_SOCKET_H
#define __MY_SOCKET_H

/**
 * 协议总述：使用最常见的TLV格式数据
 *    T:2个字节，L:4字节，V:根据L而定
 * 
 * 类型字段 T:
 *    0x0001:普通消息
 *    0x0002:请求文件列表
 *    0x0003:请求文件
 *    0x0004:文件内容
 *    0x0005:结束文件传输
 *    0x0006:请求上传文件
 * 
 * 注意： 
 *    数据的传输不包含'\0'
 */

extern const unsigned int maxConnectNum;
extern unsigned int curConnectNum;
extern const unsigned int maxMessageSize;//允许发送的最大消息长度

/**
 * @function:创建一个TCP服务
 * @return -1:创建失败
 * @return > 0:创建成功
 */
int creatTcpServer(int port);

/**
 * @function:处理新的连接
 * @param cli_fd:客户端描述符
 * 
 */
void client_handle(int cli_fd);

/**
 * @返回最大值
 * 
 */
int max(int a,int b);
void  package_head(char *send_cmd,unsigned short cmd_num,unsigned int packet_len);
/**
 * @function:读取目录下的内容，并发送到对端
 * 
 * @param dir :路径名
 */
void ls(int cli_fd,char *dir);

void Write(int fd, void *ptr, size_t nbytes);





#endif