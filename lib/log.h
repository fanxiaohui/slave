#ifndef _LOG_H_
#define _LOG_H_

#define LOGROW_LEN 300

extern struct tm *gbl_time_val;//全局时间变量

int create_logfile();
int write_log(char *msg);
int close_logfile();

#endif //_LOG_H_
