#ifndef _UDP_
#define _UDP_

#include<stdio.h>  
#include<sys/types.h>  
#include<sys/stat.h>  
#include<sys/select.h>  
#include<sys/ioctl.h> 
#include<sys/socket.h> 
#include<netinet/in.h> 
#include<arpa/inet.h> 
#include<fcntl.h>  
#include<unistd.h>  
#include<signal.h>  
#include<pthread.h> 
#include<unistd.h> 
#include<stdio.h> 
#include<stdlib.h> 
#include<errno.h> 
#include<netdb.h> 
#include<stdarg.h> 
#include<string.h> 


#define UDP_PACKET_HEAD 0xa55a5aa5

extern unsigned char udp_recvbuf[2000];
void udp_init(char* ip_sendto, int port_sendto, int port_myrecv);
int udp_send(int len, void *data);
int udp_recv();

#endif //_UDP_

