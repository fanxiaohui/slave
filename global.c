#include "global.h"

T_SYS_CONFIG sys_config;//系统配置数据

T_DATETIME datetime_now;

struct tm *gbl_time_val;//全局时间变量，其它的时间都从这里取

T_GBL_DATA gblState;
T_M2S m2s;
T_S2M s2m;


char logmsg[300];//日志消息缓冲区
