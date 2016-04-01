#include  <unistd.h>
#include  <sys/types.h>       /* basic system data types */
#include  <sys/socket.h>      /* basic socket definitions */
#include  <netinet/in.h>      /* sockaddr_in{} and other Internet defns */
#include  <arpa/inet.h>       /* inet(3) functions */
#include <sys/epoll.h> /* epoll function */
#include <fcntl.h>     /* nonblocking */
#include <sys/resource.h> /*setrlimit */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <deque>
#include <list>

using namespace std;
#define MAXEPOLLSIZE 10000
#define MAXLINE 10240

int handle(int connfd);
int RecvBuff(int connfd,void *pBuf,int iLen);
int SWL_Recv(int sock, void* pBuf, int iLen, int iFlags);
int CreateSendThread();


typedef struct _comm_pack_head_
{
	char		headFlag[4];
	int		dataLen;
}COMM_PACK_HEAD;
const char *headFlag="kfly";

enum RECV_DATA_TYPE
{
	DATA_TYPE_TRANS			= 0x0,	//透明数据
	DATA_TYPE_PACK_INFO		= 0x1
};
typedef struct _recv_data_buffer_
{
	long			dataSize;	//要接收的总的数据大小
	char			*pData;		//接收数据的缓冲区
	long			recvSize;	//已经接收的数据
	long			dataType;	//用于解析一些特殊数据，为兼容以前的传输模块设计
}RECV_DATA_BUFFER;

typedef struct _CLIENT  
{  
    int fd;                     //客户端socket描述符   
    char name[20];              //客户端名称   
    struct sockaddr_in addr;    //客户端地址信息结构体   
    char *pData;                //客户端私有数据指针 
    long dataSize;				//要接受的数据大小
	long recvSize;				//已经接收的数据
	bool bReceOver;				//数据接收完成
} CLIENT_INFO; 

static std::list<CLIENT_INFO> g_con_list_client;

static std::list<CLIENT_INFO> g_list_client_info;//已经接收完成

int DealBodyBuff(int groupFd,int connfd,CLIENT_INFO *pClientInfo,struct epoll_event &ev,int &curfds);

void printClientList(std::list<CLIENT_INFO> list_client_info);

void copyClientInfo(CLIENT_INFO *src,CLIENT_INFO *dst);


int InitClientInfo(CLIENT_INFO *pClientInfo){
	pClientInfo->fd=-1;
	memset(&pClientInfo->name,0,sizeof(pClientInfo->name));
	if(pClientInfo->pData != NULL){
		delete pClientInfo->pData;
		pClientInfo->pData=NULL;
	}
	pClientInfo->recvSize=0;
	pClientInfo->dataSize=0;
	pClientInfo->bReceOver=false;
}
int clearClientInfo(CLIENT_INFO *pClientInfo){

	if(pClientInfo->pData != NULL){
		delete pClientInfo->pData;
		pClientInfo->pData=NULL;
	}
	pClientInfo->recvSize=0;
	pClientInfo->dataSize=0;
	pClientInfo->bReceOver=false;
}

CLIENT_INFO * GetClientInfo(int connId,std::list<CLIENT_INFO> &list_client){
	std::list<CLIENT_INFO>::iterator it = list_client.begin();
	for(it;it != list_client.end();it++){
		if((*it).fd==connId)
			return &(*it);
	}
	return NULL;
}
int DelClientInfo(int connId,std::list<CLIENT_INFO> &list_client){
	std::list<CLIENT_INFO>::iterator it = list_client.begin();
	for(it;it != list_client.end();){
		if((*it).fd==connId){
			if((*it).pData !=NULL){
				delete (*it).pData;
				(*it).pData=NULL;
			}
			it=list_client.erase(it);
			break;
		}	else{
			it++;
		}	
	}
	return 0;
}
std::list<CLIENT_INFO> AddClientInfo(int connId,struct sockaddr_in ClientAddr,std::list<CLIENT_INFO> &list_client){
	CLIENT_INFO clientInfo;
	clientInfo.pData=NULL;
	InitClientInfo(&clientInfo);
	clientInfo.fd=connId;
	clientInfo.addr=ClientAddr;
	list_client.push_back(clientInfo);
    printf("debug:New connect from %s....%d.....connectCnt=%d..%s....%d\n",inet_ntoa(ClientAddr.sin_addr), ClientAddr.sin_port,list_client.size(),__FILE__,__LINE__);
	//printf("run over .......%d.....%s...\n",__LINE__,__FILE__);
	return list_client;
}

void SWL_PrintError(const char* pFile, int iLine)
{
#ifdef  __ENVIRONMENT_LINUX__
	char szErrorPos[256] = {0};
	sprintf(szErrorPos, "%s %d ", pFile, iLine);
	perror(szErrorPos);
#elif defined  __ENVIRONMENT_WIN32__
	int iErrno = WSAGetLastError();
	printf("%s %d Errno = %d\n", pFile, iLine, iErrno);
#endif 
}
int setnonblocking(int sockfd)
{
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
        return -1;
    }
    return 0;
}
int RemoveFdFromEpoll(int epollId,int fd,int &curfds,struct epoll_event &ev,std::list<CLIENT_INFO> &list_client){
	epoll_ctl(epollId, EPOLL_CTL_DEL, fd,&ev);
	curfds--;
	DelClientInfo(fd, list_client);
	close(fd);
	printf("debug: remove fd=%d from groupid = %d ..curfds=%d....%s.....%d...\n",curfds,epollId,curfds,__FILE__,__LINE__);
	return 0;
}
int main(int argc, char **argv)
{
    int  servPort = 1230;
    int listenq = 1024;

    int listenfd, connfd, kdpfd, nfds, n, nread, curfds,acceptCount = 0;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    struct epoll_event ev;
    struct epoll_event events[MAXEPOLLSIZE];
    struct rlimit rt;
    char buf[MAXLINE];
    
    //client info
	CreateSendThread();
    /* 设置每个进程允许打开的最大文件数 */
    rt.rlim_max = rt.rlim_cur = MAXEPOLLSIZE;
    if (setrlimit(RLIMIT_NOFILE, &rt) == -1) 
    {
        perror("setrlimit error");
        return -1;
    }


    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl (INADDR_ANY);
    servaddr.sin_port = htons (servPort);

    listenfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (listenfd == -1) {
        perror("can't create socket file");
        return -1;
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (setnonblocking(listenfd) < 0) {
        perror("setnonblock error");
    }

    if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr)) == -1) 
    {
        perror("bind error");
        return -1;
    } 
    if (listen(listenfd, listenq) == -1) 
    {
        perror("listen error");
        return -1;
    }
    /* 创建 epoll 句柄，把监听 socket 加入到 epoll 集合里 */
    kdpfd = epoll_create(MAXEPOLLSIZE);
    ev.events = EPOLLIN | EPOLLET;//读事件，边沿触发
    ev.data.fd = listenfd;
    if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, listenfd, &ev) < 0) 
    {
        fprintf(stderr, "epoll set insertion error: fd=%d\n", listenfd);
        return -1;
    }
    curfds = 1;

    printf("epollserver startup,port %d, max connection is %d, backlog is %d\n", servPort, MAXEPOLLSIZE, listenq);

    for (;;) {
        /* 等待有事件发生 */
        nfds = epoll_wait(kdpfd, events, curfds, -1);
        if (nfds == -1)
        {
            perror("epoll_wait");
            continue;
        }
//		printf("debug: something happened...nfds=%d...\n",nfds);
        /* 处理所有事件 */
        for (n = 0; n < nfds; ++n)
        {
            if (events[n].data.fd == listenfd) 
            {//new connect
                connfd = accept(listenfd, (struct sockaddr *)&cliaddr,&socklen);
                if (connfd < 0) 
                {
                    perror("accept error");
                    continue;
                }
                if (curfds >= MAXEPOLLSIZE) {
                    fprintf(stderr, "too many connection, more than %d\n", MAXEPOLLSIZE);
                    close(connfd);
                    continue;
                } 
                if (setnonblocking(connfd) < 0) {
                    perror("setnonblocking error");
                }
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = connfd;		
                if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, connfd, &ev) < 0)
                {
                    fprintf(stderr, "add socket '%d' to epoll failed: %s\n", connfd, strerror(errno));
                    return -1;
                }
				AddClientInfo(connfd,cliaddr, g_con_list_client);
//				printf("debug: run over........%s....%d\n",__FILE__,__LINE__);
                curfds++;
                continue;
            } 
            else if(events[n].events&EPOLLIN){
                //数据读取
                COMM_PACK_HEAD com_pack_head;
//				printf("debug: run over........%s....%d\n",__FILE__,__LINE__);
				CLIENT_INFO *pClientInfo=GetClientInfo(events[n].data.fd,g_con_list_client);
				if(pClientInfo == NULL)continue;
               // printf("server debug info:run in the epollin condition.sizeof(com_pack_head)=%d..\n",sizeof(com_pack_head));
				if(pClientInfo->dataSize == 0){
					int ret = RecvBuff(events[n].data.fd,&com_pack_head,sizeof(com_pack_head));

//					printf("debug: n=%d  ret=%d....headfl=%s....datalen=%d\n",n,ret,com_pack_head.headFlag,com_pack_head.dataLen);
					if((ret == sizeof(com_pack_head))&&(strncmp(com_pack_head.headFlag,headFlag,strlen(headFlag))==0)){
//						printf("debug info: get the head...n=%d..\n",n);
						pClientInfo->fd = events[n].data.fd;
						char buffer[20];
						memset(buffer,0,sizeof(buffer));
						sprintf(buffer,"client %d",curfds);
						memset(pClientInfo->name,0,sizeof(pClientInfo->name));
						strncpy(pClientInfo->name,buffer,strlen(buffer));
						long datasize = com_pack_head.dataLen;
						pClientInfo->dataSize = datasize;
						//pClientInfo[n]->addr=
						pClientInfo->pData = new char[datasize];
						pClientInfo->bReceOver=false;
						//printf("%%%%debug:fd=%ld,name=%s,dataSize=%ld,data=%s   ....%s....%d\n",(pClientInfo+n)->fd,(pClientInfo+n)->name,(pClientInfo+n)->dataSize,(pClientInfo+n)->pData,__FILE__,__LINE__);
						DealBodyBuff(kdpfd,events[n].data.fd,pClientInfo,ev,curfds);
					}else if(ret ==0){
						RemoveFdFromEpoll(kdpfd,events[n].data.fd,curfds,ev,g_con_list_client);
					}else{
						printf("debug: ERROR  ...ret=%d...%s.....%d...\n",ret,__FILE__,__LINE__);
					}
	            }
				else{
					//继续接收body 信息
				//	printf("debug: continue get the body buffer ...n=%d\n",n);
	            	DealBodyBuff(kdpfd,events[n].data.fd,pClientInfo,ev,curfds);
	            }
            	}
        }
    }
    close(listenfd);
    return 0;
}
int DealBodyBuff(int groupFd,int connfd,CLIENT_INFO *pClientInfo,struct epoll_event &ev,int &curfds){
	//printf("debug: recvSize=%d.....dataSize=%d......%s....%d\n",pClientInfo->recvSize,pClientInfo->dataSize,__FILE__,__LINE__);
	int readNum = RecvBuff(connfd,pClientInfo->pData+pClientInfo->recvSize,pClientInfo->dataSize-pClientInfo->recvSize);
	if(readNum>0){
		pClientInfo->recvSize+=readNum;
		//printf("debug: recvSize=%d.....dataSize=%d......%s....%d\n",pClientInfo->recvSize,pClientInfo->dataSize,__FILE__,__LINE__);
		if(pClientInfo->recvSize == pClientInfo->dataSize){
			//接收到完整的一包数据
		//	printf("debug: get a whole packet..............\n");
			pClientInfo->bReceOver=true;
			
			CLIENT_INFO clientInfo;
			//这里要完成一次深拷贝
			copyClientInfo(pClientInfo,&clientInfo);
			g_list_client_info.push_back(clientInfo);
			//printClientList(g_list_client_info);
			clearClientInfo(pClientInfo);
		}
	}else if(readNum == 0){
		//断开
		RemoveFdFromEpoll(groupFd,connfd,curfds,ev,g_con_list_client);
	}else{
		//错误数据已读完，但是不够
	//	printf("debug: ERROR  ......%s.....%d...\n",__FILE__,__LINE__);
	}
	return 0;
}
void printClientList(std::list<CLIENT_INFO> list_client_info){
	list<CLIENT_INFO>::iterator it = list_client_info.begin();
	for(it;it != list_client_info.end();it++){
		CLIENT_INFO clientInfo = *it;
		printf("%%%%debug: list size = %ld....fd=%ld,name=%s,dataSize=%ld,data=%s   clientIp=%s clientPort=%d....%s....%d\n",list_client_info.size(),clientInfo.fd,clientInfo.name,clientInfo.dataSize,clientInfo.pData,inet_ntoa(clientInfo.addr.sin_addr), clientInfo.addr.sin_port,__FILE__,__LINE__);
	}
}
void copyClientInfo(CLIENT_INFO *src,CLIENT_INFO *dst){
	dst->fd=src->fd;
	memcpy(&dst->name,&src->name,sizeof(src->name));
	dst->dataSize = src->dataSize;
	dst->recvSize=src->recvSize;
	dst->addr=src->addr;
	dst->bReceOver=src->bReceOver;
	dst->pData = new char[dst->dataSize+1];
	memcpy(dst->pData,src->pData,dst->dataSize);
	*(dst->pData+dst->dataSize) ='\0';
}
int RecvBuff(int connfd,void *pBuf,int iLen){
	char *ptr=(char *)pBuf;
	int readNum=0;
	while(readNum!=iLen){
		int ret = read(connfd,ptr,iLen);
		if(ret >0){
			readNum+=ret;
			ptr+=ret;
		}else if(ret ==0){
		//断开连接
			printf("info: get a FIN infomation....%d....%s\n",__LINE__,__FILE__);
			return 0;
		}else{
		//错误已经读完了
			//printf("info: error but get %d bytes..ret=%d..errno=%d  %d....%s\n",readNum,ret,errno,__LINE__,__FILE__);
			if((errno ==EAGAIN)){
				return readNum;//ret;
			}
			return 0;
		}
	}
	return readNum;
}
//接收数据
//int SWL_Recv(SWL_socket_t sSock, void* pBuf, int iLen, int iFlags);
int SWL_Recv(int sock, void* pBuf, int iLen, int iFlags)
{
#ifdef            __ENVIRONMENT_WIN32__
	return recv(sock, static_cast<char*>(pBuf), iLen, 0);
#else
	return static_cast<int>(recv(sock, pBuf, static_cast<size_t>(iLen), MSG_DONTWAIT));
#endif
}

int handle(int connfd) {
    int nread;
    char buf[MAXLINE];
    nread = read(connfd, buf, MAXLINE);//读取客户端socket流

    if (nread == 0) {
        printf("client close the connection\n");
        close(connfd);
        return -1;
    } 
    if (nread < 0) {
        perror("read error");
        close(connfd);
        return -1;
    }    
    write(connfd, buf, nread);//响应客户端  
    return 0;
}


void * SendProc(void *arg){
    while(1){
        sleep(5);
		//printClientList(g_list_client_info);
    }
}
int CreateSendThread(){
    int ret;
    pthread_t tid;

    if((ret = pthread_create(&tid, NULL, SendProc, 
            NULL) != 0)){
            fprintf(stderr, "pthread_create:%s\n",
                strerror(ret));
            exit(1);
        }
        return ret;
}

