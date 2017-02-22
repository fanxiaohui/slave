#ifndef _UART_H_
#define _UART_H_

void set_speed(int fd, int speed);
int set_parameter(int fd,int databits,int stopbits,int parity);

#endif //_UART_H_
