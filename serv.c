#include	<limits.h>		/* for OPEN_MAX */
#include    <sys/epoll.h>
#include    <stdio.h>
#include    <assert.h>
#include    <stdlib.h>
#include    "my_epoll.h"

int main(int argc,char *argv[]){

    int i=0;
    if(argc!=2){
        printf("%s <port>\n",argv[0]);
        exit(0);
    }
    int servfd;

    /* 1.创建服务 */
    int epfd = creatEpollServer(&servfd,atoi(argv[1]));
    assert("epfd != -1");

    /* 2.创建事件 */
    struct epoll_event events[OPEN_MAX];

    /* 3.等待事件 */
    while(1){
        int nfd = epoll_wait(epfd,events,current_connect+1,-1);
        for(i=0;i<nfd;i++){
            /* 如果是servfd可读，即有新的连接，服务器处于SYN_RCVD状态 */
            if(events[i].data.fd == servfd){
                /* 接受连接，服务器状态变为：ESTABLISHED */
                epollAccept(epfd,servfd);

            }else if(events[i].events&EPOLLIN){//如果是可读事件
                epollRead(epfd,&events[i]);
            }else{
                printf("fd = %d\n",events[i].data.fd);
                printf("没有理由运行到这里！\n");
            }
        }
    }
    
    return 0;
}