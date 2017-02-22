#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <linux/types.h>
#include <string.h>
#include <poll.h>
#include <termios.h>
#include <linux/ioctl.h>

#include "rkt.h"
#include "log.h"
#include "global.h"

#define SM_RKT_RECV_HEAD1 0
#define SM_RKT_RECV_HEAD2 1
#define SM_RKT_RECV_HEAD3 2
#define SM_RKT_RECV_HEAD4 3
#define SM_RKT_RECV_TYPE 4 //
#define SM_RKT_RECV_ZERO 5
#define SM_RKT_RECV_DATA 6
#define SM_RKT_RECV_TAIL1 7
#define SM_RKT_RECV_TAIL2 8
#define SM_RKT_RECV_TAIL3 9
#define SM_RKT_RECV_TAIL4 10

#define MSG_RKT_HEAD1 0xeb
#define MSG_RKT_HEAD2 0xeb
#define MSG_RKT_HEAD3 0xeb
#define MSG_RKT_HEAD4 0xeb
#define MSG_RKT_TAIL1 0xee
#define MSG_RKT_TAIL2 0xee
#define MSG_RKT_TAIL3 0xee
#define MSG_RKT_TAIL4 0xee

int fd_rkt;
pthread_t rkt_recv_thread_id;
FILE * fp_rkt = NULL;
int rkt_file_opened = 0;

T_RKT_R2A rkt_r2a;
T_RKT_R2A_DATA rkt_data;
T_RKT_A2R_ATT rkt_att;
T_RKT_REC rkt_rec;
unsigned char rkt_state;

//���ͻ�������ص����ݳ��ȡ����ͺͻ����������ڼ�¼���͵����ݰ���
//�Ӷ��������յ�����ʧ��Ҫ���ط�����ʱ�ظ������ѷ��͵����ݰ�
int rkt_send_len;
unsigned char rkt_send_type;
unsigned char rkt_send_data[200];

void rkt_recv_thread(void);
void rkt_r2a_process();
void rkt_data_process();

int rkt_init(void)
{
	char file_name[100];

	//��׷�ӷ�ʽ�����ݼ�¼�ļ�
//	sprintf(file_name, "RKT%4d%2d%2d-%2d%2d%2d.dat", gbl_time_val->tm_year + 1900, gbl_time_val->tm_mon + 1,
//		gbl_time_val->tm_mday, gbl_time_val->tm_hour, gbl_time_val->tm_min, gbl_time_val->tm_sec);
	sprintf(file_name, "RKT%02d%02d_%02d%02d%02d.dat", datetime_now.month, datetime_now.day,
		datetime_now.hour, datetime_now.minute, datetime_now.second);

	rkt_file_opened = 1;
	fp_rkt = fopen(file_name, "wb+");
	if (NULL == fp_rkt)
	{
		sprintf(logmsg, "Open RKT rec file failed: %d", fp_rkt);
		write_log(logmsg);
		rkt_file_opened = 0;//û��������
	}

	//��ʼ������վ����
	fd_rkt = open(DEV_RKT, O_RDWR);
	if (fd_rkt < 0)
	{
		printf("Open DEV_RKT failed: %d\n", fd_rkt);
		return -1;
	}
	set_speed(fd_rkt, 9600); //set serial port baud
	set_parameter(fd_rkt, 8, 1, 'N'); //8 data bits, 1 stop bit, no parity
	pthread_create(&rkt_recv_thread_id, NULL, (void*)rkt_recv_thread, NULL);//RS232 receive thread
}

int rkt_send(unsigned char type, unsigned short len, unsigned char *buf)
{
	unsigned char sbuf[200];
	rkt_send_len = len;
	rkt_send_type = type;
	memcpy(rkt_send_data, buf, len);
	sbuf[0] = MSG_RKT_HEAD1;
	sbuf[1] = MSG_RKT_HEAD2;
	sbuf[2] = MSG_RKT_HEAD3;
	sbuf[3] = MSG_RKT_HEAD4;
	sbuf[4] = type;
	sbuf[5] = 0x00;
	memcpy(&sbuf[6],buf,len);
	sbuf[10] = MSG_RKT_TAIL1;
	sbuf[11] = MSG_RKT_TAIL2;
	sbuf[12] = MSG_RKT_TAIL3;
	sbuf[13] = MSG_RKT_TAIL4;
	write(fd_rkt, sbuf, len+10);

	return 0;
}

void rkt_recv_thread(void)
{	
	int nread=0;
	int i = 0;
	unsigned char buf[500];
	int rkt_recv_sm;//����״̬��
	int rkt_data_cnt = 0;
	int rkt_data_len = 0;
	unsigned char rkt_data_buf[72];

	rkt_recv_sm = SM_RKT_RECV_HEAD1;
	rkt_data_cnt = 0;
	while(1)    
	{
  		nread = read(fd_rkt,buf,500);

		for (i = 0; i < nread; i++)
		{
			switch (rkt_recv_sm)
			{
			case SM_RKT_RECV_HEAD1:
				if (buf[i] == MSG_RKT_HEAD1)
				{
					rkt_recv_sm = SM_RKT_RECV_HEAD2;
				}
				break;
			case SM_RKT_RECV_HEAD2:
				if (buf[i] == MSG_RKT_HEAD2)
				{
					rkt_recv_sm = SM_RKT_RECV_HEAD3;
				}
				break;
			case SM_RKT_RECV_HEAD3:
				if (buf[i] == MSG_RKT_HEAD3)
				{
					rkt_recv_sm = SM_RKT_RECV_HEAD4;
				}
				break;
			case SM_RKT_RECV_HEAD4:
				if (buf[i] == MSG_RKT_HEAD4)
				{
					rkt_recv_sm = SM_RKT_RECV_TYPE;
				}
				break;
			case SM_RKT_RECV_TYPE:
				if (buf[i] == MSG_RKT_TYPE_R2A)
				{
					rkt_data_len = 4;
					rkt_recv_sm = SM_RKT_RECV_ZERO;
				}
				else if (buf[i] == MSG_RKT_TYPE_DAT)
				{
					rkt_data_len = 24;
					rkt_recv_sm = SM_RKT_RECV_ZERO;
				}
				else
				{
					rkt_recv_sm = SM_RKT_RECV_HEAD1;
				}
				break;
			case SM_RKT_RECV_ZERO:
				if (buf[i] == 0x0)
				{
					rkt_data_cnt = 0;
					rkt_recv_sm = SM_RKT_RECV_DATA;
				}
				else
				{
					rkt_recv_sm = SM_RKT_RECV_HEAD1;
				}
				break;
			case SM_RKT_RECV_DATA:
				rkt_data_buf[rkt_data_cnt] = buf[i];
				rkt_data_cnt++;
				if (rkt_data_cnt >= rkt_data_len) rkt_recv_sm = SM_RKT_RECV_TAIL1;
				break;
			case SM_RKT_RECV_TAIL1:
				if (buf[i] == MSG_RKT_TAIL1)
				{
					rkt_recv_sm = SM_RKT_RECV_TAIL2;
				}
				break;
			case SM_RKT_RECV_TAIL2:
				if (buf[i] == MSG_RKT_TAIL2)
				{
					rkt_recv_sm = SM_RKT_RECV_TAIL3;
				}
				break;
			case SM_RKT_RECV_TAIL3:
				if (buf[i] == MSG_RKT_TAIL3)
				{
					rkt_recv_sm = SM_RKT_RECV_TAIL4;
				}
				break;
			case SM_RKT_RECV_TAIL4:
				if (buf[i] == MSG_RKT_TAIL4)
				{
					if (rkt_data_len == sizeof(rkt_r2a))//���������Ӧ���
					{
						memcpy(&rkt_r2a, rkt_data_buf, sizeof(rkt_r2a));
						rkt_r2a_process();//������Լ��������Ӧ�����ݴ���
					}
					else if (rkt_data_len == sizeof(rkt_data))//��̽�����ݰ�
					{
						memcpy(&rkt_data, rkt_data_buf, sizeof(rkt_data));
					}
					rkt_data_process();
					rkt_recv_sm = SM_RKT_RECV_HEAD1;
				}
				break;
			default:;
				rkt_recv_sm = SM_RKT_RECV_HEAD1;
			}
		}
	}
	return;
}

//���->�Լ��ǵ����ݴ���
void rkt_r2a_process()
{
	unsigned char buf[200];
	if (rkt_r2a.sum == (rkt_r2a.id + rkt_r2a.data + rkt_r2a.func))
	{
		if (rkt_r2a.id == 0x01)
		{
			s2m.rkt.state = rkt_r2a.func;
			switch (rkt_r2a.func)
			{
			case 0x2://��������̬����
				gblState.att_to_rkt = 1;
				break;
			case 0x3://����ֹͣ������̬����
				gblState.att_to_rkt = 0;
				buf[0] = 0x2;
				buf[1] = 0x2;
				buf[2] = 0x0;
				buf[3] = 0x4;
				rkt_send(MSG_RKT_TYPE_A2R, 4, buf);//Ӧ���յ�ֹͣ��̬������Ϣ
				break;
			case 0x4://������û�д����������ж���ֹ
				buf[0] = 0x2;
				buf[1] = 0x3;
				buf[2] = 0x0;
				buf[3] = 0x5;
				rkt_send(MSG_RKT_TYPE_A2R, 4, buf);//Ӧ���յ�������ֹ��Ϣ
				break;
			case 0x5://���淢��ʧ�ܣ������������������
				buf[0] = 0x2;
				buf[1] = 0x4;
				buf[2] = rkt_r2a.data;
				buf[3] = buf[0]+buf[1]+buf[2];
				rkt_send(MSG_RKT_TYPE_A2R, 4, buf);//Ӧ���յ�����ʧ����Ϣ
				break;
			case 0x6://������Ϊ0x(1~4)�Ļ������ɹ�����׼������̽������
				s2m.rkt.rktnumber = rkt_r2a.data;//����ɹ��Ļ�����
				gblState.recv_rkt_data = 1;//׼������̽������
				buf[0] = 0x2;
				buf[1] = 0x5;
				buf[2] = rkt_r2a.data;
				buf[3] = buf[0] + buf[1] + buf[2];
				rkt_send(MSG_RKT_TYPE_A2R, 4, buf);//Ӧ���յ�����ɹ���Ϣ
				break;
			case 0x7://������Ϊ0x(1~4)�Ļ�����亣���������
				gblState.recv_rkt_data = 0;//׼������̽������
				buf[0] = 0x2;
				buf[1] = 0x6;
				buf[2] = rkt_r2a.data;
				buf[3] = buf[0] + buf[1] + buf[2];
				rkt_send(MSG_RKT_TYPE_A2R, 4, buf);//Ӧ���յ�̽�������Ϣ
				break;
			case 0x8://Ӧ�𣬸ղ��յ���ָ��δͨ��У�飬���ط�
				rkt_send(rkt_send_type, rkt_send_len, rkt_send_data);//�����ط�
				break;
			}
		}
	}
}

void rkt_data_process()
{
	if (rkt_file_opened == 1)//����̽������
	{
		memcpy(&rkt_rec._datatime, &datetime_now, sizeof(datetime_now));
		memcpy(&rkt_rec.data, &rkt_data, sizeof(rkt_data));
		fwrite(&rkt_rec, sizeof(rkt_rec), 1, fp_rkt);
	}
}


