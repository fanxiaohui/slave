#ifndef _AWS_H_
#define _AWS_H_

#include "../lib/common.h"

#define DEV_AWS_RS232 "/dev/ttyO4"

typedef struct tagAWSData
{
	unsigned char head;
	char _date[8];
	char _time[6];
	char eastwest;
	char lon[10];
	char northsouth;
	char lat[9];
	unsigned short alt;
	unsigned short temp;
	unsigned short humi;
	unsigned short windspeed;
	unsigned short winddir;
	unsigned short airpress;
	unsigned short radiation1;
	unsigned short radiation2;
	unsigned int seasault;
	unsigned int elec_cond;
	unsigned short seatemp1;
	unsigned short seatemp2;
	unsigned short seatemp3;
	unsigned short seatemp4;
	unsigned short seatemp5;
}T_AWS_DATA;

typedef struct tagAwsRec
{
	T_DATETIME _datatime;//本条记录时间
	T_AWS_DATA data;
}T_AWS_REC;

extern T_AWS_DATA aws_data;
extern T_AWS_REC aws_rec;

int aws_init();

#endif//_AWS_H_
