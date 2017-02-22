#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <time.h>
#include <sys/time.h>
#include "log.h"

#define LOGROWTIME_LEN 10
#define _LOG_ON_SCREEN(x) printf(x);
char _slog_row[LOGROW_LEN+LOGROWTIME_LEN];

int fd_logfile=0;

int create_logfile()
{
	int len=0;
	int write_len;
	char filename[200];

	sprintf(filename, "LOG%02d%02d_%02d%02d%02d.txt", gbl_time_val->tm_mon + 1,
		gbl_time_val->tm_mday, gbl_time_val->tm_hour, gbl_time_val->tm_min, gbl_time_val->tm_sec);
		
	if((fd_logfile=open(filename,O_RDWR |O_CREAT |O_APPEND))==-1)
	{
		printf("Error:Can not open %s",filename);
		exit(1);
	}
	sprintf(_slog_row,"%d:%d:%d> create log file %s\t\n",
		gbl_time_val->tm_hour,gbl_time_val->tm_min,gbl_time_val->tm_sec,filename);
	len = strlen(_slog_row);
	if(fd_logfile)
	{
		write_len=write(fd_logfile,_slog_row,len);
	}		
	return 0;
}

int write_log(char *string)
{
	int len=0;
	int write_len;
	
	sprintf(_slog_row, "%2d:%2d:%2d %s\t\n", 
		gbl_time_val->tm_hour, gbl_time_val->tm_min, gbl_time_val->tm_sec, string);
	len = strlen(_slog_row);
	_LOG_ON_SCREEN(/*string*/_slog_row);
	//_LOG_ON_SCREEN("\n");

	if(fd_logfile)
	{
		write_len=write(fd_logfile,_slog_row,len);
	}
	return len;
}

int close_logfile()
{
	if(fd_logfile) close(fd_logfile);
	return 0;
}
