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
#include "aws.h"
#include "log.h"
#include "global.h"
#include "uart.h"
#include "crc.h"

#define AWS_PACKAGE_LEN 72
#define AWS_DATA_LEN 69
#define AWS_CRC_LEN 2
#define AWS_PACKAGE_HEAD 0x7e

#define SM_AWS_RECV_HEAD 0
#define SM_AWS_RECV_DATA 1
#define SM_AWS_RECV_CRC1 2
#define SM_AWS_RECV_CRC2 3

int fd_aws;
pthread_t aws_recv_thread_id;

int aws_recv_sm;
int aws_data_cnt = 0;
unsigned char aws_data_buf[72];
unsigned char aws_crc[2];

T_AWS_DATA aws_data;
T_AWS_REC aws_rec;

FILE * fp_aws = NULL;
int aws_file_opened = 0;

void aws_recv_thread(void);
void aws_process();

int aws_init()
{
	char file_name[100];

	//��׷�ӷ�ʽ�����ݼ�¼�ļ�
//	sprintf(file_name, "AWS%2d%2d-%2d%2d%2d.dat", gbl_time_val->tm_mon + 1, gbl_time_val->tm_mday,
//		gbl_time_val->tm_hour, gbl_time_val->tm_min, gbl_time_val->tm_sec);

	sprintf(file_name, "AWS%02d%02d_%02d%02d%02d.dat", datetime_now.month, datetime_now.day,
		datetime_now.hour, datetime_now.minute, datetime_now.second);

	aws_file_opened = 1;
	fp_aws = fopen(file_name, "wb+");
	if (NULL == fp_aws)
	{
		sprintf(logmsg, "Open AWS rec file failed: %d", (int)fp_aws);
		write_log(logmsg);
		aws_file_opened = 0;//û��������
	}

	//��ʼ������վ����
	fd_aws = open(DEV_AWS_RS232, O_RDWR);
	if (fd_aws < 0)
	{
		sprintf(logmsg, "Open DEV_AWS_RS232 failed: %d", fd_aws);
		write_log(logmsg);
		return -1;
	}
	set_speed(fd_aws, 9600); //set serial port baud
	set_parameter(fd_aws, 8, 1, 'N'); //8 data bits, 1 stop bit, no parity
	pthread_create(&aws_recv_thread_id, NULL, (void*)aws_recv_thread, NULL);//RS232 receive thread
	sprintf(logmsg, "AWS RS232 init: 9600 bps");
	write_log(logmsg);
	return 0;
}


void aws_recv_thread(void)
{	
	int i,nread;
	unsigned char buf[200];
	unsigned short crc_tmp=0;
	unsigned char crc[2];
	static int aws_recv_sm_old=0;

	aws_recv_sm = SM_AWS_RECV_HEAD;
	while(1)    
	{
		nread = read(fd_aws, buf, 200);
		if(0)
		/*if(nread>0)*/ 
		{
			printf("AWS recv(%d): ",nread);
			for(i=0;i<nread;i++) printf("0x%x,",buf[i]);
			printf("\n");
		}

		for (i = 0; i < nread; i++)
		{
			switch (aws_recv_sm)
			{
			case SM_AWS_RECV_HEAD:
				if (buf[i] == AWS_PACKAGE_HEAD)
				{
					aws_data_buf[0] = buf[i];
					aws_data_cnt = 1;
					aws_recv_sm = SM_AWS_RECV_DATA;
				}
				break;
			case SM_AWS_RECV_DATA:
				aws_data_buf[aws_data_cnt] = buf[i];
				aws_data_cnt++;
				if (aws_data_cnt >= (AWS_PACKAGE_LEN - 2)) aws_recv_sm = SM_AWS_RECV_CRC1;
				break;
			case SM_AWS_RECV_CRC1:
				aws_crc[0] = buf[i];
				aws_recv_sm = SM_AWS_RECV_CRC2;
				break;
			case SM_AWS_RECV_CRC2:
				aws_crc[1] = buf[i];
				memset(buf,0,200);
				aws_recv_sm = SM_AWS_RECV_HEAD;
				//parse
				crc_tmp = crc16(aws_data_buf, aws_data_cnt);
				crc[0] = (unsigned char)(crc_tmp >> 8 & 0xff);
				crc[1] =  (unsigned char)(crc_tmp & 0xff);
				//printf("CRC: r[0]=0x%x,r[1]=0x%x, c[0]=0x%x,c[1]=0x%x\n",aws_crc[0],aws_crc[1],crc[0],crc[1]);
				if ((aws_crc[0] == crc[0]) && (aws_crc[1] == crc[1]))//У������
				{
					memcpy(&aws_data, aws_data_buf, (AWS_PACKAGE_LEN - 2));
					aws_process();
				}
				break;
			default:;
				aws_recv_sm = SM_AWS_RECV_HEAD;
			}
			/*if(0)*/if(aws_recv_sm != aws_recv_sm_old)
			{
				printf("AWS: %d-->%d\n",aws_recv_sm_old,aws_recv_sm);
			}
			aws_recv_sm_old = aws_recv_sm;
		}
		sleep(1);
	}
	return;
}

void aws_process()
{
	static int cnt=0;

	cnt++;
	s2m.aws.temp = (unsigned char)(aws_data.temp/50);
	s2m.aws.humi = (unsigned char)(aws_data.humi);
	s2m.aws.windspeed = (unsigned char)(aws_data.windspeed);
	s2m.aws.winddir = (unsigned char)(aws_data.winddir / 2);
	s2m.aws.airpress = (unsigned char)((aws_data.airpress - 300) / 4);
	s2m.aws.seasault = (unsigned char)(aws_data.seasault / 500);
	s2m.aws.elec_cond = (unsigned char)(aws_data.elec_cond / 500);
	s2m.aws.seatemp1 = (unsigned char)(aws_data.seatemp1 / 50);

	if (aws_file_opened == 1)//����̽������
	{
		memcpy(&aws_rec._datatime, &datetime_now, sizeof(datetime_now));
		memcpy(&aws_rec.data, &aws_data, sizeof(aws_data));
		fwrite(&aws_rec, sizeof(aws_rec), 1, fp_aws);
	}
	//printf("AWS temp: %d,%d,%d\n",aws_data.seatemp1,aws_data.seatemp2,aws_data.seatemp3);
/*
	printf("AWS(%d): T:%d, H:%d, wspd:%d, wdir:%d, P:%d, sault:%d, cond:%d, seatemp:%d\n",
		cnt,s2m.aws.temp,s2m.aws.humi,s2m.aws.windspeed,s2m.aws.winddir,s2m.aws.airpress,s2m.aws.seasault,s2m.aws.elec_cond,s2m.aws.seatemp1);
*/
}
	
