/* ################################################################
					������������������
		��;�������������� lcd/pisc/brodcast�ȳ���
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


/* ���Ա��� */
#define DEBUG_UPDATE

#ifdef 	DEBUG_UPDATE
#define DEBUG(format, ...) printf("FILE: "__FILE__", LINE: %d: "format, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif

/* ����һЩ���� */
#define BUF_SIZE (1024)

/* ϵͳ����ʱ��Ӧ�ñ����һЩȫ�ֵ�״̬���� */
static PST_SYS_PARAM g_sysParam; 

/* sockaddr_in size */
uint16 addr_sz = sizeof(struct sockaddr_in);

/* �ļ�����buf */
#define REVROOT 	"/update/"
ST_FILE_PKG fileBuf[BUF_SIZE];// ��������
int fb_idx = 0;	// ���������±�

/* ȫ���ϱ����� */
PST_MSG rpt_msg = NULL;


/* ����(����ŵ�ͷ�ļ���) */
int initDefualtParam(void);
int initUdpServ(void);
int fdIsReadable(void);
int handlePack(int fd);
void closeResource(void);
int rptRevErr(char *errStr);
int rptRevProc(char *procStr);
int writeLocalFile(void);



/* ����Ĭ�����в��� */
int initDefualtParam(void)
{
	/* ��ʼ������������±� */
	fb_idx = 0;
	memset(fileBuf,0,sizeof(fileBuf));

	g_sysParam = malloc(sizeof(ST_SYS_PARAM));
	if(g_sysParam == NULL)
	{
		DEBUG("g_sysParam malloc err..\n");
	}
	memset(g_sysParam,0,sizeof(ST_SYS_PARAM));
	g_sysParam->revPkgNum	= 0;	// ��ǰ�ѽ��յİ���
	g_sysParam->revFlag		= 0;	// 1Ϊ��ʼ/0δ��ʼ�����
	g_sysParam->udp_fd		= 0;	// udp���
	memset(&g_sysParam->dst_addr,0,addr_sz);
	strncpy((char *)g_sysParam->curVer,"1.0",strlen("1.0"));	
	
	return RET_OK;
}


 /* ��ʼ��UDP���� */
int initUdpServ(void)
{
	int fd;
	int retVal;
	struct sockaddr_in serv_addr;

	/* �����׽��� */
	DEBUG("Ready to create socket..\n");
	fd = socket(AF_INET,SOCK_DGRAM,0);
	if(fd <0)
	{
		DEBUG("UDP init fail..\n");
		return RET_FAIL;
	}
	/* ���׽��� */
	DEBUG("Ready to bind socket..\n");
	memset(&serv_addr,0,addr_sz);
	serv_addr.sin_family 		= AF_INET;
//	serv_addr.sin_addr.s_addr	= inet_addr("192.168.16.181");// �󶨱�����ַ
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);// �󶨱�����ַ	
	serv_addr.sin_port			= htons(UDP_PORT);
	retVal = bind(fd,(struct sockaddr *)&serv_addr,addr_sz);
	if(retVal < 0)
	{
		DEBUG("Bind udp fd fail.\n");
		return RET_FAIL;
	}

	/* ����ȫ�� fd */
	g_sysParam->udp_fd = fd;

	return RET_OK;
}


/* ����fd */
int fdIsReadable(void)
{
	/* �ȴ��ͻ������� */
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
		/* ���sock_fd */
		FD_ZERO(&fds);
		FD_SET(sock_fd,&fds);
		/* ���õȴ�ʱ��2s */
		tv.tv_sec	= 2;
		tv.tv_usec	= 0;
		retVal = select(sock_fd+1,&fds,NULL,NULL,&tv);
		switch(retVal)
		{
			case -1:
				DEBUG("select err.\n");
				if(g_sysParam->revFlag == 1)
				{
					/* �ϱ����� */
					rptRevErr("receive data err");
					/* ���־ */
					g_sysParam->revFlag = 0;
					/* ����ѽ��յ�buf���ļ� */
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
				/* ���ӽ��ճ�ʱ�ж� */
				if(g_sysParam->revFlag == 1)
				{
					/* �ϱ����� */
					rptRevErr("receive data time out");
					/* ���־ */
					g_sysParam->revFlag = 0;
					/* ����ѽ��յ�buf���ļ� */
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


/* �����봦������ */
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
		/* �жϽ������ݵ�����(������Ҫ�ж��½��ճ���) */
		DEBUG("revMsg->mType: %d\n",revMsg->mType);
		switch(revMsg->mType)
		{
			case TYPE_MSG_HEAD:
				DEBUG("TYPE_MSG_HEAD recieve.\n");
				if(g_sysParam->revFlag == 1)
				{
					/* ��ֹ�������ݰ��Ĺ����в�Ӧ�ý��յ����ݰ�ͷ */
					DEBUG("receive head pack while receiving data.\n");
					rptRevErr("get head pack while receiving error");
					g_sysParam->revFlag = 0;
					/* ���֮ǰ���յ�buf������ */
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
				/* �����Ľ��հ�ͷ������ */
				memcpy(&g_sysParam->fileHead,(PST_FILE_HEADER)revMsg->buf,sizeof(ST_FILE_HEADER));	
				/* ��ӡ��ʾͷ�������� */
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
				/* �ж������ļ�����,�汾�ž����Ƿ�������� */
				if((g_sysParam->fileHead.type == TYPE_LCD) && (g_sysParam->fileHead.version > 0))//ʵ�ʰ汾Ҫ�ӵײ�ӿڻ��
				{
					if(g_sysParam->revFlag == 0)
					{
						/* ������°汾�Ļ���λ��־ */					
						g_sysParam->revFlag	 = 1;
						/* ������λ����udp������Ϣ */
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
					/* ��ʼ���� */
					if(fb_idx < BUF_SIZE)
					{
						/* д�ڴ� */
						DEBUG("write date to buf\n");
						memcpy(&fileBuf[fb_idx],revMsg->buf,sizeof(ST_FILE_PKG));	
						fb_idx ++;
						if(fb_idx >= BUF_SIZE || (fileBuf[fb_idx-1].curPkgNum == fileBuf[fb_idx-1].totalPkgNum))
						{
							/* ��1024���߷���д�ļ� */
							//writeFile();
							/* ���buf */
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


/* ���̵߳�ִ�г��� */
void* threadHandler(void *arg)
{
	/* ���ݽ������� */
	fdIsReadable();
	return ((void *)0);
}

/* ��ʼ�������߳� */
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


/* ��ɨս��,���malloc/fd����Դ */
void closeResource(void)
{
	/* ����free clear */
	free(g_sysParam);
}


/* �ϱ����մ��� */
int rptRevErr(char *errStr)
{
	int retval = RET_FAIL;
	
	/* ��̨��ӡ���� */
	printf("=========== %s ===========\n",errStr);
	/* 1.�ϱ����� */
	/* 1.1��֯�������Ϣ */
	if(rpt_msg == NULL)
	{
		rpt_msg = (PST_MSG)malloc(sizeof(ST_MSG));
		if(rpt_msg == NULL)
			DEBUG("malloc err\n");
	}
	memset(rpt_msg,0,sizeof(ST_MSG));
	rpt_msg->mType = TYPE_MSG_RPT_ERR;
	memcpy(rpt_msg->buf,errStr,strlen(errStr));
	/* �ϱ����� */
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

/* �ϱ����ս��� */
int rptRevProc(char *procStr)
{
	int retval = RET_FAIL;
	
	/* ��̨��ӡ���� */	
	printf("=========== %s ===========\n",procStr);	
	/* 1.�ϱ����� */
	/* 1.1��֯�����ϱ����� */
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
	/* �ϱ����� */
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

/* �����յ�������д���ļ� */
int writeLocalFile(void)
{
	int retval = RET_FAIL;
	/* �������ڵ�����ȡ��,����,д�뻺�� */
	
	
	return retval;
}


/* ������(����ϴ����ʱ��Ӧ�ø�Ϊ�߳�) */
int main(int argc,char *argv[])
{
	int ret = RET_FAIL;

	printf("################ update program ###############\n");
	
	/* 1.ȫ��״̬������ʼ�� */
	printf("******** param initial *********\n");
	ret = initDefualtParam();

	/* 2.��ʼ��udpserver */
	printf("********* udp initial **********\n");	
	ret = initUdpServ();	

	/* 3.������λ�����͵�upd���� */
	printf("********* thread initial ********\n");	
	ret = initUpdateThread();
	/* �����̲����˳� */
	while(1);
	
	return ret;
}





