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

/* 文件接收buf */
#define REVROOT 	"/update/"
ST_FILE_PKG fileBuf[BUF_SIZE];// 缓存数组
int fb_idx = 0;	// 缓存数组下标

/* 全局上报缓存 */
PST_MSG rpt_msg = NULL;


/* 申明(后面放到头文件中) */
int initDefualtParam(void);
int initUdpServ(void);
int fdIsReadable(void);
int handlePack(int fd);
void closeResource(void);
int rptRevErr(char *errStr);
int rptRevProc(char *procStr);
int writeLocalFile(void);



/* 设置默认运行参数 */
int initDefualtParam(void)
{
	/* 初始化缓存数组和下标 */
	fb_idx = 0;
	memset(fileBuf,0,sizeof(fileBuf));

	g_sysParam = malloc(sizeof(ST_SYS_PARAM));
	if(g_sysParam == NULL)
	{
		DEBUG("g_sysParam malloc err..\n");
	}
	memset(g_sysParam,0,sizeof(ST_SYS_PARAM));
	g_sysParam->revPkgNum	= 0;	// 当前已接收的包数
	g_sysParam->revFlag		= 0;	// 1为开始/0未开始或结束
	g_sysParam->udp_fd		= 0;	// udp句柄
	memset(&g_sysParam->dst_addr,0,addr_sz);
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


/* 监听fd */
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
				if(g_sysParam->revFlag == 1)
				{
					/* 上报错误 */
					rptRevErr("receive data err");
					/* 清标志 */
					g_sysParam->revFlag = 0;
					/* 清空已接收的buf和文件 */
					memset(fileBuf,0,BUF_SIZE);
					fb_idx = 0;
					char cmdBuf[256] = {0};
					sprintf(cmdBuf,"rm -rf %s%s",REVROOT,g_sysParam->fileHead.name);
					if(system(cmdBuf) <0)
					{
						DEBUG("delete local buffer file fail.\n");
					}

				}
				break;
				
			case 0:
				DEBUG("select time out..\n");
				/* 添加接收超时判断 */
				if(g_sysParam->revFlag == 1)
				{
					/* 上报错误 */
					rptRevErr("receive data time out");
					/* 清标志 */
					g_sysParam->revFlag = 0;
					/* 清空已接收的buf和文件 */
					memset(fileBuf,0,BUF_SIZE);
					fb_idx = 0;
					char cmdBuf[256] = {0};
					sprintf(cmdBuf,"rm -rf %s%s",REVROOT,g_sysParam->fileHead.name);
					if(system(cmdBuf) <0)
					{
						DEBUG("delete local buffer file fail.\n");
					}

				}
				
				break;
				
			default:
				if(FD_ISSET(sock_fd,&fds) == 1)
				{
					FD_ZERO(&fds);
					DEBUG("UDP sock can be read.\n");
					handlePack(sock_fd);
				}
				break;
		}
	}		

	return retVal;
}


/* 接收与处理数据 */
int handlePack(int fd)
{
	PST_MSG revMsg;	
	int len	 	= 0;
	int sock_fd = fd;	
	int retVal	= RET_OK;	
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
		/* 判断接收数据的类型(可能需要判断下接收长度) */
		DEBUG("revMsg->mType: %d\n",revMsg->mType);
		switch(revMsg->mType)
		{
			case TYPE_MSG_HEAD:
				DEBUG("TYPE_MSG_HEAD recieve.\n");
				if(g_sysParam->revFlag == 1)
				{
					/* 防止接收数据包的过程中不应该接收到数据包头 */
					DEBUG("receive head pack while receiving data.\n");
					rptRevErr("get head pack while receiving error");
					g_sysParam->revFlag = 0;
					/* 清空之前接收的buf等数据 */
					memset(fileBuf,0,BUF_SIZE);
					fb_idx = 0;
					DEBUG("clean file receive buffer.\n");
					char cmdBuf[256] = {0};
					sprintf(cmdBuf,"rm -rf %s%s",REVROOT,g_sysParam->fileHead.name);
					if(system(cmdBuf) <0)
					{
						DEBUG("delete local buffer file fail.\n");
					}
					DEBUG("clean local buffer file.\n");
					break;
					
				}
				/* 正常的接收包头的流程 */
				memcpy(&g_sysParam->fileHead,(PST_FILE_HEADER)revMsg->buf,sizeof(ST_FILE_HEADER));	
				/* 打印显示头包的内容 */
				printf("############# get head detial #################\n");
#if 0
				printf("name: %s\n",g_sysParam->fileHead.name);
				printf("size: %d\n",g_sysParam->fileHead.size);
				printf("type: %d\n",(int)g_sysParam->fileHead.type);				
				printf("ver: %d\n",g_sysParam->fileHead.version);
				printf("crc: %d\n",g_sysParam->fileHead.crc);
#else				
				printf("name: %s\n",g_sysParam->fileHead.name);
				printf("size: %d\n",g_sysParam->fileHead.size);
				printf("type: %d\n",(int)g_sysParam->fileHead.type);				
				printf("ver: %d\n",g_sysParam->fileHead.version);
				printf("crc: %d\n",g_sysParam->fileHead.crc);
#endif	
				/* 判断升级文件类型,版本号决定是否进行升级 */
				if((g_sysParam->fileHead.type == TYPE_LCD) && (g_sysParam->fileHead.version > 0))//实际版本要从底层接口获得
				{
					if(g_sysParam->revFlag == 0)
					{
						/* 如果是新版本的话置位标志 */					
						g_sysParam->revFlag	 = 1;
						/* 保存上位机的udp连接信息 */
						g_sysParam->dst_addr = cli_addr;
					}
					
				}	
					
				if((g_sysParam->fileHead.type == TYPE_PISC) && (g_sysParam->fileHead.version > 0))
				{
					DEBUG("Client send file type is: PISC\n");				
					if(g_sysParam->revFlag == 0)
					{
						g_sysParam->revFlag  = 1;
						g_sysParam->dst_addr = cli_addr;
					}

				}

				if((g_sysParam->fileHead.type == TYPE_BRODCAST) && (g_sysParam->fileHead.version > 0))
				{
					DEBUG("Get broadcast\n");
				}
				break;
			
			case TYPE_MSG_CONTENT:
				DEBUG("TYPE_MSG_CONTENT recieve.\n");			
				if(g_sysParam->revFlag == 1)
				{
					g_sysParam->dst_addr = cli_addr;
					/* 开始接收 */
					if(fb_idx < BUF_SIZE)
					{
						/* 写内存 */
						DEBUG("write date to buf\n");
						memcpy(&fileBuf[fb_idx],revMsg->buf,sizeof(ST_FILE_PKG));	
						fb_idx ++;
						if(fb_idx >= BUF_SIZE || (fileBuf[fb_idx-1].curPkgNum == fileBuf[fb_idx-1].totalPkgNum))
						{
							/* 满1024或者发完写文件 */
							//writeFile();
							/* 清空buf */
							memset(fileBuf,0,sizeof(fileBuf));
							fb_idx = 0;
						}
					}
					
				}
				break;

			default:
				DEBUG("Recive message type error..\n");
				break;
		}
	}
	return retVal;
}


/* 多线程的执行程序 */
void* threadHandler(void *arg)
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
	ret = pthread_create(&pfd,NULL,(void *)threadHandler,NULL);
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


/* 上报接收错误 */
int rptRevErr(char *errStr)
{
	int retval = RET_FAIL;
	
	/* 后台打印进度 */
	printf("=========== %s ===========\n",errStr);
	/* 1.上报错误 */
	/* 1.1组织错误包信息 */
	if(rpt_msg == NULL)
	{
		rpt_msg = (PST_MSG)malloc(sizeof(ST_MSG));
		if(rpt_msg == NULL)
			DEBUG("malloc err\n");
	}
	memset(rpt_msg,0,sizeof(ST_MSG));
	rpt_msg->mType = TYPE_MSG_RPT_ERR;
	memcpy(rpt_msg->buf,errStr,strlen(errStr));
	/* 上报错误 */
	int result = sendto(g_sysParam->udp_fd,rpt_msg,sizeof(ST_MSG),0,(struct sockaddr *)&g_sysParam->dst_addr,addr_sz);
	if(result < 0)
	{
		DEBUG("report err fail,please check.\n");
		retval = RET_FAIL;
	}
	else
	{
		retval = RET_OK;
	}

	free(rpt_msg);
	return retval;
}

/* 上报接收进度 */
int rptRevProc(char *procStr)
{
	int retval = RET_FAIL;
	
	/* 后台打印进度 */	
	printf("=========== %s ===========\n",procStr);	
	/* 1.上报进度 */
	/* 1.1组织进度上报报文 */
	if(rpt_msg == NULL)
	{
		rpt_msg = malloc(sizeof(ST_MSG));
		if(rpt_msg == NULL)
			DEBUG("malloc err\n");
	}
	DEBUG("malloc err\n");
	memset(rpt_msg,0,sizeof(ST_MSG));
	rpt_msg->mType = TYPE_MSG_RPT_PROC;
	memcpy(rpt_msg->buf,procStr,strlen(procStr));
	/* 上报错误 */
	int result = sendto(g_sysParam->udp_fd,rpt_msg,sizeof(ST_MSG),0,(struct sockaddr *)&g_sysParam->dst_addr,addr_sz);
	if(result < 0)
	{
		DEBUG("report process fail,please check.\n");
		retval = RET_FAIL;
	}
	else
	{
		retval = RET_OK;
	}

	free(rpt_msg);

	return retval;
}

/* 将接收到的数据写入文件 */
int writeLocalFile(void)
{
	int retval = RET_FAIL;
	/* 将缓存内的数据取出,解析,写入缓存 */
	
	
	return retval;
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
	/* 主进程不能退出 */
	while(1);
	
	return ret;
}






