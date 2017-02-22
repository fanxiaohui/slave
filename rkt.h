#ifndef _RKT_H_
#define _RKT_H_
#include "../lib/common.h"

#define DEV_RKT "/dev/ttyO3"

#define MSG_RKT_TYPE_A2R 0x0e //A:自驾仪(AP); R(Rocket):火箭 A->R对话包
#define MSG_RKT_TYPE_R2A 0x0e //R->A对话包
#define MSG_RKT_TYPE_ATT 0x1c //姿态包
#define MSG_RKT_TYPE_DAT 0x22 //探空数据包

typedef struct tagRktAct{
	unsigned char id;
	unsigned char func;
	unsigned char data;
	unsigned char sum;
}T_RKT_R2A;//火箭(rocket)-->自驾仪(AP)数据

typedef struct tagRktData{
	short temp;
	unsigned short humi;
	unsigned short airpress;
	unsigned short windspd;
	unsigned short winddir;
	int lat;
	int lon;
	int alt;
	unsigned char id;
	unsigned char checksum;
}T_RKT_R2A_DATA;//火箭探空数据

typedef struct tagRktAttitude{
	int lon;			//[10-6 degree] 经度
	int lat;			//[10-6 degree] 维度
	short alt;		//[0.1 m] 高度
	short pitch;	//[0.01 degree] 俯仰
	short roll;		//[0.01 degree] 滚转
	short yaw;		//[0.01 degree] 偏航
	unsigned short check;
}T_RKT_A2R_ATT;//姿态数据

typedef struct tagRktRec
{
	T_DATETIME _datatime;//本条记录时间
	T_RKT_R2A_DATA data;
}T_RKT_REC;

extern T_RKT_R2A rkt_r2a;
extern T_RKT_R2A_DATA rkt_data;
extern T_RKT_A2R_ATT rkt_att;
extern T_RKT_REC rkt_rec;

int rkt_init(void);
int rkt_send(unsigned char type, unsigned short len, unsigned char *buf);

#endif//_RKT_H_
