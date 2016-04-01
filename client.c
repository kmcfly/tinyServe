#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<netdb.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<errno.h>
#include <pthread.h>
pthread_t tid[1000];

typedef struct _comm_pack_head_
{
	char		headFlag[4];
	int		dataLen;
	char 		*pData;
}COMM_PACK_HEAD;

void *process(void *arg)
{
    int sockfd;
    char sendbuffer[200];
    char recvbuffer[200];
  //  char buffer[1024];
    struct sockaddr_in server_addr;
    struct hostent *host;
    int portnumber,nbytes;
    int argc=3;
    char *argv[3]={"client","112.74.202.73","1230"};
    if(argc!=3)
    {
 		fprintf(stderr,"Usage :%s hostname portnumber\a\n",argv[0]);
 		exit(1);
    }
    if((host=gethostbyname(argv[1]))==NULL)
    {
 		herror("Get host name error\n");
 		exit(1);
    }
    if((portnumber=atoi(argv[2]))<0)
    {
 		fprintf(stderr,"Usage:%s hostname portnumber\a\n",argv[0]);
 		exit(1);
    }
    if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1)
    {
 		fprintf(stderr,"Socket Error:%s\a\n",strerror(errno));
 		exit(1);
    }
    bzero(&server_addr,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(portnumber);
    server_addr.sin_addr=*((struct in_addr *)host->h_addr);
    if(connect(sockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr))==-1)
    {
 		fprintf(stderr,"Connect error:%s\n",strerror(errno));
 		exit(1);
    }
	COMM_PACK_HEAD com_pack_head;
	char *pstr="lenovo thinkvision isoft";
	memset(&com_pack_head,0,sizeof(com_pack_head));
	sprintf(com_pack_head.headFlag,"%s","kfly");
	com_pack_head.dataLen=strlen(pstr);
	com_pack_head.pData=pstr;
	int length = com_pack_head.dataLen+sizeof(com_pack_head.headFlag)+sizeof(com_pack_head.dataLen);
	printf("client debug info: alllength=%ld, datalen=%d,pData=%s\n",length,com_pack_head.dataLen,com_pack_head.pData);
	char buf[20]={0};
	sprintf(buf,"%4d",com_pack_head.dataLen);
	memcpy(sendbuffer,&com_pack_head.headFlag,sizeof(com_pack_head.headFlag));
	memcpy(sendbuffer+sizeof(com_pack_head.headFlag),&com_pack_head.dataLen,sizeof(com_pack_head.dataLen));
	memcpy(sendbuffer+sizeof(com_pack_head.headFlag)+sizeof(com_pack_head.dataLen),com_pack_head.pData,com_pack_head.dataLen);
	//sprintf(sendbuffer+sizeof(com_pack_head.headFlag),"%4d%s",com_pack_head.dataLen,com_pack_head.pData);
	while(1)
    {
      //if(strcmp(sendbuffer,"quit")==0)
      //   break;
      printf("client debug info: sendbuffer=%s    length=%d\n",sendbuffer,length);
      send(sockfd,sendbuffer,length-5,0);
      //recv(sockfd,recvbuffer,200,0);
      //printf("recv data from server is :%s\n",recvbuffer);
      sleep(5);
	  send(sockfd,sendbuffer+length-5,5,0);
	  sleep(15);
	  break;
    }
   // if((nbytes=read(sockfd,buffer,1024))==-1)
    //{
// fprintf(stderr,"read error:%s\n",strerror(errno));
// exit(1);
  //  }
   // buffer[nbytes]='\0';
   // printf("I have received %s\n",buffer);
    close(sockfd);

}
int main(int argc,char **argv){
     int i, ret;
     void *thread_result;
    pid_t pid;
    for(i = 0; i < 100; i++){
        if((ret = pthread_create(&tid[i], NULL, process, 
            (void *)i)) != 0){
            fprintf(stderr, "pthread_create:%s\n",
                strerror(ret));
            exit(1);
        }
    }
    for(i = 0; i < 100; i++){
    pthread_join(tid[i], &thread_result);//等待id的线程结束  
    }
	sleep(15);
   printf("client run over ...........................\n"); 
}