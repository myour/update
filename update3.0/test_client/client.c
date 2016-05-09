/* ############################################
		升级程序功能测试用例程序
		功能：测试update.c的功能
		Author: chenrong 2016.03.30
############################################## */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

#include "update.h"

/* 调试变量 */
#define DEBUG_CLIENT

#ifdef 	DEBUG_CLIENT
#define CLI_PRINT(format, ...) printf("FILE: "__FILE__", LINE: %d: "format, __LINE__, ##__VA_ARGS__)
#else
#define CLI_PRINT(format, ...)
#endif

#define SRC_PORT	(8888)
/* 数据包 */
PST_MSG msg 	= NULL;
PST_MSG rev_msg = NULL;

/* socket */
int src_fd,dst_fd;
struct sockaddr_in src_addr,dst_addr,recv_addr;



/* 组包函数 */
PST_MSG makePack(EN_MSG_TYPE type,void *buf)
{
	//int ret = RET_OK;
	/* 根据update.h中定义构造数据包 */
	if(msg == NULL)
	{
		msg = malloc(sizeof(ST_MSG));
	}
	CLI_PRINT("msg malloc done,size %d.\n",sizeof(ST_MSG));
	if(msg == NULL)
	{
		CLI_PRINT("Msg malloc fail.\n");
	}
	memset(msg,0,sizeof(ST_MSG));
	msg->mType = type;
	switch(type)
	{
		case TYPE_MSG_HEAD:
			/* 确定数据类型 */
			memcpy(msg->buf,buf,sizeof(ST_FILE_HEADER));
			break;

		case TYPE_MSG_CONTENT:
			memcpy(msg->buf,buf,sizeof(ST_FILE_PKG));
			break;

		case TYPE_MSG_RPT_PROC:
			memcpy(msg->buf,buf,sizeof(ST_RPT_PKG));
			break;

		default:
			//ret = RET_FAIL;
			CLI_PRINT("Package type error,please check out.\n");
			break;
	}
	
	return msg;
}

/* 初始化udp */
int initUpdConn(void)
{
	int retval = RET_FAIL;
	int addr_sz = sizeof(struct sockaddr);	
	
	/* 1.初始化udp */
	src_fd = socket(AF_INET,SOCK_DGRAM,0);
	if(src_fd < 0)
	{
		CLI_PRINT("Client socket create fail..\n");
		return RET_FAIL;
	}
	CLI_PRINT("Client udp sock create success.\n");

	/* 2.绑定 */
	printf("######### socket bind #########\n");
	src_addr.sin_family			= AF_INET;
	src_addr.sin_addr.s_addr 	= htonl(INADDR_ANY);
	src_addr.sin_port			= htons(SRC_PORT);
	retval = bind(src_fd,(struct sockaddr *)&src_addr,addr_sz);
	if(retval < 0)
	{
		CLI_PRINT("Client bind fail.\n");
		close(src_fd);
		return RET_FAIL;
	}
	else
	{
		CLI_PRINT("Client sock bind success.\n");	
		retval = RET_OK;
	}

	dst_addr.sin_family 		= AF_INET;
	//dst_addr.sin_addr.s_addr	= inet_addr("192.168.16.67");
	dst_addr.sin_addr.s_addr	= inet_addr("192.168.16.181");
	dst_addr.sin_port			= htons(UDP_PORT);

	
	return retval;
}

/* 线程函数 */
void * revThreadHandler(void *arg)
{
	/* select var */
	fd_set fds;
	struct timeval tv;
	int res = 0;
	int addr_sz = sizeof(struct sockaddr);

	while(1)
	{
		/* 2.接收数据 */
		/* select initial */
		FD_ZERO(&fds);
		FD_SET(src_fd,&fds);
		/* time out */
		tv.tv_sec	= 3;
		tv.tv_usec	= 0;
		
		res = select(src_fd+1,&fds,NULL,NULL,&tv);
		CLI_PRINT("select res: %d\n",res);
		switch(res)
		{
			case -1:
				CLI_PRINT("select receive error.\n");
				break;

			case 0:
				CLI_PRINT("select receive time out.\n");
				break;

			default:
				if(FD_ISSET(src_fd,&fds) == 1)
				{
					//FD_ZERO(&fds);
					CLI_PRINT("get response data.\n");
					/* 处理接收的数据 */			
					if(rev_msg == NULL)
					{
						rev_msg = (PST_MSG)malloc(sizeof(ST_MSG));
						if(rev_msg == NULL)
						{
							CLI_PRINT("rev_msg malloc error.\n");
							break;
						}
					}
					memset(rev_msg,0,sizeof(ST_MSG));
					memset(&recv_addr,0,addr_sz);
					int len = recvfrom(src_fd,rev_msg,sizeof(ST_MSG),0,\
										(struct sockaddr *)&recv_addr,&addr_sz);
					if(len < 0)
					{
						CLI_PRINT("receive data error.\n");
						break;
					}
					else
					{
						if(rev_msg->mType == TYPE_MSG_RPT_ERR)
						{
							CLI_PRINT("Get error string: %s\n",rev_msg->buf);
						}

					}				
				}
				break;

		}		
	}
	
	return ((void *)0);
}

/* 主程序 */
int main(int argc,char *argv[])
{	

	int retval = RET_FAIL;
	
	/* 1.初始化upd */
	printf("######### Client socket initial #########\n");
	retval = initUpdConn();

	/* 2.启动接收线程 */
	printf("######### receive thread create #########\n");	
	pthread_t pid;
	int ret = pthread_create(&pid,NULL,(void *)revThreadHandler,NULL);
	if(ret != 0)
	{
		CLI_PRINT("receive thread create fail.\n");
	}
	CLI_PRINT("receive thread setup.\n");

	/* 3.组头包 */
	printf("######### making package #########\n");	
	PST_FILE_HEADER	head = (PST_FILE_HEADER)malloc(sizeof(ST_FILE_HEADER));
	if(head == NULL)
	{
		CLI_PRINT("Malloc error.\n");
		close(src_fd);
		return RET_FAIL;
	}
	CLI_PRINT("Malloc done.\n");	
	memset(head,0,sizeof(ST_FILE_HEADER));
	memcpy(head->name,"pisc_v1.0",strlen("pisc_v1.0"));
	head->type		= TYPE_PISC;	// 文件类型,区别于消息类型
	head->size		= 10240;		// 乱填的数据1M
	head->version	= 1;			// 乱填的数据
	head->crc		= 555;			// 乱填的数据
	CLI_PRINT("memcpy done.\n");
	PST_MSG message = makePack(TYPE_MSG_HEAD,&head);
	CLI_PRINT("message type: %02x\n",(int)message->mType);
	if(message = NULL)
	{
		CLI_PRINT("Make package fail..\n");		
		return RET_FAIL;
	}

	/* 4.主函数负责发送数据 */
	printf("######### Ready to send data #########\n");
	fd_set wfds;
	struct timeval wtv;
	while(1)
	{
		FD_ZERO(&wfds);
		FD_SET(src_fd,&wfds);
		/* time out */
		wtv.tv_sec	= 3;
		wtv.tv_usec	= 0;
		if(select(src_fd+1,NULL,&wfds,NULL,&wtv) > 0)
		{
			if(FD_ISSET(src_fd,&wfds) == 1)
			{
				/* 1.发送头数据 */
				CLI_PRINT("Send fd: %d\n",src_fd);
				retval = sendto(src_fd,message,sizeof(ST_MSG),0,\
								(struct sockaddr *)&dst_addr,sizeof(struct sockaddr));
				if(retval < 0)
				{
					CLI_PRINT("Send head data error.\n");
				}
				CLI_PRINT("Client send %d bytes head data.\n",retval);

			}
		}
		else
		{
			CLI_PRINT("fd can not be written.\n");
		}

		/* 1.1发送文件包数据 */
#if 0		
		retval = sendto(src_fd,message,sizeof(ST_MSG),0,(struct sockaddr *)&dst_addr,addr_sz);
		if(retval < 0)
		{
			CLI_PRINT("Send content data error.\n");
		}
		CLI_PRINT("Client send %d bytes content data.\n",retval);
#endif

		sleep(2);
	}

	free(head);
	free(msg);
	return RET_OK;
}




