#ifndef _COMMON_H_
#define _COMMON_H_

typedef struct tagDateTime
{
	unsigned short year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned char stuffing;//����ֽڣ���֤���ݰ��ֽ���Ϊ4��������
}T_DATETIME;

#define _DBG_0(x0) printf(x0);
#define _DBG_1(x0,x1) printf(x0,x1);
#define _DBG_2(x0,x1,x2) printf(x0,x1,x2);


#endif//_COMMON_H_
