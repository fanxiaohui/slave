//////////////////////////////////////////////////////////////////////////
// crc_check.h - CRC校验值计算函数,参照资料数据
//
// Author:  Fei Qing
// Created: Feb 11, 2016
//
// Modification History
// --------------------
//////////////////////////////////////////////////////////////////////////
#ifndef _CRC_H_
#define _CRC_H_

unsigned short crc16(unsigned char *ptr,unsigned int len);
unsigned short crc16_2(unsigned char *ptr,unsigned int len);
unsigned short crc16_3(unsigned char *ptr,unsigned int len);

#endif
