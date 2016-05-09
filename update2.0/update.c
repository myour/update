/* ################################################################
					程序在线升级主程序
		用途：用来在线升级 lcd/pisc/brodcast等程序
		Author:chenrong 2016.03.25
################################################################# */

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
#include <netinet/in.h>

#include "update.h"


/* 调试变量 */
#define DEBUG_UPDATE

#ifdef 	DEBUG_UPDATE
#define DEBUG(format, ...) printf("FILE: "__FILE__", LINE: %d: "format, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif

/* 定义一些变量 */
#define BUF_SIZE (1024)

/* 系统运行时候应该保存的一些全局的状态变量 */
static PST_SYS_PARAM g_sysParam; 

/* sockaddr_in size */
uint16 addr_sz = sizeof(struct sockaddr_in);


/* 申明(后面放到头文件中) */
int handlePack(void);



/* 设置默认运行参数 */
int initDefualtParam(void)
{
	g_sysParam = malloc(sizeof(ST_SYS_PARAM));
	if(g_sysParam == NULL)
	{
		DEBUG("g_sysParam malloc err..\n");
	}
	memset(g_sysParam,0,sizeof(ST_SYS_PARAM));
	g_sysParam->revPkgNum	= 0;	// 当前已接收的包数
	g_sysParam->sendFlag	= 0;	// 1为开始/0未开始或结束
	g_sysParam->udp_fd		= 0;	// udp句柄
	strncpy((char *)g_sysParam->curVer,"1.0",strlen("1.0"));	
	
	return RET_OK;
}

 /* 初始化UDP服务 */
int initUdpServ(void)
{
	int fd;
	int retVal;
	struct sockaddr_in serv_addr;

	/* 创建套接字 */
	DEBUG("Ready to create socket..\n");
	fd = socket(AF_INET,SOCK_DGRAM,0);
	if(fd <0)
	{
		DEBUG("UDP init fail..\n");
		return RET_FAIL;
	}
	/* 绑定套接字 */
	DEBUG("Ready to bind socket..\n");
	memset(&serv_addr,0,addr_sz);
	serv_addr.sin_family 		= AF_INET;
//	serv_addr.sin_addr.s_addr	= inet_addr("192.168.16.181");// 绑定本机地址
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);// 绑定本机地址	
	serv_addr.sin_port			= htons(UDP_PORT);
	retVal = bind(fd,(struct sockaddr *)&serv_addr,addr_sz);
	if(retVal < 0)
	{
		DEBUG("Bind udp fd fail.\n");
		return RET_FAIL;
	}

	/* 设置全局 fd */
	g_sysParam->udp_fd = fd;

	return RET_OK;
}


/* 使用selcet模式监听fd */
int fdIsReadable(void)
{
	/* 等待客户端连接 */
	int sock_fd;
	fd_set fds;
	struct timeval tv;
	int retVal;

	if(g_sysParam->udp_fd < 0)
	{
		DEBUG("somethig rong with udp fd.\n");
		return RET_FAIL;
	}
	sock_fd = g_sysParam->udp_fd;	
	DEBUG("Wait for recieving data..\n");
	while(1)
	{
		/* 监控sock_fd */
		FD_ZERO(&fds);
		FD_SET(sock_fd,&fds);
		/* 设置等待时间2s */
		tv.tv_sec	= 2;
		tv.tv_usec	= 0;
		retVal = select(sock_fd+1,&fds,NULL,NULL,&tv);
		switch(retVal)
		{
			case -1:
				DEBUG("select err.\n");
				break;
				
			case 0:
				DEBUG("select time out..\n");
				break;
				
			default:
				if(FD_ISSET(sock_fd,&fds) == 1)
				{
					FD_ZERO(&fds);
					DEBUG("UDP sock can be read.\n");
					handlePack();
				}
					break;
		}
	}		

	return retVal;
}


/* 接收与处理数据 */
int handlePack(void)
{
	int len	 	= 0;
	int sock_fd = g_sysParam->udp_fd;
	PST_MSG revMsg;	
	int retVal	= RET_OK;	
	//char buf[3072] = {0};
	struct sockaddr_in cli_addr;

	revMsg = (PST_MSG)malloc(sizeof(ST_MSG));
	if(revMsg == NULL)
		return RET_FAIL;
	memset(revMsg,0,sizeof(ST_MSG));
	memset(&cli_addr,0,addr_sz);	
	len = recvfrom(sock_fd,revMsg,sizeof(ST_MSG),0,(struct sockaddr *)&cli_addr,&addr_sz);
	if(len < 0)
	{
		DEBUG("recvfrom err..\n");
		free(revMsg);
		retVal = RET_FAIL;
	}
	else
	{
		/* 判断接收数据的类型 */
		switch(revMsg->mType)
		{
			case TYPE_MSG_HEAD:
				/* 接收到文件头消息 */
				DEBUG("TYPE_MSG_HEAD recieve.\n");
				memcpy(&g_sysParam->fileHead,(PST_FILE_HEADER)revMsg->buf,sizeof(ST_FILE_HEADER));	
				/* 判断升级文件类型,版本号决定是否进行升级 */
				if(g_sysParam->fileHead.type == TYPE_LCD || g_sysParam->fileHead.version > 1)//实际版本要从底层接口获得
				{
					/* 置为开始升级标志 */
					g_sysParam->sendFlag = 1;
					/* 如果当前传输的版本比原来的旧,后期给上位机回传错误消息 */
					// sendto();
				}	
					
				if(g_sysParam->fileHead.type == TYPE_PISC || g_sysParam->fileHead.version > 1)//实际版本要从底层接口获得
				{
					/* 置为开始升级标志 */
					g_sysParam->sendFlag = 1;
					DEBUG("Client send file type is: PISC\n");
				}

				if(g_sysParam->fileHead.type == TYPE_BRODCAST || g_sysParam->fileHead.version > 1)//实际版本要从底层接口获得
				{
					/* 置为开始升级标志 */
					g_sysParam->sendFlag = 1;
				}

				break;
			
			case TYPE_MSG_CONTENT:
				DEBUG("TYPE_MSG_CONTENT recieve.\n");			
				/* 接收到文件内容消息,只处理有头文件的信息 */ 
				if(g_sysParam->sendFlag == 1)
				{
					/* 开始接收 */
		
					/* 判断接收数据是否结束 */
					
				}
				break;
			case TYPE_MSG_RPT_PROC:
				DEBUG("TYPE_MSG_RPT_PROC recieve.\n");			
				DEBUG("do null thing.\n");
				break;

			default:
				DEBUG("Recive message type error..\n");
				break;
		}
	}
	return retVal;
}


/* 多线程的执行程序 */
void* updateHandler(void *arg)
{
	/* 数据解析处理 */
	fdIsReadable();
	
	return ((void *)0);
}

/* 初始化升级线程 */
int initUpdateThread(void)
{
	pthread_t pfd;
	int ret;

	/* ready to create pthread */
	ret = pthread_create(&pfd,NULL,(void *)updateHandler,NULL);
	if(ret != 0)
	{
		DEBUG("Thread create fail.\n");
		ret = RET_FAIL;
	}
	
	return ret;
}


/* 打扫战场,清除malloc/fd等资源 */
void closeResource(void)
{
	/* 各种free clear */
	free(g_sysParam);
}


/* 主程序(后面合代码的时候应该改为线程) */
int main(int argc,char *argv[])
{
	int ret = RET_FAIL;

	printf("################ update program ###############\n");
	
	/* 1.全局状态变量初始化 */
	printf("******** param initial *********\n");
	ret = initDefualtParam();

	/* 2.初始化udpserver */
	printf("********* udp initial **********\n");	
	ret = initUdpServ();	

	/* 3.监听上位机发送的upd报文 */
	printf("********* thread initial ********\n");	
	ret = initUpdateThread();

#if 0	
	if(fdIsReadable())
	{
		/* 接收报文 */
		//retVal = handlePack();
		DEBUG("Return true.\n");
	}
	else
	{
		DEBUG("Return false.\n");
	}
#endif
	while(1);
	return ret;
}











