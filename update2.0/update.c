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


/* ����(����ŵ�ͷ�ļ���) */
int handlePack(void);



/* ����Ĭ�����в��� */
int initDefualtParam(void)
{
	g_sysParam = malloc(sizeof(ST_SYS_PARAM));
	if(g_sysParam == NULL)
	{
		DEBUG("g_sysParam malloc err..\n");
	}
	memset(g_sysParam,0,sizeof(ST_SYS_PARAM));
	g_sysParam->revPkgNum	= 0;	// ��ǰ�ѽ��յİ���
	g_sysParam->sendFlag	= 0;	// 1Ϊ��ʼ/0δ��ʼ�����
	g_sysParam->udp_fd		= 0;	// udp���
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


/* ʹ��selcetģʽ����fd */
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


/* �����봦������ */
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
		/* �жϽ������ݵ����� */
		switch(revMsg->mType)
		{
			case TYPE_MSG_HEAD:
				/* ���յ��ļ�ͷ��Ϣ */
				DEBUG("TYPE_MSG_HEAD recieve.\n");
				memcpy(&g_sysParam->fileHead,(PST_FILE_HEADER)revMsg->buf,sizeof(ST_FILE_HEADER));	
				/* �ж������ļ�����,�汾�ž����Ƿ�������� */
				if(g_sysParam->fileHead.type == TYPE_LCD || g_sysParam->fileHead.version > 1)//ʵ�ʰ汾Ҫ�ӵײ�ӿڻ��
				{
					/* ��Ϊ��ʼ������־ */
					g_sysParam->sendFlag = 1;
					/* �����ǰ����İ汾��ԭ���ľ�,���ڸ���λ���ش�������Ϣ */
					// sendto();
				}	
					
				if(g_sysParam->fileHead.type == TYPE_PISC || g_sysParam->fileHead.version > 1)//ʵ�ʰ汾Ҫ�ӵײ�ӿڻ��
				{
					/* ��Ϊ��ʼ������־ */
					g_sysParam->sendFlag = 1;
					DEBUG("Client send file type is: PISC\n");
				}

				if(g_sysParam->fileHead.type == TYPE_BRODCAST || g_sysParam->fileHead.version > 1)//ʵ�ʰ汾Ҫ�ӵײ�ӿڻ��
				{
					/* ��Ϊ��ʼ������־ */
					g_sysParam->sendFlag = 1;
				}

				break;
			
			case TYPE_MSG_CONTENT:
				DEBUG("TYPE_MSG_CONTENT recieve.\n");			
				/* ���յ��ļ�������Ϣ,ֻ������ͷ�ļ�����Ϣ */ 
				if(g_sysParam->sendFlag == 1)
				{
					/* ��ʼ���� */
		
					/* �жϽ��������Ƿ���� */
					
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


/* ���̵߳�ִ�г��� */
void* updateHandler(void *arg)
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
	ret = pthread_create(&pfd,NULL,(void *)updateHandler,NULL);
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

#if 0	
	if(fdIsReadable())
	{
		/* ���ձ��� */
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











