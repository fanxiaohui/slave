/////////////////////////////////////////////////////////////////////////////////////////////
// modbus_polling.c - ��ѯRS485�����ϵ��豸�������л����ͳ���
//
//
////////////////////////////////////////////////////////////////////////////////////////////
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
#include "modbus_polling.h"
#include "log.h"
#include "global.h"
#include "crc.h"
#include "uart.h"

//switcher macro define
#define SW_ST_DISCHARGING 0x55
#define SW_ST_REQUEST 0x5a
#define SW_ST_STOP 0xaa

#define SW_CMD_SWITCH_ENABLE 0x55
#define SW_CMD_SWITCH_DISABLE 0xaa
#define SW_CMD_SWITCH_TO_CH0 0x1
#define SW_CMD_SWITCH_TO_CH1 0x2
#define SW_CMD_AUTO 0x55
#define SW_CMD_SHUTDOWN 0xaa

//state machine
#define SM_MODBUS_RECV_IDLE 0
#define SM_MODBUS_RECV_ADDR 1
#define SM_MODBUS_RECV_FLAG 2
#define SM_MODBUS_RECV_DATA 3
#define SM_MODBUS_RECV_CRC1 4
#define SM_MODBUS_RECV_CRC2 5

#define SM_CHARGER_OFF 0
#define SM_CHARGER_TURNOFF 1
#define SM_CHARGER_SET_CHANNEL 2
#define SM_CHARGER_SET_VOLTAGE 3
#define SM_CHARGER_SET_CURRENT 4
#define SM_CHARGER_TURNON 5
#define SM_CHARGER_QUARY 6
#define SM_CHARGER_WAIT 7
#define SM_CHARGER_DUMMY 8

T_SWITCHER_DATA switcher_state;
T_SWITCHER_DATA switcher_state_tmp;
T_SWITCHER_CFG switcher_config;
T_CHARGER_DATA charger_state;
T_CHARGER_DATA charger_state_tmp;
T_CHARGER_CFG charger_config;

T_MODBUS_PARA _modbus;//MODBUSͨ�Ź������������ȫ����������ṹ��
T_MODBUS_ERROR modbus_error;

//modbus����UART(RS485)�˿�
int fd_modbus;

pthread_t modbus_recv_thread_id;
pthread_t modbus_master_thread_id;
sem_t sem_modbus;
unsigned char charger_workchan_wish12 = 0;

int switcher_sm;//�л���״̬��
int charger_sm;//����״̬��
int charger_channel;//���ͨ��

//��ȡ�л�����״̬��������ص�ѹ��
//IN: timeout_ms �ȴ�ʱ��[ms]����С�ֱ���Ϊ10ms
//OUT: �������ֵΪ1����data ���ص��״̬�����򷵻�������Ч

//modbus�����̣߳�������RS485�����Ͻ�������modbus����
void modbus_recv_thread(void);
//modbus��ѯ�̣߳������߳��е��ź���������ѯ����
void modbus_master_thread(void);
//�����л��������͹���״̬
int modbus_set_switcher(unsigned short addr, unsigned short value, int timeout_ms);
//��ȡ�л�����״̬
int modbus_get_switcher_state(T_SWITCHER_DATA* data, int timeout_ms);
//���ó��������͹���״̬
int modbus_set_charger(unsigned short addr, unsigned short value, int timeout_ms);
//��ȡ������״̬
int modbus_get_charger_state(T_CHARGER_DATA* data, int timeout_ms);
//
void modbus_polling();


int modbus_init()
{
	//MODBUS�����ϵ����ݼ�¼ȫ������ȫ�ּ�¼��,��ˣ��ڳ�ʼ��ʱ�����´�����¼�ļ�

	//��ʼ��MODBUS����
	fd_modbus = open(DEV_MODBUS, O_RDWR);
	if (fd_modbus < 0)
	{
		sprintf(logmsg, "Open DEV_MODBUS failed: %d", fd_modbus);
		write_log(logmsg);
		return -1;
	}
	set_speed(fd_modbus, 9600); //set serial port baud
	set_parameter(fd_modbus, 8, 1, 'N'); //8 data bits, 1 stop bit, no parity
	pthread_create(&modbus_recv_thread_id, NULL, (void*)modbus_recv_thread, NULL);//RS485 �����߳�
	pthread_create(&modbus_master_thread_id, NULL, (void*)modbus_master_thread, NULL);//Modbus �����߳�
	sprintf(logmsg, "MODBUS init: 9600 bps");
	write_log(logmsg);

	charger_config.set_channel = 0;
	charger_config.set_voltage = CHARGER_VOLTAGE_DFT;
	charger_config.set_current = CHARGER_CURRENT_DFT;
	return 0;
}

#define _RECVBUF_LEN 500
unsigned char rcvQ[_RECVBUF_LEN];
int rcvQTail;

//#define _NEW_CODE

void modbus_recv_thread(void)
{
	int nread,i,j,cnt;
	unsigned char buf[500];
	unsigned char mb_buf[200];
	unsigned short crc_tmp = 0;
	unsigned char _crc[2];
	int head=0;
	int get_pack = 0;

	_modbus.recv_sm = SM_MODBUS_RECV_ADDR;
	rcvQTail = 0;
	while (1)
	{
		nread = read(fd_modbus, buf, 500);

#ifndef _NEW_CODE
		for (i = 0; i < nread; i++)
		{
			switch (_modbus.recv_sm)
			{
			case SM_MODBUS_RECV_ADDR:
				if (buf[i] == _modbus.recv_addr)
				{
					mb_buf[0] = buf[i];
					_modbus.recv_cnt = 1;
					_modbus.recv_sm = SM_MODBUS_RECV_FLAG;
				}
				break;
			case SM_MODBUS_RECV_FLAG:
				if (buf[i] == _modbus.recv_flag)
				{
					mb_buf[1] = buf[i];
					_modbus.recv_cnt++;
					_modbus.recv_sm = SM_MODBUS_RECV_DATA;
				}
				else
				{
					_modbus.recv_sm = SM_MODBUS_RECV_ADDR;

				}

				break;
			case SM_MODBUS_RECV_DATA:
				mb_buf[_modbus.recv_cnt] = buf[i];
				_modbus.recv_cnt++;

				if (_modbus.recv_cnt >= _modbus.recv_len)//recv_len:����CRC�İ�����
				{
					_modbus.recv_sm = SM_MODBUS_RECV_CRC1;
				}

				break;
			case SM_MODBUS_RECV_CRC1:
				_crc[0] = buf[i];
				_modbus.recv_sm = SM_MODBUS_RECV_CRC2;

				break;
			case SM_MODBUS_RECV_CRC2:
				_crc[1] = buf[i];
				crc_tmp = crc16(mb_buf, _modbus.recv_cnt);

				//printf("recv crc: %x,%x; calc: %x,%x\n",(crc_tmp >> 8) & 0xff,(crc_tmp & 0xff),_crc[0],_crc[1]);
				if ((_crc[0] == ((crc_tmp >> 8) & 0xff)) && (_crc[1] == (crc_tmp & 0xff)))//������ȷ���ɴ�������
				{
					if ((mb_buf[0] == _modbus.recv_addr) && (mb_buf[1] == _modbus.recv_flag))

					{
						//��ȡ״ֵ̬
						memcpy(_modbus.buf,mb_buf,_modbus.recv_len);
						_modbus.recv_ok = 1;

						_DBG_0("                                                  recv data: ");
						for(j=0;j<_modbus.recv_len;j++) printf("%x ",mb_buf[j]);
						_DBG_0("\n");
					}

				}
				_modbus.recv_sm = SM_MODBUS_RECV_ADDR;
				break;
			default:;

			}
		}
#else 
		if(nread>0)
		{
			memcpy(&rcvQ[rcvQTail],buf,nread);
			rcvQTail += nread;

			i = 0;
			get_pack = 0;
			while((i<rcvQTail)&&(get_pack==0))
			{
				switch (_modbus.recv_sm)
				{
				case SM_MODBUS_RECV_ADDR:
					if (rcvQ[i] == _modbus.recv_addr)
					{
						mb_buf[0] = rcvQ[i];
						_modbus.recv_cnt = 1;
						_modbus.recv_sm = SM_MODBUS_RECV_FLAG;
						head = i;
						i++;
						cnt=1;
					}
					break;
				case SM_MODBUS_RECV_FLAG:
					if (rcvQ[i] == _modbus.recv_flag)
					{
						mb_buf[1] = rcvQ[i];
						_modbus.recv_cnt++;
						_modbus.recv_sm = SM_MODBUS_RECV_DATA;
						i++;
						cnt++;
					}
					else
					{
						head++;
						i = head;
						_modbus.recv_sm = SM_MODBUS_RECV_ADDR;
					}
					break;
				case SM_MODBUS_RECV_DATA:
					mb_buf[_modbus.recv_cnt] = rcvQ[i];
					_modbus.recv_cnt++;
					if (_modbus.recv_cnt >= _modbus.recv_len)//recv_len:����CRC�İ�����
					{
						_modbus.recv_sm = SM_MODBUS_RECV_CRC1;
					}
					i++;
					cnt++;
					break;
				case SM_MODBUS_RECV_CRC1:
					_crc[0] = rcvQ[i];
					_modbus.recv_sm = SM_MODBUS_RECV_CRC2;
					i++;
					cnt++;
					break;
				case SM_MODBUS_RECV_CRC2:
					_crc[1] = rcvQ[i];
					crc_tmp = crc16(mb_buf, _modbus.recv_cnt);
					//printf("recv crc: %x,%x; calc: %x,%x\n",(crc_tmp >> 8) & 0xff,(crc_tmp & 0xff),_crc[0],_crc[1]);
					if ((_crc[0] == ((crc_tmp >> 8) & 0xff)) && (_crc[1] == (crc_tmp & 0xff)))//������ȷ���ɴ�������
					{
						if ((mb_buf[0] == _modbus.recv_addr) && (mb_buf[1] == _modbus.recv_flag))
						{
							//��ȡ״ֵ̬

							memcpy(_modbus.buf,mb_buf,_modbus.recv_len);
							_modbus.recv_ok = 1;
							//_DBG_0("recv data: ");
							//for(j=0;j<_modbus.recv_len;j++) printf("%x ",mb_buf[j]);

							//_DBG_0("\n");
						}
						i++;
						cnt++;
						memcpy(rcvQ,&rcvQ[head],rcvQTail-head-cnt);
						rcvQTail = rcvQTail-head-cnt;
						get_pack = 1;
					}
					else
					{
						head++;
						i = head;
					}
					_modbus.recv_sm = SM_MODBUS_RECV_ADDR;
					break;
				default:;
				}
			}
		}
#endif
	}
	return;
}

int modbus_master_process_ena=0;

void modbus_master_thread(void)
{
	int ret = 0;
	static T_M2S_SWITCHER m2s_switcher_old;
	static T_S2M_SWITCHER s2m_switcher_old;
	static unsigned char generator_state_old;
	unsigned short us_tmp;

	//set switcher vol llim
	us_tmp = (unsigned short)(SWITCHER_DISCHARGE_DIS_VOLTAGE_DFT * 10);
	ret = modbus_set_switcher(0x20, us_tmp, MODBUS_TIMEOUT_MS);//set switcher discharge llim
	if (ret == 1)
	{
		sprintf(logmsg, "INI set switcher llim to %d",SWITCHER_DISCHARGE_DIS_VOLTAGE_DFT);
		write_log(logmsg);
	}
	sleep(1);
	//set switcher vol hlim
	us_tmp = (unsigned short)(SWITCHER_DISCHARGE_ENA_VOLTAGE_DFT *10);
	ret = modbus_set_switcher(0x21, us_tmp, MODBUS_TIMEOUT_MS);//set switcher discharge hlim
	if (ret == 1)
	{
		sprintf(logmsg, "INI set switcher hlim to %d",SWITCHER_DISCHARGE_ENA_VOLTAGE_DFT);
		write_log(logmsg);
	}
	sleep(1);
	//set charger voltage
	us_tmp = (unsigned short)(CHARGER_VOLTAGE_DFT * 10);//change unit
	ret = modbus_set_charger(0x10, us_tmp, MODBUS_TIMEOUT_MS);//set charger voltage
	if (ret == 1)
	{
		sprintf(logmsg, "INI set charger voltage to %d",CHARGER_VOLTAGE_DFT);
		write_log(logmsg);
	}
	sleep(1);
	//set charger current
	us_tmp = (unsigned short)(CHARGER_CURRENT_DFT * 10);
	ret = modbus_set_charger(0x11, us_tmp, MODBUS_TIMEOUT_MS);//set charger current
	if (ret == 1)
	{
		sprintf(logmsg, "INI set charger current to %d",CHARGER_CURRENT_DFT);
		write_log(logmsg);
	}
	sleep(1);

	ret = modbus_set_switcher(0x10, 0xaaaa, MODBUS_TIMEOUT_MS);//set all channel discharge enable
	sprintf(logmsg, "INI set switcher charging state to 0xaaaa");
	write_log(logmsg);

	gblState.switcher_enable = 1;//for test
	gblState.charger_enable = 1;//0: disable charger; 1:enable charger

	while (1)
	{
		sem_wait(&sem_modbus);//2sec

		if (m2s.switcher.llim_req == 0) s2m.switcher.llim_req_ack = 0;
		if (m2s.switcher.hlim_req == 0) s2m.switcher.hlim_req_ack = 0;
		if (m2s.switcher.ch0_on_req == 0) s2m.switcher.ch0_on_req_ack = 0;
		if (m2s.switcher.ch1_on_req == 0) s2m.switcher.ch1_on_req_ack = 0;
		if (m2s.switcher.enable_req == 0) s2m.switcher.enable_req_ack = 0;
		if (m2s.switcher.disable_req == 0) s2m.switcher.disable_req_ack = 0;
		//set switcher lim by master<--GCS
		if ((m2s.switcher.llim_req == 1) && (s2m.switcher.llim_req_ack == 0))
		{
			sprintf(logmsg, "GCS set switcher voltage_llim = %d", m2s.switcher.voltage_llim);
			write_log(logmsg);
			us_tmp = (unsigned short)(m2s.switcher.voltage_llim * 10);
			ret = modbus_set_switcher(0x20, us_tmp, MODBUS_TIMEOUT_MS);//set switcher discharge llim
			if (ret == 1)
			{
				switcher_config.voltage_llim = m2s.switcher.voltage_llim;
			}
			s2m.switcher.llim_req_ack = 1;
		}
		else if ((m2s.switcher.hlim_req == 1) && (s2m.switcher.hlim_req_ack == 0))
		{
			sprintf(logmsg, "GCS set switcher voltage_hlim = %d", m2s.switcher.voltage_hlim);
			write_log(logmsg);
			us_tmp = (unsigned short)(m2s.switcher.voltage_hlim *10);
			ret = modbus_set_switcher(0x21, us_tmp, MODBUS_TIMEOUT_MS);//set switcher discharge hlim
			if (ret == 1)
			{
				switcher_config.voltage_hlim = m2s.switcher.voltage_hlim;
			}
			s2m.switcher.hlim_req_ack = 1;
		}
		//set switcher discharge channel by master<--GCS
		else if ((m2s.switcher.ch0_on_req == 1) && (s2m.switcher.ch0_on_req_ack == 0))
		{
			sprintf(logmsg, "GCS set switcher ch0_on_req");
			write_log(logmsg);
			ret = modbus_set_switcher(0x22, 0x1, MODBUS_TIMEOUT_MS);//set switcher chan to channel0
			if (ret == 1)
			{
				//_DBG_0("ch0_on_req ok\n");
			}
			s2m.switcher.ch0_on_req_ack = 1;
		}
		else if ((m2s.switcher.ch1_on_req == 1) && (s2m.switcher.ch1_on_req_ack == 0))
		{
			sprintf(logmsg, "GCS set switcher ch1_on_req");
			write_log(logmsg);
			ret = modbus_set_switcher(0x22, 0x2, MODBUS_TIMEOUT_MS);//set switcher chan to channel1
			if (ret == 1)
			{
				//_DBG_0("ch1_on_req ok\n");
			}
			s2m.switcher.ch1_on_req_ack = 1;
		}
		//set switcher discharge enable or disable by master<--GCS
		else if ((m2s.switcher.enable_req == 1) && (s2m.switcher.enable_req_ack == 0))
		{
			sprintf(logmsg, "GCS set switcher enable");
			write_log(logmsg);
			ret = modbus_set_switcher(0x22, 0x55, MODBUS_TIMEOUT_MS);//set switcher work enable
			if (ret == 1)
			{
				//_DBG_0("sw enable ok\n");
				gblState.switcher_enable = 1;
			}
			s2m.switcher.enable_req_ack = 1;
		}
		else if ((m2s.switcher.disable_req == 1) && (s2m.switcher.disable_req_ack == 0))
		{
			sprintf(logmsg, "GCS set switcher disable");
			write_log(logmsg);
			ret = modbus_set_switcher(0x22, 0xaa, MODBUS_TIMEOUT_MS);//set switcher work disable
			if (ret == 1)
			{
				gblState.switcher_enable = 0;
			}
			s2m.switcher.disable_req_ack = 1;
		}

		//CHARGER
		//clear 
		if(m2s.charger.set_voltage_req == 0) s2m.charger.set_voltage_req_ack = 0;
		if(m2s.charger.set_current_req == 0) s2m.charger.set_current_req_ack = 0;
		if(m2s.charger.set_channel_req == 0) s2m.charger.set_channel_req_ack = 0;
		if(m2s.charger.turn_on_req == 0) s2m.charger.turn_on_req_ack = 0;
		if(m2s.charger.turn_off_req == 0) s2m.charger.turn_off_req_ack = 0;

		if ((m2s.charger.set_voltage_req == 1) && (s2m.charger.set_voltage_req_ack == 0))
		{
			sprintf(logmsg, "GCS set charger set_charger_voltage = %d", m2s.charger.set_voltage);
			write_log(logmsg);
			us_tmp = (unsigned short)(m2s.charger.set_voltage * 10);//change unit
			ret = modbus_set_charger(0x10, us_tmp, MODBUS_TIMEOUT_MS);//set charger voltage
			charger_config.set_voltage = m2s.charger.set_voltage;
			s2m.charger.set_voltage_req_ack = 1;
		}
		else if ((m2s.charger.set_current_req == 1) && (s2m.charger.set_current_req_ack == 0))
		{
			sprintf(logmsg, "GCS set charger set_charger_current = %d", m2s.charger.set_current);
			write_log(logmsg);
			us_tmp = (unsigned short)(m2s.charger.set_current * 10);
			ret = modbus_set_charger(0x11, us_tmp, MODBUS_TIMEOUT_MS);//set charger current
			charger_config.set_current = m2s.charger.set_current;
			s2m.charger.set_current_req_ack = 1;
		}
		else if ((m2s.charger.set_channel_req == 1) && (s2m.charger.set_channel_req_ack == 0))
		{
			sprintf(logmsg, "GCS set charger set_charger_channel = %d", m2s.charger.set_channel);
			write_log(logmsg);
			us_tmp = (unsigned short)(m2s.charger.set_channel);
			if((us_tmp==1)||(us_tmp==2))
			{
				ret = modbus_set_charger(0x13, us_tmp, MODBUS_TIMEOUT_MS);//set charger channel
				charger_config.set_channel = m2s.charger.set_channel;
			}
			else
			{
			}
			s2m.charger.set_channel_req_ack = 1;
		}
		else if ((m2s.charger.turn_on_req == 1) && (s2m.charger.turn_on_req_ack == 0))
		{
			sprintf(logmsg, "GCS set charger turn_on_req");
			write_log(logmsg);
			ret = modbus_set_charger(0x12, 0, MODBUS_TIMEOUT_MS);//set charger turn on
			s2m.charger.turn_on_req_ack = 1;
			gblState.charger_GCS_stop_charge = 0;
		}
		else if ((m2s.charger.turn_off_req == 1) && (s2m.charger.turn_off_req_ack == 0))
		{
			sprintf(logmsg, "GCS set charger turn_off_req");
			write_log(logmsg);
			ret = modbus_set_charger(0x12, 1, MODBUS_TIMEOUT_MS);//set charger turn off
			s2m.charger.turn_off_req_ack = 1;
			gblState.charger_GCS_stop_charge = 1;	

			ret = modbus_set_switcher(0x10, 0xaaaa, MODBUS_TIMEOUT_MS);//set all channel discharge enable
			sprintf(logmsg, "set switcher charging state to 0xaaaa");
			write_log(logmsg);		
		}
/*
		printf("SW: llim:%d,%d; hlim:%d,%d; ch0:%d,%d; ch1:%d,%d; en:%d,%d; dis:%d,%d\n",
			m2s.switcher.llim_req,s2m.switcher.llim_req_ack,
			m2s.switcher.hlim_req,s2m.switcher.hlim_req_ack,
			m2s.switcher.ch0_on_req,s2m.switcher.ch0_on_req_ack,
			m2s.switcher.ch1_on_req,s2m.switcher.ch1_on_req_ack,
			m2s.switcher.enable_req,s2m.switcher.enable_req_ack,
			m2s.switcher.disable_req,s2m.switcher.disable_req_ack);
		printf("SW: v_llim: %d, v_hlim: %d\n",m2s.switcher.voltage_llim,m2s.switcher.voltage_hlim);
		printf("CG: vol:%d,%d; cur:%d,%d; ch:%d,%d; on:%d,%d; off:%d,%d\n",
			m2s.charger.set_voltage_req,s2m.charger.set_voltage_req_ack,
			m2s.charger.set_current_req,s2m.charger.set_current_req_ack,
			m2s.charger.set_channel_req,s2m.charger.set_channel_req_ack,
			m2s.switcher.ch1_on_req,s2m.charger.turn_on_req_ack,
			m2s.charger.turn_on_req,s2m.charger.turn_off_req_ack);
		printf("CG: v: %d, c: %d, ch:%d\n",m2s.charger.set_voltage,m2s.charger.set_current,m2s.charger.set_channel);
		printf("GEN: %d,%d\n",s2m.generator_onoff_req,m2s.generator_state);
*/
		if(m2s.generator_state != generator_state_old)
		{
			sprintf(logmsg, "GCS set generator_state = %d",m2s.generator_state);
			write_log(logmsg);
			if(m2s.generator_state==0)
			{
				ret = modbus_set_switcher(0x10, 0xaaaa, MODBUS_TIMEOUT_MS);//set all channel discharge enable
				sprintf(logmsg, "generator_state = 0, set switcher charging state to 0xaaaa");
				write_log(logmsg);
			}
		}
		generator_state_old = m2s.generator_state;

		//polling modbus
		/*if(m2s.test_polling_ena)*/ modbus_polling();
	}
}

//��ѯ�л�����·��ص�ѹ���ŵ�״̬
int modbus_get_switcher_state(T_SWITCHER_DATA* data, int timeout_ms)
{
	unsigned char buf[20];
	unsigned short crc_tmp = 0;
	int i = 0,j=0;
	int _timeout_cnt;
	unsigned short us_tmp;

	//���͸�ʽ����վ��ַ(1B) | ���ܺ�(1B) | �Ĵ�����ʼ��ַ(2B) | ��ȡ�ļĴ�����(2B) | CRC
	//�˴��� SWITCHER_ADDR | 0x03 | 0x0, 0x0 | 0x0,0x4 | CRCH,CRCL
	buf[0] = SWITCHER_ADDR;//��վ��ַ
	buf[1] = 0x03;//���ܺ�
	buf[2] = 0x0;
	buf[3] = 0x0;//[2][3]:��ʼ�Ĵ�����ַ
	buf[4] = 0x0;
	buf[5] = 0x4;//[4][5]:��ȡ�ļĴ�������ÿ���Ĵ���2���ֽڣ�����˴˴�Ϊ��ȡ8���ֽ�
	crc_tmp = crc16(buf, 6);
	buf[6] = (crc_tmp >> 8) & 0xff;
	buf[7] = crc_tmp & 0xff;
/*
	_DBG_0("modbus_get_switcher_state >>>: ");
	for (i = 0; i < 8; i++) _DBG_1("%x ", buf[i]);
	_DBG_0("\n");
*/
	write(fd_modbus, buf, 8);

	//Ԥ�ڷ��أ���վ��ַ(1B) | ���ܺ�(1B) | �������ݵ��ֽ���(1B) | 8�ֽڵ����� | CRCH,CRCL ������CRCһ��11���ֽ�
	//�˴���SWITCHER_ADDR | 0x03 | 0x08 | 4*2�ֽڵ�����,ÿ���Ĵ�����λ��ǰ | CRCH,CRCL
	_modbus.recv_addr = SWITCHER_ADDR;
	_modbus.recv_flag = 0x03;
	_modbus.recv_len = 11;//����11���ֽڣ�����CRC��
	_modbus.recv_cnt = 0;
	_modbus.recv_ok = 0;

	_timeout_cnt = (int)(0.1 * timeout_ms);//������ת��Ϊsleep����
	for (i = 0; i < _timeout_cnt; i++)
	{
		usleep(1000*10);//[10ms]
		if (_modbus.recv_ok == 1) 
		{
			us_tmp = (_modbus.buf[3]<<8)|(_modbus.buf[4]);
			data->voltage[0] = (unsigned char)((int)(0.1 * us_tmp + 0.5));
			us_tmp = (_modbus.buf[5]<<8)|(_modbus.buf[6]);
			data->workstate[0] = (unsigned char)(us_tmp);
			us_tmp = (_modbus.buf[7]<<8)|(_modbus.buf[8]);
			data->voltage[1] = (unsigned char)((int)(0.1 * us_tmp + 0.5));
			us_tmp = (_modbus.buf[9]<<8)|(_modbus.buf[10]);
			data->workstate[1] = (unsigned char)(us_tmp);

			//_DBG_0("modbus_get_switcher_state <<<: ");
			//for (j = 0; j < _modbus.recv_len; j++) _DBG_1("%x ", _modbus.buf[j]);
			//_DBG_0("\n");
			break;
		}
	}
	if (_modbus.recv_ok == 0)
	{
		_modbus.switcher_error_cnt++;
	}
	return _modbus.recv_ok;
}

int modbus_set_switcher(unsigned short addr, unsigned short value, int timeout_ms)
{
	unsigned char buf[20];
	unsigned short crc_tmp = 0;
	int i = 0;
	int _timeout_cnt;
	
	//���͸�ʽ����վ��ַ(1B) | ���ܺ�(1B) | �Ĵ�����ʼ��ַ(2B) | �Ĵ���������ֵ(2B) | CRC
	//�˴��� SWITCHER_ADDR | 0x06 | addrH,addrL | valueH,valueL | CRCH,CRCL
	buf[0] = SWITCHER_ADDR;
	buf[1] = 0x06;//��������
	buf[2] = 0x0;
	buf[3] = (unsigned char)addr;//�л����Ĵ�����ַ
	buf[4] = (unsigned char)((value>>8)&0xff);
	buf[5] = (unsigned char)(value & 0xff);
	crc_tmp = crc16(buf, 6);
	buf[6] = (crc_tmp >> 8) & 0xff;
	buf[7] = crc_tmp & 0xff;
	write(fd_modbus, buf, 8);
/*
	_DBG_1("modbus_set_switcher, value(%x): ", value);
	for (i = 0; i < 8; i++) _DBG_1("%x ",buf[i]);
	_DBG_0("\n");
*/
	//Ԥ�ڷ����뷢��������ͬ����վ��ַ(1B) | ���ܺ�(1B) | �Ĵ�����ʼ��ַ(2B) | �Ĵ���������ֵ(2B) | CRC
	_modbus.recv_addr = SWITCHER_ADDR;
	_modbus.recv_flag = 0x06;
	_modbus.recv_len = 6;//����6���ֽڣ�����CRC��
	_modbus.recv_cnt = 0;
	_modbus.recv_ok = 0;
	
	_timeout_cnt = (int)(0.1 * timeout_ms);
	for (i = 0; i < _timeout_cnt; i++)
	{
		usleep(1000 * 10);//10ms
		if (_modbus.recv_ok == 1) break;
	}
	return _modbus.recv_ok;
}

int modbus_get_charger_state(T_CHARGER_DATA* data, int timeout_ms)
{
	unsigned char buf[20];
	unsigned short crc_tmp = 0;
	int i = 0;
	int j=0;
	int _timeout_cnt;
	unsigned short us_tmp;

	//���͸�ʽ����վ��ַ(1B) | ���ܺ�(1B) | �Ĵ�����ʼ��ַ(2B) | ��ȡ�ļĴ�����(2B) | CRC
	//�˴��� CHARGER_ADDR | 0x03 | 0x0, 0x0 | 0x0,0x4 | CRCH,CRCL
	buf[0] = CHARGER_ADDR;
	buf[1] = 0x03;
	buf[2] = 0x0;
	buf[3] = 0x0;
	buf[4] = 0x0;
	buf[5] = 0x4;
	crc_tmp = crc16(buf, 6);
	buf[6] = (crc_tmp >> 8) & 0xff;
	buf[7] = crc_tmp & 0xff;

	write(fd_modbus, buf, 8);
/*
	_DBG_0("modbus_get_charger_state >>>: ");
	for (i = 0; i < 8; i++) _DBG_1("%x ",buf[i]);
	_DBG_0("\n");
*/
	//Ԥ�ڷ��أ���վ��ַ(1B) | ���ܺ�(1B) | �������ݵ��ֽ���(1B) | 8�ֽڵ����� | CRCH,CRCL ������CRCһ��11���ֽ�
	//�˴���CHARGER_ADDR | 0x03 | 0x08 | 4*2�ֽڵ�����,ÿ���Ĵ�����λ��ǰ | CRCH,CRCL
	_modbus.recv_addr = CHARGER_ADDR;
	_modbus.recv_flag = 0x03;
	_modbus.recv_len = 11;//����11���ֽڣ�����CRC��
	_modbus.recv_cnt = 0;
	_modbus.recv_ok = 0;
	
	_timeout_cnt = (int)(0.1 * timeout_ms);
	_modbus.charger_error_cnt = 0;
	for (i = 0; i < _timeout_cnt; i++)
	{
		usleep(1000 * 10);//10ms
		if (_modbus.recv_ok == 1)
		{
			us_tmp = (_modbus.buf[9] << 8) | (_modbus.buf[10]);
			data->work_chan = (unsigned char)(us_tmp & 0xff);
			
			if ((data->work_chan == 1)||(data->work_chan==2))
			{
				us_tmp = (_modbus.buf[3] << 8) | (_modbus.buf[4]);
				data->voltage[data->work_chan-1] = (unsigned char)((int)(0.1 * us_tmp + 0.5));
				us_tmp = (_modbus.buf[5] << 8) | (_modbus.buf[6]);
				data->current[data->work_chan-1] = (unsigned char)((int)(0.1 * us_tmp + 0.5));
				us_tmp = (_modbus.buf[7] << 8) | (_modbus.buf[8]);
				data->workstate[data->work_chan-1] = (unsigned char)(us_tmp&0xff);
			}
/*
			_DBG_0("modbus_get_charger_state <<<: ");
			for (j = 0; j < _modbus.recv_len; j++) _DBG_1("%x ", _modbus.buf[j]);
			_DBG_0("\n");
*/
			break;
		}
	}
	if (_modbus.recv_ok == 0)
	{
		_modbus.charger_error_cnt++;
	}
	return _modbus.recv_ok;
}

int modbus_set_charger(unsigned short addr, unsigned short value, int timeout_ms)
{
	unsigned char buf[20];
	unsigned short crc_tmp = 0;
	int i = 0;
	int _timeout_cnt;

	//���͸�ʽ����վ��ַ(1B) | ���ܺ�(1B) | �Ĵ�����ʼ��ַ(2B) | �Ĵ���������ֵ(2B) | CRC
	//�˴��� CHARGER_ADDR | 0x06 | addrH,addrL | valueH,valueL | CRCH,CRCL
	buf[0] = CHARGER_ADDR;
	buf[1] = 0x06;
	buf[2] = 0x0;
	buf[3] = (unsigned char)addr;//�л����Ĵ�����ַ
	buf[4] = (unsigned char)((value >> 8) & 0xff);
	buf[5] = (unsigned char)(value & 0xff);
	crc_tmp = crc16(buf, 6);
	buf[6] = (crc_tmp >> 8) & 0xff;
	buf[7] = crc_tmp & 0xff;

	write(fd_modbus, buf, 8);
/*
	_DBG_1("modbus_set_charger, value(%x): ", value);
	for (i = 0; i < 8; i++) _DBG_1("%x ",buf[i]);
	_DBG_0("\n");
*/
	//Ԥ�ڷ����뷢��������ͬ����վ��ַ(1B) | ���ܺ�(1B) | �Ĵ�����ʼ��ַ(2B) | �Ĵ���������ֵ(2B) | CRC
	_modbus.recv_addr = CHARGER_ADDR;
	_modbus.recv_flag = 0x06;
	_modbus.recv_len = 6;//����6���ֽڣ�����CRC��
	_modbus.recv_cnt = 0;
	_modbus.recv_ok = 0;
	
	_timeout_cnt = (int)(0.1 * timeout_ms);
	for (i = 0; i < _timeout_cnt; i++)
	{
		usleep(1000 * 10);//10ms
		if (_modbus.recv_ok == 1) break;
	}
	
	return _modbus.recv_ok;
}

void modbus_polling()
{
	int ret = 0;
	int i = 0;
	static unsigned short generator_state_old = 0;
	static unsigned char work_chan_old;
	static int charger_quary_enable_cnt=0;
	static int charger_sm_old=0;
	unsigned short us_tmp;

	unsigned char charger_ch01_tmp=0;

	////SWITCHER////
	ret = modbus_get_switcher_state(&switcher_state_tmp, MODBUS_TIMEOUT_MS);//get switcher state
	if (ret > 0)//return ok
	{
		memcpy(&switcher_state, &switcher_state_tmp, sizeof(switcher_state));
		sprintf(logmsg, "SW state: %d,0x%x,%d,0x%x", switcher_state.voltage[0], switcher_state.workstate[0],
			switcher_state.voltage[1], switcher_state.workstate[1]);
		write_log(logmsg);

		//GENERATOR
		//Calc when turn on/off the generator
		//if any un-discharge channel's voltage is below REQ_CHARGE_VOLTAGE,the GEN show work.
		if(((switcher_state.workstate[0] == SW_ST_STOP) && (switcher_state.voltage[0] < REQ_CHARGE_VOLTAGE))
			||((switcher_state.workstate[1] == SW_ST_STOP) && (switcher_state.voltage[1] < REQ_CHARGE_VOLTAGE)))
		{
			if(m2s.generator_state == 0) s2m.generator_onoff_req = 1;
		}
		//if any un-discharge channel is not charging and the voltage is bigger than BATTERY_FULL_VOLTAGE, then GEN done.
/*		
		else
		{
			if(charger_state.work_chan == 0)
			{
				if((switcher_state.workstate[0] == SW_ST_DISCHARGING)
					&&(switcher_state.voltage[1] > (BATTERY_FULL_VOLTAGE-1)))
				{
					s2m.generator_onoff_req = 0;
				}
				else if((switcher_state.workstate[1] == SW_ST_DISCHARGING)
					&&(switcher_state.voltage[0] > (BATTERY_FULL_VOLTAGE-1)))
				{
					s2m.generator_onoff_req = 0;
				}
				else if((switcher_state.voltage[0] > (BATTERY_FULL_VOLTAGE-1))
					&&(switcher_state.voltage[1] > (BATTERY_FULL_VOLTAGE-1)))
				{
					s2m.generator_onoff_req = 0;
				}
			}
		}
*/
		//Calc swicther state
		if (gblState.switcher_enable==1)//switcher is enable. Note: this bit is always enable...fei201701
		{
			if(charger_state.work_chan != work_chan_old)
			{
				if(charger_state.work_chan == 0)
				{
					ret = modbus_set_switcher(0x10, 0xaaaa, MODBUS_TIMEOUT_MS);//set all channel discharge enable
					sprintf(logmsg, "set switcher charging state to 0xaaaa");
					write_log(logmsg);
				}
				else if(charger_state.work_chan == 1)
				{
					ret = modbus_set_switcher(0x10, 0xaa55, MODBUS_TIMEOUT_MS);//chan0 disable, chan1 enable
					sprintf(logmsg, "set switcher charging state to 0xaa55");
					write_log(logmsg);
				}
				else if(charger_state.work_chan == 2)
				{
					ret = modbus_set_switcher(0x10, 0x55aa, MODBUS_TIMEOUT_MS);//chan1 disable, chan0 enable
					sprintf(logmsg, "set switcher charging state to 0x55aa");
					write_log(logmsg);
				}
				if(ret>0)
				{
					work_chan_old = charger_state.work_chan;
				}
				else
				{
					sprintf(logmsg, "set switcher the charger state error!");
					write_log(logmsg);
				}
			}
		}
	}

	////CHARGER////
	//first: quary the charger state
	charger_quary_enable_cnt--;
	if(charger_quary_enable_cnt<=0)
	{
		ret = modbus_get_charger_state(&charger_state_tmp, MODBUS_TIMEOUT_MS);
		if (ret > 0)
		{
			//display data
			sprintf(logmsg, "CG state: chan:%d, V:%d,%d,A:%d,%d,state:0x%x,0x%x",
				charger_state_tmp.work_chan, charger_state_tmp.voltage[0], charger_state_tmp.voltage[1],
				charger_state_tmp.current[0], charger_state_tmp.current[1], charger_state_tmp.workstate[0],
				charger_state_tmp.workstate[1]);
			write_log(logmsg);

			if(charger_state_tmp.work_chan>0)
			{
				//charger_state.work_chan = charger_state_tmp.work_chan;
				gblState.charge_time++;//calc the charging time.
			}
			if ((charger_state_tmp.work_chan == 1) || (charger_state_tmp.work_chan == 2))
			{
				charger_state.work_chan = charger_state_tmp.work_chan;
				charger_ch01_tmp = charger_state.work_chan - 1;//charger_ch01_tmp: ch01 denote chan is 0 or 1
				charger_state.voltage[charger_ch01_tmp] = charger_state_tmp.voltage[charger_ch01_tmp];
				charger_state.current[charger_ch01_tmp] = charger_state_tmp.current[charger_ch01_tmp];
				charger_state.workstate[charger_ch01_tmp] = charger_state_tmp.workstate[charger_ch01_tmp];
			}
			else
			{
				charger_state.work_chan = charger_state_tmp.work_chan;
			}
			charger_quary_enable_cnt = 1;
			gblState.charger_quary_ok = 1;
		}
		else
		{
			sprintf(logmsg, "modbus_get_charger_state error!!!!!!!!!");
			write_log(logmsg);
			charger_quary_enable_cnt = 5;
			gblState.charger_quary_ok = 0;

			if (m2s.generator_state == 0) //generator doesn't work, so we do not need to run charger
			{
				charger_sm = SM_CHARGER_OFF;
				charger_state.work_chan = 0;
				charger_config.work_chan = 0;
			}
			charger_sm = SM_CHARGER_OFF;
		}
	}

	//for test
/*
	if (m2s.generator_state == 0) //generator doesn't work, so we do not need to run charger
	{
		charger_sm = SM_CHARGER_OFF;
		charger_state.work_chan = 0;
		charger_config.work_chan = 0;
	}	
*/
	if(gblState.charger_GCS_stop_charge ==1) charger_sm = SM_CHARGER_DUMMY;
	switch (charger_sm)
	{
	case SM_CHARGER_OFF:
		//for test if (m2s.generator_state == 1)//generator is working
		{
			if(gblState.charger_enable ==1)
			{
				charger_sm = SM_CHARGER_TURNOFF;
				charger_config.work_chan = 1;
			}
		}
		break;
	case SM_CHARGER_TURNOFF://1
		ret = modbus_set_charger(0x12, 1, MODBUS_TIMEOUT_MS);//turn off the charger
		if (ret > 0) charger_sm = SM_CHARGER_SET_CHANNEL;
		else 
		{
			sprintf(logmsg, "Error: In SM_CHARGER_TURNOFF: modbus_set_charger(0x12)");
			write_log(logmsg);
		}
		break;
	case SM_CHARGER_SET_CHANNEL://2
		ret = 0;
		if(charger_config.set_channel == 1)//receive from VGS
		{
			if (switcher_state.workstate[0] == SW_ST_STOP)
			{
				_DBG_0("[1]\n");
				charger_config.work_chan = 1;
			}
			else//if chan0 is not STOP, then chan1 must be STOP
			{
				_DBG_0("[2]\n");
				charger_config.work_chan = 2;
			}
			charger_config.set_channel = 0;
		}
		else if(charger_config.set_channel == 2)//receive from VGS
		{
			if (switcher_state.workstate[1] == SW_ST_STOP)
			{
				_DBG_0("[3]\n");
				charger_config.work_chan = 2;
			}
			else
			{
				_DBG_0("[4]\n");
				charger_config.work_chan = 1;
			}
			charger_config.set_channel = 0;
		}
		else if(charger_config.work_chan == 1)
		{
			if((switcher_state.workstate[0] == SW_ST_STOP)&&(switcher_state.voltage[0] < REQ_CHARGE_VOLTAGE))
			{
				_DBG_0("[5]\n");
				charger_config.work_chan = 1;
			}
			else if((switcher_state.workstate[1] == SW_ST_STOP)&&(switcher_state.voltage[1] < REQ_CHARGE_VOLTAGE))
			{
				_DBG_0("[6]\n");
				charger_config.work_chan = 2;
			}
			else
			{
				_DBG_0("[7]\n");
				charger_config.work_chan = 0;
			}
		}
		else if(charger_config.work_chan == 2)
		{
			if((switcher_state.workstate[1] == SW_ST_STOP)&&(switcher_state.voltage[1] < REQ_CHARGE_VOLTAGE))
			{
				_DBG_0("[8]\n");
				charger_config.work_chan = 2;
			}
			else if((switcher_state.workstate[0] == SW_ST_STOP)&&(switcher_state.voltage[0] < REQ_CHARGE_VOLTAGE))
			{
				_DBG_0("[9]\n");
				charger_config.work_chan = 1;
			}
			else
			{
				_DBG_0("[10]\n");
				charger_config.work_chan = 0;
			}
		}
		if((charger_config.work_chan==1)||(charger_config.work_chan==2))
		{
			//select work channel
			ret = modbus_set_charger(0x13, charger_config.work_chan, MODBUS_TIMEOUT_MS);
			if (ret > 0)
			{
				charger_sm = SM_CHARGER_SET_VOLTAGE;
				sprintf(logmsg, "CG set channel to %d",charger_config.work_chan);
				write_log(logmsg);				
			}
			else
			{
				sprintf(logmsg, "Error: In SM_CHARGER_SET_CHANNEL");
				write_log(logmsg);
			}
		}

		break;
	case SM_CHARGER_SET_VOLTAGE:
		if((charger_config.set_voltage >= CHARGER_VOLTAGE_MIN)&&(charger_config.set_voltage <= CHARGER_VOLTAGE_MAX))
		{
			us_tmp = charger_config.set_voltage * 10;
		}	
		else
		{	
			us_tmp = CHARGER_VOLTAGE_DFT * 10;
		}
		ret = modbus_set_charger(0x10, us_tmp, MODBUS_TIMEOUT_MS);//set charge voltage: write voltage to 0x10,[0.1V]
		if (ret > 0)
		{
			charger_sm = SM_CHARGER_SET_CURRENT;
			sprintf(logmsg, "CG set voltage to %d",us_tmp/10);
			write_log(logmsg);				
		}
		else
		{
			sprintf(logmsg, "Error: In SM_CHARGER_SET_VOLTAGE");
			write_log(logmsg);
		}
		break;
	case SM_CHARGER_SET_CURRENT:
		if((charger_config.set_current >= CHARGER_CURRENT_MIN)&&(charger_config.set_current <= CHARGER_CURRENT_MAX))
		{
			us_tmp = charger_config.set_current * 10;
		}	
		else
		{	
			us_tmp = CHARGER_CURRENT_DFT * 10;
		}
		ret = modbus_set_charger(0x11, us_tmp, MODBUS_TIMEOUT_MS);//���õ���:���ַ0x10д��������λ[0.1A]
		if (ret > 0)
		{
			charger_sm = SM_CHARGER_TURNON;
			sprintf(logmsg, "CG set current to %d",us_tmp/10);
			write_log(logmsg);				
		}
		else
		{
			sprintf(logmsg, "Error: In SM_CHARGER_SET_CURRENT");
			write_log(logmsg);
		}
		break;
	case SM_CHARGER_TURNON:
		ret = modbus_set_charger(0x12, 0, MODBUS_TIMEOUT_MS);//����
		if (ret > 0)
		{
			charger_sm = SM_CHARGER_QUARY;
			gblState.charge_time=0;//[2second]
		}
		break;
	case SM_CHARGER_QUARY://����е�״̬��ѯ
		if(gblState.charger_quary_ok==1)
		{
			if(charger_state.work_chan>0) gblState.charge_time++;//calc the charging time.

			if ((charger_state.work_chan == 1) || (charger_state.work_chan == 2))
			{
				charger_ch01_tmp = charger_state.work_chan - 1;//charger_ch01_tmp: ch01 denote chan is 0 or 1
				if((charger_state.voltage[charger_ch01_tmp]>(BATTERY_FULL_VOLTAGE-5))
					&&(charger_state.current[charger_ch01_tmp]<BATTERY_FULL_CURRENT))
				{
					ret = modbus_set_charger(0x12, 1, MODBUS_TIMEOUT_MS);//turn off the charger
					if (ret > 0)
					{
						charger_sm = SM_CHARGER_WAIT;
					}
					else 
					{
						sprintf(logmsg, "Error: In SM_CHARGER_QUARY: modbus_set_charger(0x12)");
						write_log(logmsg);
					}
				}
				if(gblState.charge_time >= 30*60*10)//charge over 10 hours
				{
					ret = modbus_set_charger(0x12, 1, MODBUS_TIMEOUT_MS);//turn off the charger
					if (ret > 0)
					{
						charger_sm = SM_CHARGER_WAIT;
					}
					else 
					{
						sprintf(logmsg, "Error: In SM_CHARGER_QUARY: modbus_set_charger(0x12)");
						write_log(logmsg);
					}
				}
			}
			else if(charger_state.work_chan == 0)
			{
				charger_sm = SM_CHARGER_WAIT;
			}
			else
			{
				charger_sm = SM_CHARGER_TURNOFF;
			}
		}
		else
		{
		}
		break;
	case SM_CHARGER_WAIT:
		if(charger_state.work_chan == 0)
		{
			if(charger_config.work_chan == 1) charger_config.work_chan = 2;
			else if(charger_config.work_chan == 2) charger_config.work_chan = 1;	
			charger_sm = SM_CHARGER_SET_CHANNEL;
		}
		break;
	case SM_CHARGER_DUMMY: //do nothing
		if(gblState.charger_GCS_stop_charge == 0)
		{
			charger_sm = SM_CHARGER_TURNOFF;
		}
		break;

	default:;
	}

	if(charger_sm_old != charger_sm)
	{
		//_DBG_2("Charger SM: %d-->%d\n", charger_sm_old, charger_sm);
		sprintf(logmsg, "Charger SM: %d-->%d", charger_sm_old, charger_sm);
		write_log(logmsg);
	}
	charger_sm_old = charger_sm;
}

	
