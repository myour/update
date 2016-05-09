/* ####################################################
				���������ܲ����ļ�
	���ܣ�����update.c�Ĺ���
	Author: chenrong 2016.03.30
#################################################### */
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
#define DEBUG_CLIENT

#ifdef 	DEBUG_CLIENT
#define CLI_PRINT(format, ...) printf("FILE: "__FILE__", LINE: %d: "format, __LINE__, ##__VA_ARGS__)
#else
#define CLI_PRINT(format, ...)
#endif

#define SRC_PORT	(8888)

/* ���ݰ� */
PST_MSG msg = NULL;


/* ȫ�� */
int src_fd,dst_fd;
struct sockaddr_in src_addr,dst_addr,rev_addr;
int addr_sz = sizeof(struct sockaddr);


/* ������� */
PST_MSG makePack(EN_MSG_TYPE type,void *buf)
{
	//int ret = RET_OK;
	/* ����update.h�ж��幹�����ݰ� */
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
			/* ȷ���������� */
			memcpy((PST_FILE_HEADER)msg->buf,(PST_FILE_HEADER)buf,sizeof(ST_FILE_HEADER));		
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


void* revHandler(void *arg)
{
	int len = 0;
	PST_MSG revMsg = (PST_MSG)malloc(sizeof(ST_MSG));
	if(revMsg == NULL)
	{
		CLI_PRINT("memery malloc err..\n");
		return (void *)-1;		
	}
	
	while(1)
	{
		memset(revMsg,0,sizeof(ST_MSG));
		len = recvfrom(src_fd,revMsg,sizeof(ST_MSG),0,(struct sockaddr *)&rev_addr,&addr_sz);
		if(len < 0)
		{
			CLI_PRINT("recvfrom err..\n");
			return (void *)-1;
		}
		/* ��ӡ���յ����� */
		switch(revMsg->mType)
		{
			case TYPE_MSG_RPT_ERR:
				CLI_PRINT("recieve TYPE_MSG_RPT_ERR msg.\n");
				CLI_PRINT("****** %s ******\n",revMsg->buf);
				break;

			case TYPE_MSG_RPT_PROC:
				CLI_PRINT("rev TYPE_MSG_RPT_PROC msg.\n");
				break;

			default:
				CLI_PRINT("Recieve type error.\n");
				break;
		}
	}

	return (void *)0;
}

int initRevThread(void)
{

	int result,retval = RET_OK;
	pthread_t pid;
	
	/* �̳߳�ʼ�� */
	printf("#############  rev thread init  ##################\n");
	result = pthread_create(&pid,NULL,(void *)revHandler,NULL);
	if(result < 0)
	{
		CLI_PRINT("receive thread create fail.\n");
		retval = RET_FAIL;
	}
	CLI_PRINT("receive thread create success.\n");
	return retval;
}



/* ������ */
int main(int argc,char *argv[])
{
	//int src_fd,dst_fd;
	//struct sockaddr_in src_addr,dst_addr;
	//int addr_sz	= sizeof(struct sockaddr);	
	int retval = RET_FAIL;

	
	/* 1.��ʼ��udp */
	printf("######### Client socket initial #########\n");
	src_fd = socket(AF_INET,SOCK_DGRAM,0);
	if(src_fd < 0)
	{
		CLI_PRINT("Client socket create fail..\n");
		return RET_FAIL;
	}
	CLI_PRINT("Client udp sock create success.\n");

	/* 2.�� */
	printf("######### socket bind #########\n");
	src_addr.sin_family			= AF_INET;
	src_addr.sin_addr.s_addr 	= htonl(INADDR_ANY);
	src_addr.sin_port			= htons(SRC_PORT);
	retval = bind(src_fd,(struct sockaddr *)&src_addr,addr_sz);
	if(retval < 0)
	{
		CLI_PRINT("Client bind fail.\n");
		return RET_FAIL;
	}
	CLI_PRINT("Client sock bind success.\n");


	/* 3.��� */
	printf("######### making package #########\n");	
	PST_FILE_HEADER	head = (PST_FILE_HEADER)malloc(sizeof(ST_FILE_HEADER));
	if(head == NULL)
	{
		CLI_PRINT("Malloc error.\n");
		close(src_fd);
		return RET_FAIL;
	}
	memset(head,0,sizeof(ST_FILE_HEADER));
	CLI_PRINT("Malloc done.\n");
	strncpy(head->name,"pisc_v1.0",strlen("pisc_v1.0"));
	head->type 		= TYPE_PISC;	// �ļ�����,��������Ϣ����
	head->size 		= 1024;			// ���������
	head->version 	= 1;			// ���������
	head->crc 		= 555;			// ���������
	CLI_PRINT("memcpy done.\n");	
	PST_MSG message = makePack(TYPE_MSG_HEAD,head);
	CLI_PRINT("Make package done..\n");
	
	/* 4.���Ͳ������� */
	printf("######### Ready to send data #########\n");
	dst_addr.sin_family 		= AF_INET;
	//dst_addr.sin_addr.s_addr	= inet_addr("192.168.16.67");
	dst_addr.sin_addr.s_addr	= inet_addr("192.168.16.181");
	dst_addr.sin_port			= htons(UDP_PORT);

	/* �����߳� */
	initRevThread();
	while(1)
	{
		retval = sendto(src_fd,message,sizeof(ST_MSG),0,(struct sockaddr *)&dst_addr,addr_sz);
		if(retval < 0)
		{
			CLI_PRINT("Send data error.\n");
		}
		
		CLI_PRINT("Client send %d bytes data.\n",retval);
		sleep(2);

	}

	free(head);
	free(msg);
	return RET_OK;
}



