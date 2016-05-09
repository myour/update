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
#include "crc.h"


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
#define REVROOT 		"/update/"
#define REPLACEROOT 	"/update/old/"

/* �������� */
ST_FILE_PKG fileBuf[BUF_SIZE];
/* ���������±� */
int fb_idx = 0;	
/* ���ݽ��ս�����־ */
int revOver = 0;

/* ȫ���ϱ����� */
PST_MSG rpt_msg = NULL;
/* ���� */
PST_MSG revMsg = NULL; 


/* ����(����ŵ�ͷ�ļ���) */
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
	��������: 	����Ĭ�����в���
	���:		��
#####################################*/
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

/*####################################
	��������: 	��ʼ��UDP����
	���:		��
#####################################*/
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
	//serv_addr.sin_addr.s_addr	= inet_addr("192.168.16.181");// �󶨱�����ַ
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


/*####################################
	��������: 	����fd
	���:		��
#####################################*/
int isFdReadable(void)
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
		/* ���õȴ�ʱ��5s */
		tv.tv_sec	= 5;
		tv.tv_usec	= 0;
		retVal = select(sock_fd+1,&fds,NULL,NULL,&tv);
		switch(retVal)
		{
			case -1:
				DEBUG("receive err.\n");
				if(g_sysParam->revFlag == 1)
				{
					/* �ϱ����� */
					rptRevErr("receive data err");
					DEBUG("receive data error ,clean up received data!!!\n");
					/* ���־ */
					g_sysParam->revFlag = 0;
					/* ���֮ǰ���յ�buf������ */
					delRecvFile();

				}
				break;
				
			case 0:
				DEBUG("receive time out..\n");
				/* ��ӽ��ճ�ʱ�ж� */
				if(g_sysParam->revFlag == 1)
				{
					/* �ϱ����� */
					rptRevErr("receive data time out");
					DEBUG("receive data time out ,clean up received data!!!\n");
					/* ���־ */
					g_sysParam->revFlag = 0;
					/* ���֮ǰ���յ�buf������ */
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
	��������: 	�����봦������
	���:		int fd
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
		/* �жϽ������ݵ����� */
		DEBUG("revMsg->mType: %d\n",revMsg->mType);
		switch(revMsg->mType)
		{
			case TYPE_MSG_HEAD:
				printf("###### TYPE_MSG_HEAD recieve #####\n");
				if(g_sysParam->revFlag == 1)
				{
					/* �������ݰ��Ĺ����в�Ӧ�ý��յ����ݰ�ͷ */
					rptRevErr("get head pack while receiving error");
					DEBUG("receive head clash,clean up received data!!\n");
					g_sysParam->revFlag = 0;
					/* ���֮ǰ���յ�buf������ */
					delRecvFile();
					DEBUG("clean local buffer file.\n");
					break;
					
				}
			
				/* �����Ľ��հ�ͷ������ */	
				memcpy(&(g_sysParam->fileHead),(PST_FILE_HEADER)revMsg->buf,sizeof(ST_FILE_HEADER));	
#if 0
				DEBUG("******** get buf detail ********\n");		
				int i = 0;
				for(i=128;i<152;i++)
				{
					DEBUG("revMsg->buf[%d] = %d\n",i,revMsg->buf[i]);
				}
#endif				
				/* ��ӡ��ʾͷ�������� */
				DEBUG("******** get head detial ********\n");
				DEBUG("name: %s\n",g_sysParam->fileHead.name);
				DEBUG("size: %d\n",g_sysParam->fileHead.size);
				DEBUG("type: %d\n",(int)g_sysParam->fileHead.type);				
				DEBUG("ver:	 %c\n",g_sysParam->fileHead.version);
				DEBUG("crc:	 %lu\n",g_sysParam->fileHead.crc);
	
				/* �ж������ļ�����,�汾�ž����Ƿ��������(ʵ�ʰ汾Ҫ�ӵײ�ӿڻ��) */
				/* lcd ��ʾ������ */
				if((g_sysParam->fileHead.type == TYPE_LCD) && (g_sysParam->fileHead.version > 0))
				{
					DEBUG("Client send file type is: LCD\n");
					if(g_sysParam->revFlag == 0)
					{
						/* ������°汾�Ļ���λ��־ */					
						g_sysParam->revFlag	 = 1;
						/* ������λ����udp������Ϣ */
						g_sysParam->dst_addr = cli_addr;	
					}
					
				}	

				/* ������������� */	
				if((g_sysParam->fileHead.type == TYPE_PISC) && (g_sysParam->fileHead.version > 0))
				{
					DEBUG("Client send file type is: PISC\n");				
					if(g_sysParam->revFlag == 0)
					{
						g_sysParam->revFlag  = 1;
						g_sysParam->dst_addr = cli_addr;
					}
					
				}

				/* �㲥���ƺи��� */
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
					/* ȡĿ�Ķ�udp��ַ */
					g_sysParam->dst_addr = cli_addr;
					/* ��ʼ���� */					
					if(fb_idx < BUF_SIZE)
					{													
						DEBUG("writting date to buf\n");
						/* д�ڴ�(ÿ��д2+2+1024�ֽ�) */
						memcpy(&fileBuf[fb_idx],revMsg->buf,sizeof(ST_FILE_PKG));
						
						/* ��ֹ��λ��������� */
						if((fileBuf[fb_idx].curPkgNum > fileBuf[fb_idx].totalPkgNum)&& fb_idx>=0)
						{
							DEBUG("receive data error! where totalPkgNum > curPkgNum.\n");
							/* �ϱ����� */
							rptRevErr("receive data totalPkgNum > curPkgNum");
							/* ���־ */
							g_sysParam->revFlag = 0;
							/* ���֮ǰ���յ�buf������ */
							DEBUG("content pack error,clean up received data!!!\n");
							delRecvFile();							
							break;
						}

						/* �ϱ����� */
						char tmpbuf[32] = {0};
						sprintf(tmpbuf,"%d/%d",fileBuf[fb_idx].curPkgNum,fileBuf[fb_idx].totalPkgNum);
						rptRevProc(tmpbuf);
						
						/* ����д�� */
						if(fb_idx >= (BUF_SIZE-1) || (fileBuf[fb_idx].curPkgNum == fileBuf[fb_idx].totalPkgNum))
						{	
							if(fileBuf[fb_idx].curPkgNum == fileBuf[fb_idx].totalPkgNum)
							{
								revOver = 1;
							}
							
							/* ��1024���߷���д�ļ� */
							DEBUG("There is %d record in the ram.\n",fb_idx+1);
							DEBUG("Save buffer data into %s%s\n",REVROOT,g_sysParam->fileHead.name);
							if(writeLocalFile() == RET_FAIL)
							{
								DEBUG("Write %s%s fail,now try to write second time.\n",REVROOT,g_sysParam->fileHead.name);
								if(writeLocalFile() == RET_FAIL)
								{
									DEBUG("Write %s%s unsuccessful,please check out system!!!\n",REVROOT,g_sysParam->fileHead.name);
									/* �������flash�𻵻�����ԭ��������д��ʧ��,��������ݶ�ʧ */
									//break;
								}
							}
							/* ���ݴ��� */
							if(fileBuf[fb_idx].curPkgNum == fileBuf[fb_idx].totalPkgNum)
							{
								/* �����ļ�������� */
								DEBUG("File transmit over.\n");
								g_sysParam->revFlag = 0;
								/* У������ļ�����ȷ�� */
								DEBUG("Try to check file.\n");
								int retCode = checkRevFile();
								if(RET_OK == retCode)
								{
									DEBUG("replace file done..\n");
								
									/* 1.�滻���ļ� */
									if(replaceOldFile(REPLACEROOT) == RET_FAIL)
									{
										DEBUG("Can't replace old file.\n");
										/* ���� */
										if(replaceOldFile(REPLACEROOT) == RET_FAIL)
											rptRevErr("Can't replace old file");
									}
									else
									{
										/* 2.����ϵͳ */
										//rebootSystem();
										/* ��ѹ������,ִ�������ű� */
										
									}
								
								}
								else
								{
									/* 1.�����ļ�����ȷ�ϱ����� */
									rptRevErr("CheckSum received file error");
									/* 2.ɾ���ļ� */
									DEBUG("Delete received data.\n");									
								#if 0	
									// ����ʹ��
									/* 1.�滻���ļ� */
									if(replaceOldFile(REPLACEROOT) == RET_FAIL)
									{
										DEBUG("Can't replace old file.\n");
										/* ���� */
										if(replaceOldFile(REPLACEROOT) == RET_FAIL)
											rptRevErr("Can't replace old file");
									}
								#endif
									/* ɾ�� */
									delRecvFile();
								}	
							}
							/* ���buf */
							memset(fileBuf,0,sizeof(fileBuf));
							fb_idx = 0;
						}
						else
						{
							/* �ڴ������±��ƶ� */
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
	��������: 	���̵߳�ִ�г���
	���:		void *arg
######################################*/
void* threadHandler(void *arg)
{
	/* ���ݽ������� */
	isFdReadable();
	return ((void *)0);
}


/*####################################
	��������: 	��ʼ�������߳�
	���:		��
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
	��������: 	���malloc/
				�ر�ȫ��fd����Դ 
	���:		char *errStr
######################################*/
void closeResource(void)
{
	/* ����free clear */
	free(g_sysParam);
	//close();
}


/*####################################
	��������: 	�ϱ����մ���
	���:		char *errStr
######################################*/
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

	//free(rpt_msg);
	return retval;
}


/*####################################
	��������: 	�ϱ����ս���
	���:		char *procStr
######################################*/
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

	//free(rpt_msg);

	return retval;
}


/*####################################
 ��������: 	�����յ�������д���ļ�
 ���:		��
######################################*/
int writeLocalFile(void)
{
	int file_fd;
	int cnt = 0;	
	int w_num = 0;
	int retval = RET_OK;
	
	/* 1.�ļ����������,�����ļ�����׷�� */
	char fileName[128] = {0};
	sprintf(fileName,"%s%s",REVROOT,g_sysParam->fileHead.name);
	file_fd = open(fileName,O_CREAT|O_WRONLY|O_APPEND,0666);
	if(file_fd < 0)
	{
		DEBUG("Open %s fail.\n",fileName);
		return RET_FAIL;
	}
	/* 2.д�ļ� */
	for(cnt=0; cnt<= fb_idx; cnt++)
	{				
		if((fb_idx == cnt) && (revOver ==1))// ��Ҫ�ж��Ƿ񵽴��ļ�ĩβ
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

		/* �����ú���ȥ�� */
		DEBUG("Write bytes: %d\n",w_num);
		
	}
	/* 3.ͬ�� */
	sync();
	close(file_fd);
	return retval;

}


/*####################################
	��������: 	У����յ��ļ�
	���:		��
######################################*/
int checkRevFile(void)
{	
	unsigned long crcResult = 0;
	unsigned int fileSize = 0;
	int retval = RET_OK;
	int fd = 0;
	

	/* 1.�ļ����У�� */
	/* 1.���ļ���ȡ��С */
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
	
	/* 2.crc32У�� */
	char *checkBuf = (char *)malloc(fileSize+2);// ���������
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
	��������: 	�滻ԭ���ļ�
	���:		��
######################################*/
int replaceOldFile(char *filePath)
{	
	int retval = RET_OK;
	char cmd[128] = {0};

	switch(g_sysParam->fileHead.type)
	{
		case TYPE_LCD:
			/* 1.ɾ��ԭ���ļ� */
			sprintf(cmd,"rm -rf %s%s",filePath,"lcd");// "lcd"ָ�ļ����水��ʵ����������޸�
			if(system(cmd) == -1)
				retval = RET_FAIL;
				
			/* 2.�������ļ�������Ŀ¼ */
			sprintf(cmd,"mv %s%s %s%s",REVROOT,g_sysParam->fileHead.name,filePath,"lcd");
			if(system(cmd) == -1)
				retval = RET_FAIL;

			/* 3.��������Ȩ�� */
			sprintf(cmd,"chmod +x %s%s",filePath,"lcd");
			if(system(cmd) == -1)
				retval = RET_FAIL;	
				
			break;
			
		case TYPE_PISC:
			/* 1.ɾ��ԭ���ļ� */
			sprintf(cmd,"rm -rf %s%s",filePath,"pisc");
			if(system(cmd) == -1)
				retval = RET_FAIL;
				
			/* 2.�������ļ�������Ŀ¼ */
			sprintf(cmd,"mv %s%s %s%s",REVROOT,g_sysParam->fileHead.name,filePath,"pisc");
			if(system(cmd) == -1)
				retval = RET_FAIL;

			/* 3.��������Ȩ�� */
			sprintf(cmd,"chmod +x %s%s",filePath,"pisc");
			if(system(cmd) == -1)
				retval = RET_FAIL;	

			break;			

		case TYPE_BRODCAST:
			/* 1.ɾ��ԭ���ļ� */
			sprintf(cmd,"rm -rf %s%s",filePath,"brodcast");
			if(system(cmd) == -1)
				retval = RET_FAIL;
				
			/* 2.�������ļ�������Ŀ¼ */
			sprintf(cmd,"mv %s%s %s%s",REVROOT,g_sysParam->fileHead.name,filePath,"brodcast");
			if(system(cmd) == -1)
				retval = RET_FAIL;

			/* 3.��������Ȩ�� */
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
	��������: 	ϵͳ����
	���:		��
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
	��������: 	ɾ������ʱ���м��ļ�
	���:		��
######################################*/
int delRecvFile(void)
{	
	int retval = RET_FAIL;

	/* ����ѽ��յ�buf���ļ� */
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
	��������: 	������(���ܲ���ʹ��)
	���:		int argc,
				char *argv[]
##########################################*/
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






