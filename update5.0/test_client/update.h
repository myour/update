/* ####################################################
				升级程序头文件
	功能：主要用来定义一些，过程中将用到的
	数据类型结构体,宏定义
	Author: chenrong 2016.03.25
#################################################### */

#ifndef __UPDATE_H__
#define __UPDATE_H__

/* 类型定义 */
typedef unsigned char uint8;
typedef unsigned int  uint16;
typedef unsigned long uint32;
typedef struct sockaddr_in st_sockaddr_in;


/* 一些常用的变量 */
#define RET_OK			(0)
#define RET_FAIL		(-1)
/* 文件发送 */
#define FILE_NAME_LEN	(128)
#define SEND_BUF_LEN	(1024)
/* UDP */
#define UDP_PORT		(9527)


/* UDP消息类型 */
typedef enum __MSG_TYPE__ {
	TYPE_MSG_HEAD 		= 0x01,// 发送文件头消息
	TYPE_MSG_CONTENT	= 0x02,// 发送文件内容消息
	TYPE_MSG_RPT_PROC	= 0x03,// 发送上报进度消息
	TYPE_MSG_RPT_ERR	= 0x04,// 发送上报进度消息	
}EN_MSG_TYPE,*PEN_MSG_TYPE;


/* 消息结构体 */
typedef struct __MSG__
{
	EN_MSG_TYPE	mType;
	uint8 buf[2048];
}ST_MSG,*PST_MSG;


/* 更新文件的类型 */
typedef enum __FILE_TYPE__ {
	TYPE_LCD = 0,
	TYPE_PISC,
	TYPE_BRODCAST,
}EN_FILE_TYPE,*PEN_FILE_TYPE;


/* 数据传输报文结构体 */
typedef struct __FILE_PKG__
{
	uint16	totalPkgNum;			// 文件的总包数
	uint16	curPkgNum;				// 当前已经发送的包数
	uint8	content[SEND_BUF_LEN];	// 需要更新的文件名
}ST_FILE_PKG,*PST_FILE_PKG;


/* 数据传输头结构体 */
typedef struct __FILE_HEADER__
{
	uint8	name[FILE_NAME_LEN];// 需要更新的文件名
	uint8	version;			// 当前发送文件的版本号
	uint16	size;				// 文件的大小
//	uint16	totalpkgNum;		// 将要发送的总包数
	EN_FILE_TYPE type;			// 当前发送的文件类型
	uint16 crc;					// crc校验码
}ST_FILE_HEADER,*PST_FILE_HEADER;


/* 上报进度结构体 */
typedef struct __RPT_PKG__
{
	EN_FILE_TYPE fType;	// 当前正上传的文件类型
	uint8 status;		// 当前的上传的状态(0正常/1异常)
	uint8 percent;		// 当前文件的上传进度 eg:20表示20%
	
}ST_RPT_PKG,*PST_RPT_PKG;


/* 全局运行参数 */
typedef struct __SYS_PARAM__
{
	uint8	revFlag;		// 包发送开始标志
	uint8 	revPkgNum;		// 当前接收的包数
	uint8	curVer[8];		// 当前已经发送的包数
	uint16	udp_fd;			// 文件的总包数	
	st_sockaddr_in dst_addr;
	ST_FILE_HEADER fileHead;	
}ST_SYS_PARAM,*PST_SYS_PARAM;

/* 文件接收状态机 */
typedef enum __STATUS__ 
{
	revidle	= 0x00,	// 还没接收到数据报
	revHead	= 0x01, // 接收到文件头包,等待接收二进制文件包
	revFile	= 0x02,	// 正在接收二进制文件包
	revDone	= 0x03,	// 文件包接收完毕
	revErr	= 0x04,	// 数据包在传输过程中出错
}EN_REV_STAT,*PEN_REV_STAT;



#endif


