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

#include "bd.h"
#include "log.h"
#include "global.h"

int fd_bd;
pthread_t bd_recv_thread_id;
pthread_t bd_send_thread_id;
FILE * fp_bd = NULL;
int bd_file_opened = 0;

T_BD_AP2GCS bd_ap2gcs;
T_BD_GCS2AP bd_gcs2ap;
T_BD_REC bd_rec;
int bd_send_data_ena;

void bd_recv_thread(void);
void bd_send_thread(void);

int bd_init(void)
{
	char file_name[100];

	//以追加方式打开数据记录文件
	sprintf(file_name, "BD%02d%02d_%02d%02d%02d.dat", datetime_now.month, datetime_now.day,
		datetime_now.hour, datetime_now.minute, datetime_now.second);

	bd_file_opened = 1;
	fp_bd = fopen(file_name, "wb+");
	if (NULL == fp_bd)
	{
		sprintf(logmsg, "Open BD rec file failed: %d", fp_bd);
		write_log(logmsg);
		bd_file_opened = 0;//没有正常打开
	}

	//初始化串口
	fd_bd = open(DEV_BD, O_RDWR);
	if (fd_bd < 0)
	{
		printf("Open DEV_BD failed: %d\n", fd_bd);
		return -1;
	}
	set_speed(fd_bd, BD_BAUDRATE); //set serial port baud
	set_parameter(fd_bd, 8, 1, 'N'); //8 data bits, 1 stop bit, no parity
	sprintf(logmsg, "BD init: %d bps", BD_BAUDRATE);
	write_log(logmsg);
	pthread_create(&bd_recv_thread_id, NULL, (void*)bd_recv_thread, NULL);//BD 接收数据线程
	pthread_create(&bd_send_thread_id, NULL, (void*)bd_send_thread, NULL);//BD 发送数据线程
}

int bd_send_cmd(unsigned char cmd_id)
{
	unsigned short len = 0;//包长度
	char sbuf[200];//包信息缓冲区
	unsigned char checksum;//校验和
	int i = 0;
	int j = 0;

	switch (cmd_id)
	{
	case CMD_GLJC:
		len = 12;
		strncpy(sbuf, "$GLJC", 5);//包ID
		sbuf[5] = (len >> 8) & 0xff;//包长度
		sbuf[6] = (len)& 0xff;
		sbuf[7] = 0;//本机地址
		sbuf[8] = 0;
		sbuf[9] = 0;
		sbuf[10] = 1;//单次输出		
		break;
	case CMD_ICJC:
		len = 12;
		strncpy(sbuf, "$ICJC", 5);//包ID
		sbuf[5] = (len >> 8) & 0xff;//包长度
		sbuf[6] = (len)& 0xff;
		sbuf[10] = 1;//单次输出		
		break;
	default:;
	}

	checksum = 0;
	for (i = 0; i<(len - 1); i++)
	{
		checksum ^= sbuf[i];
	}
	sbuf[len - 1] = checksum;
	write(fd_bd, sbuf, len);

	//显示数据
	sprintf(logmsg,">>: ");
	for (j = 0; j < len; j++) sprintf(&logmsg[4+j*3],"%02x ", sbuf[j]);
	write_log(logmsg);
}

int bd_send_data(unsigned short len, unsigned char *buf)
{
	unsigned short pack_len = 0;//包长度
	unsigned short msg_len_bits = 0;
	unsigned int _addr;
	char sbuf[200];//包信息缓冲区
	unsigned char checksum;//校验和
	int i = 0;
	int j = 0;

	pack_len = 18 + len;
	strncpy(sbuf, "$TXSQ", 5);//包ID
	sbuf[5] = (pack_len >> 8) & 0xff;//包长度
	sbuf[6] = (pack_len)& 0xff;
	_addr = _BD_ID_AP;
	sbuf[7] = (_addr >> 16) & 0xff;//本机地址
	sbuf[8] = (_addr >> 8) & 0xff;
	sbuf[9] = _addr & 0xff;
	sbuf[10] = 0x46;//信息类别为0x46
	_addr = _BD_ID_GCS;
	sbuf[11] = (_addr >> 16) & 0xff;//对方地址
	sbuf[12] = (_addr >> 8) & 0xff;
	sbuf[13] = (_addr)& 0xff;

	msg_len_bits = len * 8;
	sbuf[14] = (msg_len_bits >> 8) & 0xff;//包长度
	sbuf[15] = (msg_len_bits)& 0xff;

	sbuf[16] = 0;//不需要应答
	memcpy(&sbuf[17], buf, pack_len);

	checksum = 0;
	for (i = 0; i<(pack_len - 1); i++)
	{
		checksum ^= sbuf[i];
	}
	sbuf[pack_len - 1] = checksum;
	write(fd_bd, sbuf, pack_len);

	if (bd_file_opened == 1)//保存北斗数据
	{
		memcpy(&bd_rec._datatime, &datetime_now, sizeof(datetime_now));
		bd_rec.type = BD_REC_DATA_SEND;
		bd_rec.len = pack_len;
		memcpy(&bd_rec.data[0], buf, len);
		fwrite(&bd_rec, sizeof(bd_rec), 1, fp_bd);
	}

	//显示数据
	sprintf(logmsg, ">>: ");
	for (j = 0; j < pack_len; j++) sprintf(&logmsg[4+j*3], "%02x ", sbuf[j]);
	write_log(logmsg);
}

#define BD_RECVBUF_LEN 1024
char rcvQ[BD_RECVBUF_LEN];
int rcvQTail;
unsigned char recvpackbuf[200];

void bd_recv_thread(void)
{	
	int nread=0;
	int i = 0;
	int j=0;
	unsigned char buf[500];

	unsigned short pack_len;
	unsigned char checksum;

	rcvQTail = 0;
	while (1)
	{
		nread = read(fd_bd, buf, 500);

		if (nread > 0)//收到数据
		{
			memcpy(&rcvQ[rcvQTail], buf, nread);
			rcvQTail += nread;

			if (rcvQTail >= 12)//大于最小包的长度，可以进行处理
			{
				for (i = 0; i < (rcvQTail - 7); i++) //rcvQTail-7:除去头部ID的5个字节和长度的2个字节
				{
					if ((strncmp(&rcvQ[i], STR_GLZK, 5) == 0)
						|| (strncmp(&rcvQ[i], STR_TXXX, 5) == 0)//通信信息
						|| (strncmp(&rcvQ[i], STR_ICXX, 5) == 0)//IC信息
						|| (strncmp(&rcvQ[i], STR_FKXX, 5) == 0)//反馈信息
						)//包头正确
					{
						//memcpy(&pack_len,&rcvQ[i+5],2);
						pack_len = ((rcvQ[i + 5] << 8) & 0xff00) | (rcvQ[i + 6] & 0xff);
						if ((rcvQTail - i) >= pack_len)//缓冲区内数据长度超过包长度
						{
							memcpy(recvpackbuf, &rcvQ[i], pack_len);
							checksum = 0;
							for (j = 0; j < (pack_len - 1); j++)
							{
								checksum ^= recvpackbuf[j];
							}
							if (checksum == recvpackbuf[pack_len - 1])//包校验正确，表示收到正确数据包，可以进行解析
							{
								//解析完，删除已解析的数据
								memcpy(rcvQ, &rcvQ[i + pack_len], (rcvQTail - (i + pack_len)));
								rcvQTail = rcvQTail - (i + pack_len);

								//处理数据
								bd_recv_process(pack_len,recvpackbuf);

								//保存北斗操作
								sprintf(logmsg, "<<: ");
								for (j = 0; j < pack_len; j++) sprintf(&logmsg[4+j*3], "%02x ", recvpackbuf[j]);
								write_log(logmsg);
							}
						}//if((rcvQTail-i)>=pack_len)
					}
				}//for
			}// if(rcvQTail>=12)
			if (rcvQTail > 500)
			{
				rcvQTail = 0;
			}
		}
	}
	return;
}

void bd_recv_process(unsigned short len, unsigned char *buf)
{
	int data_len;
	int i = 0;
	unsigned char checksum;
	unsigned char _data[500];
	int bd_peer_id;

	if ((strncmp(buf, STR_TXXX, 5) == 0))
	{
		memcpy(&bd_peer_id,&buf[11],3);
		printf("BD Recv addr: 0x%x(%d)", bd_peer_id, bd_peer_id);
		if (bd_peer_id == _BD_ID_GCS)
		{
			memcpy(&data_len,&buf[16],2);
			printf("BD Recv data len: 0x%x(%d)", data_len, data_len);

			if (data_len <= 80)//最大80字节
			{
				memcpy(&bd_gcs2ap, &buf[18], data_len);
			}
			//保存数据
			if (bd_file_opened == 1)//保存北斗数据
			{
				memcpy(&bd_rec._datatime, &datetime_now, sizeof(datetime_now));
				bd_rec.type = BD_REC_DATA_RECV;
				bd_rec.len = data_len;
				memcpy(&bd_rec.data[0], &buf[18], data_len);
				fwrite(&bd_rec, sizeof(bd_rec), 1, fp_bd);
			}
		}
	}
}

void bd_send_thread(void)
{
	int i = 0;
	while (1)
	{
		bd_send_data_ena = 0;
		bd_send_cmd(CMD_GLJC);
		sleep(1);
		bd_send_data_ena = 1;//for test

		if (bd_send_data_ena == 1)
		{
			printf("%d:%d:\n",datetime_now.minute,datetime_now.second);
			bd_ap2gcs.msgid = MSGID_AP_REAL;
			bd_ap2gcs.cnt++;
			for (i = 0; i < 78; i++) bd_ap2gcs.data[i] = 0xa5;
			bd_send_data(sizeof(T_BD_AP2GCS), (unsigned char*)&bd_ap2gcs);
			sleep(60);//[sec]
		}
	}
}


