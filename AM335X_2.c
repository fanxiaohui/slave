#include<stdio.h>  
#include<sys/types.h>  
#include<sys/stat.h>  
#include<fcntl.h>  
#include<sys/select.h>  
#include<unistd.h>  
#include<signal.h>  
#include<string.h>  
#include<pthread.h> 
#include<sys/ioctl.h> 
#include <errno.h>
#include <linux/ioctl.h>
#include <linux/watchdog.h>

#include "udp.h"
#include "global.h"
#include "log.h"
#include "aws.h"
#include "rkt.h"
#include "bd.h"
#include "modbus_polling.h"

#define SYSCONFIG_FILE "SLAVE_CFG.dat"

//int udprecv_permission=0;
int udprecv_permission=1;
pthread_t udprecv_thread_id;
void udprecv_thread(void);

int fd_watchdog;
FILE * fp_sys = NULL;

//wangbo20170112 test
static int udp_cnt;

void m2s_process();

int main(void)  
{  
	int seconds = 0;
	int mseconds = _TIMER_INTERVAL;//[us]
	struct timeval timerInterval;
	time_t timep;
	int tick = 0;
	int ret = 0;
	int data = 0;

	//获取系统时间
	time(&timep);
	gbl_time_val = localtime(&timep);
	datetime_now.year = (unsigned char)(gbl_time_val->tm_year + 1900);
	datetime_now.month = (unsigned char)(gbl_time_val->tm_mon + 1);
	datetime_now.day = (unsigned char)gbl_time_val->tm_mday;
	datetime_now.hour = (unsigned char)gbl_time_val->tm_hour;
	datetime_now.minute = (unsigned char)gbl_time_val->tm_min;
	datetime_now.second = (unsigned char)gbl_time_val->tm_sec;

	//创建日志文件
	create_logfile();

	sprintf(logmsg, "Start slave. ver: 2017-01-20-1");
	write_log(logmsg);

	//读取配置文件，用于恢复系统到上次启动的状态
	fp_sys = fopen(SYSCONFIG_FILE, "rb");
	if (NULL == fp_sys)
	{
		sprintf(logmsg, "can't open file SYSCONFIG_FILE, create it");
		write_log(logmsg);
		fp_sys = fopen(SYSCONFIG_FILE, "wb+");
		fseek(fp_sys, 0, SEEK_SET);//定位文件指针到文件开始位置
	}
	else
	{
		//此处可添加
		fread(&sys_config, 1, sizeof(sys_config), fp_sys);
		memcpy(&gblState, &sys_config.gblState, sizeof(gblState));
	}

	//初始化UDP
	udp_init(IP_MASTER,UDP_M_RECV_PORT,UDP_S_RECV_PORT);//udp_init(ip_sendto,port_sendto,port_myrecv)
	

	//BD初始化
	//bd_init();

	//modbus总线初始化
	modbus_init();
	sem_init(&sem_modbus, 0, 1);//初始化modbus总线管理线程信号量

	//气象站初始化
	aws_init();

	//火箭初始化
	rkt_init();

	//UDP接收线程
	pthread_create(&udprecv_thread_id, NULL, (void*)udprecv_thread, NULL);//udprecv thread	

	sprintf(logmsg, "create udp receive thread.");
	write_log(logmsg);
	udprecv_permission = 1;

	fd_watchdog = open("/dev/watchdog", O_WRONLY);
	if (fd_watchdog == -1) 
	{
		sprintf(logmsg, "create watchdog failed.");
		write_log(logmsg);
	}

	ret = ioctl(fd_watchdog, WDIOC_GETTIMEOUT, &data);
	if (ret) 
	{
		sprintf(logmsg, "Watchdog Timer : WDIOC_GETTIMEOUT failed.");
		write_log(logmsg);
	}
	else 
	{
		sprintf(logmsg, "Current timeout value is : %d seconds", data);
		write_log(logmsg);
	}

	//主程序给出定时中断.
	while(1)
	{
		//定时器，定时时间由seconds和mseconds确定，目前为20ms
		timerInterval.tv_sec = seconds;
		timerInterval.tv_usec = mseconds;
		select(0,NULL,NULL,NULL, &timerInterval);

		//定时器计数			
		tick ++;
		if(tick>999999) tick = 0;


		if ((tick%_UDPSEND_CNT) == 0)//定时发送UDP数据包 [100ms]
		{
			s2m.head = S2M_HEAD;
			s2m.heart_beat++;

			s2m.switcher.voltage[0] = switcher_state.voltage[0];
			s2m.switcher.voltage[1] = switcher_state.voltage[1];
			s2m.switcher.workstate[0] = switcher_state.workstate[0];
			s2m.switcher.workstate[1] = switcher_state.workstate[1];
			s2m.switcher.voltage_llim = m2s.switcher.voltage_llim;//switcher_config.voltage_llim;
			s2m.switcher.voltage_hlim = m2s.switcher.voltage_hlim;//switcher_config.voltage_hlim;

			udp_send(sizeof(s2m), &s2m);
		}
			
		if((tick%_ONESEC_CNT)==0)//refresh feq: 1Hz
		{
			//获取系统时间，作为各种记录的时间
			time(&timep);
			gbl_time_val = localtime(&timep);
			//将获取的时钟信息写入时间结构
			datetime_now.year = (unsigned char)(gbl_time_val->tm_year + 1900);
			datetime_now.month = (unsigned char)(gbl_time_val->tm_mon + 1);
			datetime_now.day = (unsigned char)gbl_time_val->tm_mday;
			datetime_now.hour = (unsigned char)gbl_time_val->tm_hour;
			datetime_now.minute = (unsigned char)gbl_time_val->tm_min;
			datetime_now.second = (unsigned char)gbl_time_val->tm_sec;

			if (gblState.att_to_rkt)//发送姿态数据给火箭系统
			{
				rkt_att.lon = m2s.lon;
				rkt_att.lat = m2s.lat;
				rkt_att.alt = m2s.alt;
				rkt_att.pitch = m2s.pitch;
				rkt_att.roll = m2s.roll;
				rkt_att.yaw = m2s.yaw;
				rkt_att.check = 0;//to be done
				rkt_send(MSG_RKT_TYPE_ATT, sizeof(rkt_att), (unsigned char*)&rkt_att);//应答，收到发射中止消息
			}

		}	

		if ((tick%_TWOSEC_CNT) == 0)//每2秒执行一次modbus管理线程
		{
			sem_post(&sem_modbus);
			//printf("M2S.heartbeat=%d\n",m2s.heart_beat);
		}
		if ((tick%_TENSEC_CNT) == 0)//每10秒执行一次
		{
			//if(s2m.generator_onoff_req == 0) s2m.generator_onoff_req = 1;
			//else s2m.generator_onoff_req = 0;
		}

		if ((tick%(_ONESEC_CNT*30)) == 0)//每30秒执行一次喂狗
		{
			if (fd_watchdog >= 0) 
			{
				if (1 != write(fd_watchdog, "\0", 1))
				{
					sprintf(logmsg, "FAILED feeding watchdog");
					write_log(logmsg);
				}
			}
		}

	}
	return 0;  
} 

//UDP接收线程
void udprecv_thread(void)
{
	int nread,i;
	char buf[512];
	int packhead;
	int packcnt;
	int packlen;

	while (udprecv_permission)
	{//udp_cnt++;

		nread = udp_recv();
		if(nread>0)
		{
			//printf(".........recv:%d\n",nread);
			memcpy(&packhead, udp_recvbuf, 4);
			if (packhead == ntohl(UDP_PACKET_HEAD))
			{
				memcpy(&packcnt, &udp_recvbuf[4], 4);
				//memcpy(&m2s, &udp_recvbuf[8], sizeof(m2s));
				memcpy(&m2s, &udp_recvbuf[12], sizeof(m2s));
				m2s_process();//调用处理m2s数据的函数

			}
		}
	}
	//pthread_exit(NULL); 
	
	return;
}

//此处用于处理从主控接收到的数据指令
//主要用于处理火箭
//和485总线相关的都放在modbus_polling.c中了
void m2s_process()
{
	unsigned char buf[200];

	if (m2s.rkt.launch_req == 0) s2m.rkt.launch_req_ack = 0;
	if (m2s.switcher.enable_req == 0) s2m.switcher.enable_req_ack = 0;
	if (m2s.switcher.disable_req == 0) s2m.switcher.disable_req_ack = 0;
	if (m2s.switcher.ch0_on_req == 0) s2m.switcher.ch0_on_req_ack = 0;
	if (m2s.switcher.ch1_on_req == 0) s2m.switcher.ch1_on_req_ack = 0;
	if (m2s.switcher.llim_req == 0) s2m.switcher.llim_req_ack = 0;
	if (m2s.switcher.hlim_req == 0) s2m.switcher.hlim_req_ack = 0;

	//火箭处理
	if ((m2s.rkt.launch_req ==1) && (s2m.rkt.launch_req_ack==0))
	{
		buf[0] = 0x2;
		buf[1] = 0x0;
		buf[2] = 0x1;
		buf[3] = 0x3;
		rkt_send(MSG_RKT_TYPE_A2R, 4, buf);//向火箭系统发送请求发射指令
		s2m.rkt.launch_req_ack = 1;
	}
}



