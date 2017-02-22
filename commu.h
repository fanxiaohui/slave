#ifndef HEADERS_COMMU_H_
#define HEADERS_COMMU_H_

#define IP_MASTER "10.10.80.1"//给副控发送数据
#define IP_SLAVER "10.10.80.2"
//#define IP_MASTER "192.168.0.101"//给副控发送数据
//#define IP_SLAVER "192.168.0.2"
#define UDP_M_RECV_PORT 7000
#define UDP_S_RECV_PORT 7500
#define UDP_PACKET_HEAD 0xa55a5aa5

#define M2S_HEAD 1
#define S2M_HEAD 2

//M->S切换器的设置命令
typedef struct tag_M2S_Switcher
{
	unsigned char enable_req :1;//=1切换器允许输出请求，收到响应(s2m.enable_req_ack=1)后清零
	unsigned char disable_req :1;//=1切换器禁止输出，收到反馈后清零
	unsigned char ch0_on_req:1;//=1切换器强制通道0，收到反馈后清零
	unsigned char ch1_on_req :1;//=1切换器强制通道1，收到反馈后清零
	unsigned char llim_req :1;//=1切换器电压下限设置请求，收到反馈后清零
	unsigned char hlim_req :1;//=1切换器电压上限设置请求，收到反馈后清零
	unsigned char spare :2;
	unsigned char voltage_llim;//切换器切换电压下限
	unsigned char voltage_hlim;//切换器切换电压上限
}T_M2S_SWITCHER;

//S->M切换器返回的数据
typedef struct tag_S2M_Switcher
{
	//unsigned char ena;//切换器允许输出反馈
	unsigned char enable_req_ack :1;//切换器允许输出请求的响应，在enable_req=0时为0，在enable_req=1时为1
	unsigned char disable_req_ack :1;//切换器禁止输出请求的响应，在disable_req=0时为0，在disable_req=1时为1
	unsigned char ch0_on_req_ack :1;//通道0设置请求的响应
	unsigned char ch1_on_req_ack :1;//通道1设置请求的响应
	unsigned char llim_req_ack :1;//电压下限设置请求的响应
	unsigned char hlim_req_ack :1;//电压上限设置请求的响应
	unsigned char spare :2;
	unsigned short voltage_llim;//切换器切换电压下限
	unsigned short voltage_hlim;//切换器切换电压上限
	unsigned char workstate[2];//通道0和1的状态：0x55,0x5a,0xaa
	unsigned char voltage[2];//电池电压
}T_S2M_SWITCHER;

/*
typedef struct tag_M2S_Charger
{
unsigned char turn_onoff_req;//充电机开机/关机请求，1:开机;0:关机
unsigned char turn_onoff_req_cnt;//副控检测到该计数值发生变化时根据turn_on_off_req状态命令充电机开机/关机
unsigned char set_voltage_cnt;//设置电压计数
unsigned char set_current_cnt;//设置电流计数
unsigned char set_channel_cnt;//设置通道计数
unsigned char set_voltage;//要设置的电压，在set_voltage_cnt变化时进行设置
unsigned char set_current;//要设置的电流，在set_current_cnt变化时进行设置
unsigned char set_channel;//设置的通道号,0;1;2一般只用0和1.在set_channel_cnt变化时进行设置
}T_M2S_CHARGER;
*/
typedef struct tag_M2S_Charger
{
	unsigned char turn_on_req;//充电机开机请求
	unsigned char turn_off_req;//充电机关机请求
	unsigned char set_voltage_req;//设置电压请求
	unsigned char set_current_req;//设置电流请求
	unsigned char set_channel_req;//设置通道请求
	unsigned char set_voltage;//要设置的电压，在set_voltage_cnt变化时进行设置
	unsigned char set_current;//要设置的电流，在set_current_cnt变化时进行设置
	unsigned char set_channel;//设置的通道号,0;1;2一般只用0和1.在set_channel_cnt变化时进行设置
}T_M2S_CHARGER;

typedef struct tag_S2M_Charger
{
	unsigned char turn_on_req_ack;//充电机开机请求响应
	unsigned char turn_off_req_ack;//充电机关机请求响应
	unsigned char set_voltage_req_ack;//设置电压请求响应
	unsigned char set_current_req_ack;//设置电流请求响应
	unsigned char set_channel_req_ack;//设置通道请求响应
	unsigned char channel;//当前充电通道号
	unsigned char voltage[2];//两个通道各自的电压
	unsigned char current[2];//两个通道各自的电流
	unsigned char work_state;//工作状态
}T_S2M_CHARGER;

//气象站返回探测数据
typedef struct tag_S2M_Aws
{
	unsigned char temp;//温度
	unsigned char humi;//湿度
	unsigned char windspeed;
	unsigned char winddir;
	unsigned char airpress;
	unsigned char seasault;
	unsigned char elec_cond;
	unsigned char seatemp1;
}T_S2M_AWS;

typedef struct tag_M2S_Rkt
{
	unsigned char launch_req;//请求火箭发射
}T_M2S_RKT;

typedef struct tag_S2M_Rkt
{
	unsigned char launch_req_ack;//请求火箭发射计数返回
	unsigned char state;//火箭系统状态
	unsigned char rktnumber;//火箭编号
	short alt;//火箭飞行高度
}T_S2M_RKT;

typedef struct tagT_DATA_M2S
{
	unsigned short head;
	unsigned short heart_beat;

	unsigned short test_polling_ena :1;
	unsigned short test :15;

	unsigned char generator_state;//发电机状态,=1:启动;=0:停止

	//切换器数据
	T_M2S_SWITCHER switcher;

	//充电机数据
	T_M2S_CHARGER charger;

	//火箭
	T_M2S_RKT rkt;

	//给火箭的位置和姿态数据
	int lon;		//[10-6 degree] 经度
	int lat;		//[10-6 degree] 维度
	short alt;		//[0.1 m] 高度
	short pitch;	//[0.01 degree] 俯仰
	short roll;		//[0.01 degree] 滚转
	short yaw;		//[0.01 degree] 偏航
	
}T_M2S;//128bytes


typedef struct tagT_DATA_S2M
{
	unsigned short head;
	unsigned short heart_beat;

	//发电机控制		
	unsigned char generator_onoff_req;//发电机请求开启/关闭.当需要充电但发电机generator_state=0时，向主控发出请求信号

	//切换器数据
	T_S2M_SWITCHER switcher;

	//充电机数据
	T_S2M_CHARGER charger;

	//气象站
	T_S2M_AWS aws;

	//火箭
	T_S2M_RKT rkt;

}T_S2M;//total:128bytes


#endif /* HEADERS_COMMU_H_ */
