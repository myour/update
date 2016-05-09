/* ####################################################
				��������ͷ�ļ�
	���ܣ���Ҫ��������һЩ�������н��õ���
	�������ͽṹ��,�궨��
	Author: chenrong 2016.03.25
#################################################### */

#ifndef __UPDATE_H__
#define __UPDATE_H__

/* ���Ͷ��� */
typedef unsigned char uint8;
typedef unsigned int  uint16;
typedef unsigned long uint32;

/* һЩ���õı��� */
#define RET_OK			(0)
#define RET_FAIL		(-1)
/* �ļ����� */
#define FILE_NAME_LEN	(256)
#define SEND_BUF_LEN	(1024)
/* UDP */
#define UDP_PORT		(9527)


/* UDP��Ϣ���� */
typedef enum __MSG_TYPE__ {
	TYPE_MSG_HEAD 		= 0x01,// �����ļ�ͷ��Ϣ
	TYPE_MSG_CONTENT	= 0x02,// �����ļ�������Ϣ
	TYPE_MSG_RPT_PROC	= 0x03,// �����ϱ�������Ϣ
}EN_MSG_TYPE,*PEN_MSG_TYPE;


/* ��Ϣ�ṹ�� */
typedef struct __MSG__
{
	EN_MSG_TYPE	mType;
	uint8 buf[2048];
}ST_MSG,*PST_MSG;


/* �����ļ������� */
typedef enum __FILE_TYPE__ {
	TYPE_LCD = 0,
	TYPE_PISC,
	TYPE_BRODCAST,
}EN_FILE_TYPE,*PEN_FILE_TYPE;


/* ���ݴ���ͷ�ṹ�� */
typedef struct __FILE_HEADER__
{
	uint8	name[FILE_NAME_LEN];// ��Ҫ���µ��ļ���
	uint8	version;			// ��ǰ�����ļ��İ汾��
	uint16	size;				// �ļ��Ĵ�С
//	uint16	totalpkgNum;		// ��Ҫ���͵��ܰ���
	EN_FILE_TYPE type;			// ��ǰ���͵��ļ�����
	uint16 crc;					// crcУ����
}ST_FILE_HEADER,*PST_FILE_HEADER;


/* ���ݴ��䱨�Ľṹ�� */
typedef struct __FILE_PKG__
{
	uint16	totalPkgNum;			// �ļ����ܰ���
	uint16	curPkgNum;				// ��ǰ�Ѿ����͵İ���
	uint8	content[SEND_BUF_LEN];	// ��Ҫ���µ��ļ���
}ST_FILE_PKG,*PST_FILE_PKG;


/* �ϱ����Ƚṹ�� */
typedef struct __RPT_PKG__
{
	EN_FILE_TYPE fType;	// ��ǰ���ϴ����ļ�����
	uint8 status;		// ��ǰ���ϴ���״̬(0����/1�쳣)
	uint8 percent;		// ��ǰ�ļ����ϴ����� eg:20��ʾ20%
	
}ST_RPT_PKG,*PST_RPT_PKG;


/* ȫ�����в��� */
typedef struct __SYS_PARAM__
{
	uint8	sendFlag;	// �����Ϳ�ʼ��־
	uint8 	revPkgNum;	// ��ǰ���յİ���
	uint8	curVer[8];	// ��ǰ�Ѿ����͵İ���
	uint16	udp_fd;		// �ļ����ܰ���	
	ST_FILE_HEADER fileHead;	
}ST_SYS_PARAM,*PST_SYS_PARAM;



#endif


