#ifndef _BD_H_
#define _BD_H_
#include "../lib/common.h"

#define DEV_BD "/dev/ttyO4"
#define BD_BAUDRATE 115200

#define BD_REC_DATA_RECV 1 /*��¼�ļ��б���ı��������ǽ�������*/
#define BD_REC_DATA_SEND 2/*��¼�ļ��б���ı��������Ƿ�������*/

#define _BD_ID_AP  0x032ec2 /* 208578 �Լ���ID��ģ��*/
#define _BD_ID_GCS 0x032ec4 /* 208580 ����վID��һ���*/
#define MSG_LEN_MAX 80 /*[byte]�����ĳ���*/

#define MSGID_AP_REAL 0xa1 //�Լ���ʵʱ���ݰ�
#define MSGID_GCS_CMD 0x51 //����վ�����������ݰ�
#define MSGID_GCS_SP 0x52 //����վ�������ݰ�


#define CMD_GLJC 1 //���ʼ��
#define CMD_DWSQ 2 //��λ����
#define CMD_TXSQ 3 //ͨ������
#define CMD_DSSQ 4 //��ʱ����
#define CMD_ICJC 5 //IC���

#define STR_GLJC "$GLJC" //���ʼ��
#define STR_DWSQ "$DWSQ" //��λ����
#define STR_TXSQ "$TXSQ" //ͨ������
#define STR_DSSQ "$DSSQ" //��ʱ����
#define STR_ICJC "$ICJC" //IC���

//#define STR_GLZK "$GLJC" //����״��
#define STR_GLZK "$GLZK" //����״��
#define STR_DWXX "$DWXX" //��λ��Ϣ
#define STR_TXXX "$TXXX" //ͨ����Ϣ
#define STR_DSJG "$DSJG" //��ʱ���
#define STR_ICXX "$ICXX" //IC��Ϣ
#define STR_FKXX "$FKXX" //������Ϣ

//���յ���һ�������Ϣ
#define STR_GNGS "$GNGS" //
#define STR_GPGS "$GPGS" //
#define STR_GNRM "$GNRM" //
#define STR_GNGG "$GNGG" //
#define STR_GNGL "$GNGL" //

typedef struct tagBDGCS2AP{
	unsigned char msgid;
	unsigned char cnt;
	unsigned char data[78];
}T_BD_GCS2AP;//����վ(GCS)-->�Լ���(AP)����

typedef struct tagBDAP2GCS{
	unsigned char msgid;
	unsigned char cnt;
	unsigned char data[78];
}T_BD_AP2GCS;//�Լ���(AP)-->����վ(GCS)����

//�����շ����ݼ�¼��ÿ��ͨ���������͵����ݺͽ��յ����ݶ����������¼����ұ��ʱ��
//�����շ���Ϊ���80���ֽڣ����ԣ��ڱ�������ʱ�����ͻ�������ݶ�������ͬһ���ֽ���data�
//��ͨ����������type(=BD_REC_DATA_SEND or BD_REC_DATA_RECV)��ȷ���Ƿ��͵����ݻ��ǽ��յ����ݡ�
//BD_REC_DATA_AP2GCS
typedef struct tagBDRec
{
	T_DATETIME _datatime;//������¼ʱ��
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
