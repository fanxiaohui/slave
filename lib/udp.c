#include <semaphore.h>
#include "udp.h"

int sockFd_send;//发送套接字
int sockFd_recv;//接收套接字

struct sockaddr_in	udpMyAddr;//发起UDP通信的端口，即发送UDP包的端口
struct sockaddr_in	udpSendtoAddr;//接收方的端口号。发送时为对方端口，接收时为本机接收端口
char*	m_pszAddress;
int m_sockAddrSize;
struct sockaddr_in client_addr;
int client_addr_len;



unsigned char m_sendbuf[2000];
unsigned char udp_recvbuf[2000];


void udp_init(char* ip_sendto, int port_sendto, int port_myrecv)
{
	/***UDP发送套接字***/

	//设置发送地址
	m_sockAddrSize = sizeof(struct sockaddr_in);
	bzero((char*)&udpSendtoAddr, m_sockAddrSize);
	udpSendtoAddr.sin_family = AF_INET;
	udpSendtoAddr.sin_addr.s_addr = inet_addr(ip_sendto);
	udpSendtoAddr.sin_port = htons(port_sendto);
	//创建UDP发送套接字
	sockFd_send = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockFd_send == -1)
	{
		perror("udp send create socket failed");
		close(sockFd_send);
		return;
	}
	printf("udp send ini ok!\n");

	/***UDP接收套接字***/

	//接收地址
	m_sockAddrSize = sizeof(struct sockaddr_in);
	bzero((char*)&udpMyAddr, m_sockAddrSize);
	udpMyAddr.sin_family = AF_INET;
	udpMyAddr.sin_addr.s_addr = INADDR_ANY;
	udpMyAddr.sin_port = htons(port_myrecv);
	client_addr_len = sizeof(client_addr);
	//创建UDP接收套接字
	sockFd_recv = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockFd_recv == -1)
	{
		perror("udp recv create socket failed");
		close(sockFd_recv);
		return;
	}
	//绑定套接字
	if (bind(sockFd_recv, (struct sockaddr *)&udpMyAddr, sizeof(struct sockaddr_in)) == -1)
	{
		perror("udp recv bind err");
		close(sockFd_recv);
		return;
	}
	printf("udp recv ini ok!\n");
}
//发送数据包格式
// 4B包头| 4B包计数 | 4B包长度
int udp_send(int len, void *data)
{
	int m_head;
	int m_len;
	int ret;
	static unsigned int m_packcnt = 0;

	m_head = htonl(UDP_PACKET_HEAD);
	memcpy(m_sendbuf, &m_head, 4);
	m_packcnt++;
	memcpy(&m_sendbuf[4], &m_packcnt, 4);
	memcpy(&m_sendbuf[8], &len, 4);
	memcpy(&m_sendbuf[12], data, len);
	m_len = 12 + len;
	ret = sendto(sockFd_send, m_sendbuf, m_len, 0, (struct sockaddr *)&udpSendtoAddr, m_sockAddrSize);
}



int udp_recv()
{
	return recvfrom(sockFd_recv, udp_recvbuf, 2000, 0, (struct sockaddr *)&client_addr, &client_addr_len);
}


