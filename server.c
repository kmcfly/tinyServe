/************************************************* 
* File name   : server.c 
* Description : �����̲��������� 
* Author      : sg131971@qq.com 
* Version     : V1.0 
* Date        :  
* Compiler    : arm-linux-gcc-4.4.3 
* Target      : mini2440(Linux-2.6.32) 
* History     :  
*   <author>  <time>   <version >   <desc> 
*************************************************/  
#include <stdio.h>   
#include <string.h>   
#include <unistd.h>   
#include <sys/types.h>   
#include <sys/socket.h>   
#include <netinet/in.h>   
#include <arpa/inet.h>   
#include <sys/time.h>   
#include <stdlib.h>   
  
#define PORT 1234               //�������˿�   
#define BACKLOG 5               //listen�����еȴ���������   
#define MAXDATASIZE 2048        //��������С   
  
typedef struct _CLIENT  
{  
    int fd;                     //�ͻ���socket������   
    char name[20];              //�ͻ�������   
    struct sockaddr_in addr;    //�ͻ��˵�ַ��Ϣ�ṹ��   
    char data[MAXDATASIZE];     //�ͻ���˽������ָ��   
} CLIENT;  
  
void process_client(CLIENT * client, char *recvbuf, int len);   //�ͻ���������   
  
/************************************************* 
* Function    : main() 
* Description :  
* Calls       : process_client() 
* Called By   :  
* Input       :  
* Output      :  
* Return      :  
*************************************************/  
void main(int argc ,char **argv)  
{  
    int i, maxi, maxfd, sockfd;  
    int nready;  
    ssize_t n;  
    fd_set rset, allset;        //select������ļ�����������   
    int listenfd, connectfd;    //socket�ļ�������   
    struct sockaddr_in server;  //��������ַ��Ϣ�ṹ��   
  
    CLIENT client[FD_SETSIZE];  //FD_SETSIZEΪselect����֧�ֵ��������������   
    char recvbuf[MAXDATASIZE];  //������   
    int sin_size;               //��ַ��Ϣ�ṹ���С   
  
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)  
    {                           //����socket�������ڼ����ͻ��˵�socket   
        perror("Creating socket failed.");  
        exit(1);  
    }  
  
    int opt = SO_REUSEADDR;  
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  //����socket����   
  
    bzero(&server, sizeof(server));  
    server.sin_family = AF_INET;  
    server.sin_port = htons(PORT);  
    server.sin_addr.s_addr = htonl(INADDR_ANY);  
  
    if (bind(listenfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)  
    {                           //����bind�󶨵�ַ   
        perror("Bind error.");  
        exit(1);  
    }  
  
    if (listen(listenfd, BACKLOG) == -1)  
    {                           //����listen��ʼ����   
        perror("listen() error\n");  
        exit(1);  
    }  
  
    //��ʼ��select   
    maxfd = listenfd;  
    maxi = -1;  
    for (i = 0; i < FD_SETSIZE; i++)  
    {  
        client[i].fd = -1;  
    }  
    FD_ZERO(&allset);           //���   
    FD_SET(listenfd, &allset);  //������socket����select��������������   
  
    while (1)  
    {  
        struct sockaddr_in addr;  
        rset = allset;  
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);    //����select   
        printf("Select() break and the return num is %d. \n", nready);  
  
        if (FD_ISSET(listenfd, &rset))  
        {                       //����Ƿ����¿ͻ�������   
            printf("Accept a connection.\n");  
            //����accept�����ط�������ͻ������ӵ�socket������   
            sin_size = sizeof(struct sockaddr_in);  
            if ((connectfd =  
                 accept(listenfd, (struct sockaddr *)&addr, (socklen_t *) & sin_size)) == -1)  
            {  
                perror("Accept() error\n");  
                continue;  
            }  
  
            //���¿ͻ��˵ļ�������   
            for (i = 0; i < FD_SETSIZE; i++)  
            {  
                if (client[i].fd < 0)  
                {  
                    char buffer[20];  
                    client[i].fd = connectfd;   //����ͻ���������   
                    memset(buffer, '0', sizeof(buffer));  
                    sprintf(buffer, "Client[%.2d]", i);  
                    memcpy(client[i].name, buffer, strlen(buffer));  
                    client[i].addr = addr;  
                    memset(buffer, '0', sizeof(buffer));  
                    sprintf(buffer, "Only For Test!");  
                    memcpy(client[i].data, buffer, strlen(buffer));  
                    printf("You got a connection from %s:%d.\n", inet_ntoa(client[i].addr.sin_addr),ntohs(client[i].addr.sin_port));  
                    printf("Add a new connection:%s\n",client[i].name);  
                    break;  
                }  
            }  
              
            if (i == FD_SETSIZE)  
                printf("Too many clients\n");  
            FD_SET(connectfd, &allset); //����socket���ӷ���select��������   
            if (connectfd > maxfd)  
                maxfd = connectfd;  //ȷ��maxfd�����������   
            if (i > maxi)       //�������Ԫ��ֵ   
                maxi = i;  
            if (--nready <= 0)  
                continue;       //���û���¿ͻ������ӣ�����ѭ��   
        }  
  
        for (i = 0; i <= maxi; i++)  
        {  
            if ((sockfd = client[i].fd) < 0)    //����ͻ���������С��0����û�пͻ������ӣ������һ��   
                continue;  
            // �пͻ����ӣ�����Ƿ�������   
            if (FD_ISSET(sockfd, &rset))  
            {  
                printf("Receive from connect fd[%d].\n", i);  
                if ((n = recv(sockfd, recvbuf, MAXDATASIZE, 0)) == 0)  
                {               //�ӿͻ���socket�����ݣ�����0��ʾ�����ж�   
                    close(sockfd);  //�ر�socket����   
                    printf("%s closed. User's data: %s\n", client[i].name, client[i].data);  
                    FD_CLR(sockfd, &allset);    //�Ӽ���������ɾ����socket����   
                    client[i].fd = -1;  //����Ԫ�����ʼֵ����ʾû�ͻ�������   
                }  
                else  
                    process_client(&client[i], recvbuf, n); //���յ��ͻ����ݣ���ʼ����   
                if (--nready <= 0)  
                    break;      //���û���¿ͻ��������ݣ�����forѭ���ص�whileѭ��   
            }  
        }  
    }  
    close(listenfd);            //�رշ���������socket        
}  
  
/************************************************* 
* Function    : process_client() 
* Description : ����ͻ������Ӻ��� 
* Calls       :  
* Called By   : main() 
* Input       :  
* Output      :  
* Return      :  
*************************************************/  
void process_client(CLIENT * client, char *recvbuf, int len)  
{  
    char sendbuf[MAXDATASIZE];  
    int i;  
  
    printf("Received client( %s ) message: %s\n", client->name, recvbuf);  
     
    for (i = 0; i < len - 1; i++)  
    {  
        sendbuf[i] = recvbuf[i];  
    }  
      
    sendbuf[len - 1] = '\0';  
  
    send(client->fd, sendbuf, strlen(sendbuf), 0);  
}  