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
#include "crc.h"


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
#define REVROOT 		"/update/"
#define REPLACEROOT 	"/update/old/"

/* 缓存数组 */
ST_FILE_PKG fileBuf[BUF_SIZE];
/* 缓存数组下标 */
int fb_idx = 0;	
/* 数据接收结束标志 */
int revOver = 0;

/* 全局上报缓存 */
PST_MSG rpt_msg = NULL;
/* 接收 */
PST_MSG revMsg = NULL; 


/* 申明(后面放到头文件中) */
int initDefualtParam(void);
int initUdpServ(void);
int isFdReadable(void);
int handlePack(int fd);
void closeResource(void);
int rptRevErr(char *errStr);
int rptRevProc(char *procStr);
int writeLocalFile(void);
int checkRevFile(void);
int replaceOldFile(char *filePath);
int rebootSystem(void);
int delRecvFile(void);



/*####################################
	函数功能: 	设置默认运行参数
	入参:		无
#####################################*/
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

/*####################################
	函数功能: 	初始化UDP服务
	入参:		无
#####################################*/
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
	//serv_addr.sin_addr.s_addr	= inet_addr("192.168.16.181");// 绑定本机地址
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


/*####################################
	函数功能: 	监听fd
	入参:		无
#####################################*/
int isFdReadable(void)
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
		/* 设置等待时间5s */
		tv.tv_sec	= 5;
		tv.tv_usec	= 0;
		retVal = select(sock_fd+1,&fds,NULL,NULL,&tv);
		switch(retVal)
		{
			case -1:
				DEBUG("receive err.\n");
				if(g_sysParam->revFlag == 1)
				{
					/* 上报错误 */
					rptRevErr("receive data err");
					DEBUG("receive data error ,clean up received data!!!\n");
					/* 清标志 */
					g_sysParam->revFlag = 0;
					/* 清空之前接收的buf等数据 */
					delRecvFile();

				}
				break;
				
			case 0:
				DEBUG("receive time out..\n");
				/* 添加接收超时判断 */
				if(g_sysParam->revFlag == 1)
				{
					/* 上报错误 */
					rptRevErr("receive data time out");
					DEBUG("receive data time out ,clean up received data!!!\n");
					/* 清标志 */
					g_sysParam->revFlag = 0;
					/* 清空之前接收的buf等数据 */
					delRecvFile();
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


/*####################################
	函数功能: 	接收与处理数据
	入参:		int fd
#####################################*/
int handlePack(int fd)
{
	int len	 	= 0;
	int sock_fd = fd;	
	int retVal	= RET_OK;	
	struct sockaddr_in cli_addr;

	if(revMsg == NULL)
	{
		revMsg = (PST_MSG)malloc(sizeof(ST_MSG));
		if(revMsg == NULL)
		{
			DEBUG("revMsg malloc fail.\n");
			return RET_FAIL;		
		}

	}
		
	memset(revMsg,0,sizeof(ST_MSG));
	memset(&cli_addr,0,addr_sz);	
	len = recvfrom(sock_fd,revMsg,sizeof(ST_MSG),0,(struct sockaddr *)&cli_addr,&addr_sz);
	if(len < 0)
	{
		DEBUG("recvfrom err..\n");
		free(revMsg);
		revMsg = NULL;
		retVal = RET_FAIL;
	}
	else
	{
		/* 判断接收数据的类型 */
		DEBUG("revMsg->mType: %d\n",revMsg->mType);
		switch(revMsg->mType)
		{
			case TYPE_MSG_HEAD:
				printf("###### TYPE_MSG_HEAD recieve #####\n");
				if(g_sysParam->revFlag == 1)
				{
					/* 接收数据包的过程中不应该接收到数据包头 */
					rptRevErr("get head pack while receiving error");
					DEBUG("receive head clash,clean up received data!!\n");
					g_sysParam->revFlag = 0;
					/* 清空之前接收的buf等数据 */
					delRecvFile();
					DEBUG("clean local buffer file.\n");
					break;
					
				}
			
				/* 正常的接收包头的流程 */	
				memcpy(&(g_sysParam->fileHead),(PST_FILE_HEADER)revMsg->buf,sizeof(ST_FILE_HEADER));	
#if 0
				DEBUG("******** get buf detail ********\n");		
				int i = 0;
				for(i=128;i<152;i++)
				{
					DEBUG("revMsg->buf[%d] = %d\n",i,revMsg->buf[i]);
				}
#endif				
				/* 打印显示头包的内容 */
				DEBUG("******** get head detial ********\n");
				DEBUG("name: %s\n",g_sysParam->fileHead.name);
				DEBUG("size: %d\n",g_sysParam->fileHead.size);
				DEBUG("type: %d\n",(int)g_sysParam->fileHead.type);				
				DEBUG("ver:	 %c\n",g_sysParam->fileHead.version);
				DEBUG("crc:	 %lu\n",g_sysParam->fileHead.crc);
	
				/* 判断升级文件类型,版本号决定是否进行升级(实际版本要从底层接口获得) */
				/* lcd 显示屏更新 */
				if((g_sysParam->fileHead.type == TYPE_LCD) && (g_sysParam->fileHead.version > 0))
				{
					DEBUG("Client send file type is: LCD\n");
					if(g_sysParam->revFlag == 0)
					{
						/* 如果是新版本的话置位标志 */					
						g_sysParam->revFlag	 = 1;
						/* 保存上位机的udp连接信息 */
						g_sysParam->dst_addr = cli_addr;	
					}
					
				}	

				/* 中央控制器更新 */	
				if((g_sysParam->fileHead.type == TYPE_PISC) && (g_sysParam->fileHead.version > 0))
				{
					DEBUG("Client send file type is: PISC\n");				
					if(g_sysParam->revFlag == 0)
					{
						g_sysParam->revFlag  = 1;
						g_sysParam->dst_addr = cli_addr;
					}
					
				}

				/* 广播控制盒更新 */
				if((g_sysParam->fileHead.type == TYPE_BRODCAST) && (g_sysParam->fileHead.version > 0))
				{
					DEBUG("Client send file type is: BRODCAST\n");
					if(g_sysParam->revFlag == 0)
					{
						g_sysParam->revFlag  = 1;
						g_sysParam->dst_addr = cli_addr;
					}
				}
				break;
			
			case TYPE_MSG_CONTENT:
				printf("###### TYPE_MSG_CONTENT recieve #####\n");
				if(g_sysParam->revFlag == 1)
				{					
					/* 取目的端udp地址 */
					g_sysParam->dst_addr = cli_addr;
					/* 开始接收 */					
					if(fb_idx < BUF_SIZE)
					{													
						DEBUG("writting date to buf\n");
						/* 写内存(每次写2+2+1024字节) */
						memcpy(&fileBuf[fb_idx],revMsg->buf,sizeof(ST_FILE_PKG));
						
						/* 防止上位机填错数据 */
						if((fileBuf[fb_idx].curPkgNum > fileBuf[fb_idx].totalPkgNum)&& fb_idx>=0)
						{
							DEBUG("receive data error! where totalPkgNum > curPkgNum.\n");
							/* 上报错误 */
							rptRevErr("receive data totalPkgNum > curPkgNum");
							/* 清标志 */
							g_sysParam->revFlag = 0;
							/* 清空之前接收的buf等数据 */
							DEBUG("content pack error,clean up received data!!!\n");
							delRecvFile();							
							break;
						}

						/* 上报进度 */
						char tmpbuf[32] = {0};
						sprintf(tmpbuf,"%d/%d",fileBuf[fb_idx].curPkgNum,fileBuf[fb_idx].totalPkgNum);
						rptRevProc(tmpbuf);
						
						/* 缓存写满 */
						if(fb_idx >= (BUF_SIZE-1) || (fileBuf[fb_idx].curPkgNum == fileBuf[fb_idx].totalPkgNum))
						{	
							if(fileBuf[fb_idx].curPkgNum == fileBuf[fb_idx].totalPkgNum)
							{
								revOver = 1;
							}
							
							/* 满1024或者发完写文件 */
							DEBUG("There is %d record in the ram.\n",fb_idx+1);
							DEBUG("Save buffer data into %s%s\n",REVROOT,g_sysParam->fileHead.name);
							if(writeLocalFile() == RET_FAIL)
							{
								DEBUG("Write %s%s fail,now try to write second time.\n",REVROOT,g_sysParam->fileHead.name);
								if(writeLocalFile() == RET_FAIL)
								{
									DEBUG("Write %s%s unsuccessful,please check out system!!!\n",REVROOT,g_sysParam->fileHead.name);
									/* 如果由于flash损坏或其它原因导致数据写入失败,会造成数据丢失 */
									//break;
								}
							}
							/* 数据传完 */
							if(fileBuf[fb_idx].curPkgNum == fileBuf[fb_idx].totalPkgNum)
							{
								/* 本次文件接收完毕 */
								DEBUG("File transmit over.\n");
								g_sysParam->revFlag = 0;
								/* 校验接收文件的正确性 */
								DEBUG("Try to check file.\n");
								int retCode = checkRevFile();
								if(RET_OK == retCode)
								{
									DEBUG("replace file done..\n");
								
									/* 1.替换旧文件 */
									if(replaceOldFile(REPLACEROOT) == RET_FAIL)
									{
										DEBUG("Can't replace old file.\n");
										/* 重试 */
										if(replaceOldFile(REPLACEROOT) == RET_FAIL)
											rptRevErr("Can't replace old file");
									}
									else
									{
										/* 2.重启系统 */
										//rebootSystem();
										/* 解压升级包,执行升级脚本 */
										
									}
								
								}
								else
								{
									/* 1.接收文件不正确上报错误 */
									rptRevErr("CheckSum received file error");
									/* 2.删除文件 */
									DEBUG("Delete received data.\n");									
								#if 0	
									// 调试使用
									/* 1.替换旧文件 */
									if(replaceOldFile(REPLACEROOT) == RET_FAIL)
									{
										DEBUG("Can't replace old file.\n");
										/* 重试 */
										if(replaceOldFile(REPLACEROOT) == RET_FAIL)
											rptRevErr("Can't replace old file");
									}
								#endif
									/* 删除 */
									delRecvFile();
								}	
							}
							/* 清空buf */
							memset(fileBuf,0,sizeof(fileBuf));
							fb_idx = 0;
						}
						else
						{
							/* 内存数组下标移动 */
							fb_idx ++;
						}
					}
				}
				else
				{
					DEBUG("g_sysParam->revFlag == 0 No head can't receive data.\n");
				}
				break;

			default:
				DEBUG("Recive message type error..\n");
				break;
		}
	}
	return retVal;
}


/*####################################
	函数功能: 	多线程的执行程序
	入参:		void *arg
######################################*/
void* threadHandler(void *arg)
{
	/* 数据解析处理 */
	isFdReadable();
	return ((void *)0);
}


/*####################################
	函数功能: 	初始化升级线程
	入参:		无
######################################*/
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


/*####################################
	函数功能: 	清除malloc/
				关闭全局fd等资源 
	入参:		char *errStr
######################################*/
void closeResource(void)
{
	/* 各种free clear */
	free(g_sysParam);
	//close();
}


/*####################################
	函数功能: 	上报接收错误
	入参:		char *errStr
######################################*/
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

	//free(rpt_msg);
	return retval;
}


/*####################################
	函数功能: 	上报接收进度
	入参:		char *procStr
######################################*/
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

	//free(rpt_msg);

	return retval;
}


/*####################################
 函数功能: 	将接收到的数据写入文件
 入参:		无
######################################*/
int writeLocalFile(void)
{
	int file_fd;
	int cnt = 0;	
	int w_num = 0;
	int retval = RET_OK;
	
	/* 1.文件如果不存在,创建文件否则追加 */
	char fileName[128] = {0};
	sprintf(fileName,"%s%s",REVROOT,g_sysParam->fileHead.name);
	file_fd = open(fileName,O_CREAT|O_WRONLY|O_APPEND,0666);
	if(file_fd < 0)
	{
		DEBUG("Open %s fail.\n",fileName);
		return RET_FAIL;
	}
	/* 2.写文件 */
	for(cnt=0; cnt<= fb_idx; cnt++)
	{				
		if((fb_idx == cnt) && (revOver ==1))// 需要判断是否到达文件末尾
		{
			revOver = 0;
			int last = g_sysParam->fileHead.size % SEND_BUF_LEN;
			if(last > 0)
			{
				if((w_num = write(file_fd,fileBuf[cnt].content,last)) < 0)
				{			
					DEBUG("Write %s error.\n",fileName);
					retval = RET_FAIL;
				}
			}
			else if(0 == last)
			{
				if((w_num = write(file_fd,fileBuf[cnt].content,SEND_BUF_LEN)) < 0)
				{			
					DEBUG("Write %s error.\n",fileName);
					retval = RET_FAIL;
				}	
			}

		}
		else
		{
			//if((w_num = write(file_fd,fileBuf[cnt].content,strlen((char *)fileBuf[cnt].content)-1)) < 0)
			if((w_num = write(file_fd,fileBuf[cnt].content,SEND_BUF_LEN)) < 0)
			{			
				DEBUG("Write %s error.\n",fileName);
				retval = RET_FAIL;
			}			
		}

		/* 调试用后期去掉 */
		DEBUG("Write bytes: %d\n",w_num);
		
	}
	/* 3.同步 */
	sync();
	close(file_fd);
	return retval;

}


/*####################################
	函数功能: 	校验接收的文件
	入参:		无
######################################*/
int checkRevFile(void)
{	
	unsigned long crcResult = 0;
	unsigned int fileSize = 0;
	int retval = RET_OK;
	int fd = 0;
	

	/* 1.文件大家校验 */
	/* 1.打开文件获取大小 */
	char filePath[128] = {0};
	sprintf(filePath,"%s%s",REVROOT,g_sysParam->fileHead.name);
	fd = open(filePath,O_RDONLY);
	if(fd < 0)
	{
		DEBUG("Can not open %s.\n",filePath);
		return RET_FAIL;
	}

	fileSize = lseek(fd,0L,SEEK_END);
	lseek(fd,0L,SEEK_SET);
	DEBUG("client tell us file size: %d\n",g_sysParam->fileHead.size);
	DEBUG("we get file size: %d\n",fileSize);
	if(g_sysParam->fileHead.size != fileSize)
	{
		DEBUG("Receive size not match send size.\n");
		retval = RET_FAIL;
	}
	else
	{
		DEBUG("Receive file: size ok.\n");
	}
	
	/* 2.crc32校验 */
	char *checkBuf = (char *)malloc(fileSize+2);// 多分配两个
	unsigned long iRet = read(fd,checkBuf,fileSize);
	if(iRet < 0)
	{
		DEBUG("Read receive file %s fail.\n",filePath);
		retval = RET_FAIL;		
	}
	else
	{
		DEBUG("Read %lu bytes data for crc check.\n",iRet);
	}
	crcResult = CRC32((unsigned char *)checkBuf,fileSize);
	DEBUG("receive crc code: %lu\n",g_sysParam->fileHead.crc);
	DEBUG("Calculate crc code: %lu\n",crcResult);
	if(g_sysParam->fileHead.crc != crcResult)
	{
		DEBUG("Calculate crc not match send file crc.\n");
		retval = RET_FAIL;
	}
	else
	{
		DEBUG("Crc check ok.\n");
	}

	free(checkBuf);
	checkBuf = NULL;
	close(fd);
	return retval;
}

/*####################################
	函数功能: 	替换原有文件
	入参:		无
######################################*/
int replaceOldFile(char *filePath)
{	
	int retval = RET_OK;
	char cmd[128] = {0};

	switch(g_sysParam->fileHead.type)
	{
		case TYPE_LCD:
			/* 1.删除原有文件 */
			sprintf(cmd,"rm -rf %s%s",filePath,"lcd");// "lcd"指文件后面按照实际来定义宏修改
			if(system(cmd) == -1)
				retval = RET_FAIL;
				
			/* 2.剪切新文件到运行目录 */
			sprintf(cmd,"mv %s%s %s%s",REVROOT,g_sysParam->fileHead.name,filePath,"lcd");
			if(system(cmd) == -1)
				retval = RET_FAIL;

			/* 3.设置运行权限 */
			sprintf(cmd,"chmod +x %s%s",filePath,"lcd");
			if(system(cmd) == -1)
				retval = RET_FAIL;	
				
			break;
			
		case TYPE_PISC:
			/* 1.删除原有文件 */
			sprintf(cmd,"rm -rf %s%s",filePath,"pisc");
			if(system(cmd) == -1)
				retval = RET_FAIL;
				
			/* 2.剪切新文件到运行目录 */
			sprintf(cmd,"mv %s%s %s%s",REVROOT,g_sysParam->fileHead.name,filePath,"pisc");
			if(system(cmd) == -1)
				retval = RET_FAIL;

			/* 3.设置运行权限 */
			sprintf(cmd,"chmod +x %s%s",filePath,"pisc");
			if(system(cmd) == -1)
				retval = RET_FAIL;	

			break;			

		case TYPE_BRODCAST:
			/* 1.删除原有文件 */
			sprintf(cmd,"rm -rf %s%s",filePath,"brodcast");
			if(system(cmd) == -1)
				retval = RET_FAIL;
				
			/* 2.剪切新文件到运行目录 */
			sprintf(cmd,"mv %s%s %s%s",REVROOT,g_sysParam->fileHead.name,filePath,"brodcast");
			if(system(cmd) == -1)
				retval = RET_FAIL;

			/* 3.设置运行权限 */
			sprintf(cmd,"chmod +x %s%s",filePath,"brodcast");
			if(system(cmd) == -1)
				retval = RET_FAIL;	

			break;
			
		default:
			DEBUG("There is no such type file.\n");
			break;
	}

	
	return retval;
}

/*####################################
	函数功能: 	系统重启
	入参:		无
######################################*/
int rebootSystem(void)
{	
	int retval = RET_OK;
	retval = system("reboot");
	if(retval == -1)
	{
		DEBUG("File update ok but can't reboot system.\n");
		retval = RET_FAIL;
	}
	return retval;
}


/*####################################
	函数功能: 	删除接收时的中间文件
	入参:		无
######################################*/
int delRecvFile(void)
{	
	int retval = RET_FAIL;

	/* 清空已接收的buf和文件 */
	memset(fileBuf,0,BUF_SIZE);
	fb_idx = 0;
	char cmdBuf[256] = {0};
	sprintf(cmdBuf,"rm -rf %s%s",REVROOT,g_sysParam->fileHead.name);
	retval = system(cmdBuf);
	if(retval == -1)
	{
		DEBUG("Delete recieve file fail.\n");
	}
	return retval;
}



/*#########################################
	函数功能: 	主程序(功能测试使用)
	入参:		int argc,
				char *argv[]
##########################################*/
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






