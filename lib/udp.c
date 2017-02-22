#include <semaphore.h>
#include "udp.h"

int sockFd_send;//�����׽���
int sockFd_recv;//�����׽���

struct sockaddr_in	udpMyAddr;//����UDPͨ�ŵĶ˿ڣ�������UDP���Ķ˿�
struct sockaddr_in	udpSendtoAddr;//���շ��Ķ˿ںš�����ʱΪ�Է��˿ڣ�����ʱΪ�������ն˿�
char*	m_pszAddress;
int m_sockAddrSize;
struct sockaddr_in client_addr;
int client_addr_len;



unsigned char m_sendbuf[2000];
unsigned char udp_recvbuf[2000];


void udp_init(char* ip_sendto, int port_sendto, int port_myrecv)
{
	/***UDP�����׽���***/

	//���÷��͵�ַ
	m_sockAddrSize = sizeof(struct sockaddr_in);
	bzero((char*)&udpSendtoAddr, m_sockAddrSize);
	udpSendtoAddr.sin_family = AF_INET;
	udpSendtoAddr.sin_addr.s_addr = inet_addr(ip_sendto);
	udpSendtoAddr.sin_port = htons(port_sendto);
	//����UDP�����׽���
	sockFd_send = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockFd_send == -1)
	{
		perror("udp send create socket failed");
		close(sockFd_send);
		return;
	}
	printf("udp send ini ok!\n");

	/***UDP�����׽���***/

	//���յ�ַ
	m_sockAddrSize = sizeof(struct sockaddr_in);
	bzero((char*)&udpMyAddr, m_sockAddrSize);
	udpMyAddr.sin_family = AF_INET;
	udpMyAddr.sin_addr.s_addr = INADDR_ANY;
	udpMyAddr.sin_port = htons(port_myrecv);
	client_addr_len = sizeof(client_addr);
	//����UDP�����׽���
	sockFd_recv = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockFd_recv == -1)
	{
		perror("udp recv create socket failed");
		close(sockFd_recv);
		return;
	}
	//���׽���
	if (bind(sockFd_recv, (struct sockaddr *)&udpMyAddr, sizeof(struct sockaddr_in)) == -1)
	{
		perror("udp recv bind err");
		close(sockFd_recv);
		return;
	}
	printf("udp recv ini ok!\n");
}
//�������ݰ���ʽ
// 4B��ͷ| 4B������ | 4B������
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


