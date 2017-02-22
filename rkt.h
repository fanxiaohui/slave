#ifndef _RKT_H_
#define _RKT_H_
#include "../lib/common.h"

#define DEV_RKT "/dev/ttyO3"

#define MSG_RKT_TYPE_A2R 0x0e //A:�Լ���(AP); R(Rocket):��� A->R�Ի���
#define MSG_RKT_TYPE_R2A 0x0e //R->A�Ի���
#define MSG_RKT_TYPE_ATT 0x1c //��̬��
#define MSG_RKT_TYPE_DAT 0x22 //̽�����ݰ�

typedef struct tagRktAct{
	unsigned char id;
	unsigned char func;
	unsigned char data;
	unsigned char sum;
}T_RKT_R2A;//���(rocket)-->�Լ���(AP)����

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
}T_RKT_R2A_DATA;//���̽������

typedef struct tagRktAttitude{
	int lon;			//[10-6 degree] ����
	int lat;			//[10-6 degree] ά��
	short alt;		//[0.1 m] �߶�
	short pitch;	//[0.01 degree] ����
	short roll;		//[0.01 degree] ��ת
	short yaw;		//[0.01 degree] ƫ��
	unsigned short check;
}T_RKT_A2R_ATT;//��̬����

typedef struct tagRktRec
{
	T_DATETIME _datatime;//������¼ʱ��
	T_RKT_R2A_DATA data;
}T_RKT_REC;

extern T_RKT_R2A rkt_r2a;
extern T_RKT_R2A_DATA rkt_data;
extern T_RKT_A2R_ATT rkt_att;
extern T_RKT_REC rkt_rec;

int rkt_init(void);
int rkt_send(unsigned char type, unsigned short len, unsigned char *buf);

#endif//_RKT_H_
