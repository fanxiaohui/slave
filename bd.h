#ifndef _BD_H_
#define _BD_H_
#include "../lib/common.h"

#define DEV_BD "/dev/ttyO4"
#define BD_BAUDRATE 115200

#define BD_REC_DATA_RECV 1 /*记录文件中保存的本条数据是接收数据*/
#define BD_REC_DATA_SEND 2/*记录文件中保存的本条数据是发送数据*/

#define _BD_ID_AP  0x032ec2 /* 208578 自驾仪ID，模块*/
#define _BD_ID_GCS 0x032ec4 /* 208580 地面站ID，一体机*/
#define MSG_LEN_MAX 80 /*[byte]最大电文长度*/

#define MSGID_AP_REAL 0xa1 //自驾仪实时数据包
#define MSGID_GCS_CMD 0x51 //地面站控制命令数据包
#define MSGID_GCS_SP 0x52 //地面站航点数据包


#define CMD_GLJC 1 //功率检测
#define CMD_DWSQ 2 //定位申请
#define CMD_TXSQ 3 //通信申请
#define CMD_DSSQ 4 //定时申请
#define CMD_ICJC 5 //IC检测

#define STR_GLJC "$GLJC" //功率检测
#define STR_DWSQ "$DWSQ" //定位申请
#define STR_TXSQ "$TXSQ" //通信申请
#define STR_DSSQ "$DSSQ" //定时申请
#define STR_ICJC "$ICJC" //IC检测

//#define STR_GLZK "$GLJC" //功率状况
#define STR_GLZK "$GLZK" //功率状况
#define STR_DWXX "$DWXX" //定位信息
#define STR_TXXX "$TXXX" //通信信息
#define STR_DSJG "$DSJG" //定时结果
#define STR_ICXX "$ICXX" //IC信息
#define STR_FKXX "$FKXX" //反馈信息

//接收到的一体机的信息
#define STR_GNGS "$GNGS" //
#define STR_GPGS "$GPGS" //
#define STR_GNRM "$GNRM" //
#define STR_GNGG "$GNGG" //
#define STR_GNGL "$GNGL" //

typedef struct tagBDGCS2AP{
	unsigned char msgid;
	unsigned char cnt;
	unsigned char data[78];
}T_BD_GCS2AP;//地面站(GCS)-->自驾仪(AP)数据

typedef struct tagBDAP2GCS{
	unsigned char msgid;
	unsigned char cnt;
	unsigned char data[78];
}T_BD_AP2GCS;//自驾仪(AP)-->地面站(GCS)数据

//北斗收发数据记录，每条通过北斗发送的数据和接收的数据都放在这个记录里，并且标出时间
//由于收发均为最多80个字节，所以，在保存数据时将发送或接收数据都保存在同一个字节流data里，
//并通过数据类型type(=BD_REC_DATA_SEND or BD_REC_DATA_RECV)来确定是发送的数据还是接收的数据。
//BD_REC_DATA_AP2GCS
typedef struct tagBDRec
{
	T_DATETIME _datatime;//本条记录时间
	unsigned short type;
	unsigned short len;
	char data[80];
}T_BD_REC;

extern T_BD_AP2GCS bd_ap2gcs;
extern T_BD_GCS2AP bd_gcs2ap;
extern T_BD_REC bd_rec;
extern int bd_send_data_ena;

int bd_init(void);
int bd_send_cmd(unsigned char cmd_id);
int bd_send_data(unsigned short len, unsigned char *buf);
void bd_recv_process(unsigned short len, unsigned char *buf);

#endif//_BD_H_
