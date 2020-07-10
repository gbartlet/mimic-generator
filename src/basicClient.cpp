#include <stdio.h> 
#include <stdlib.h> 
#include <thread>
#include <unistd.h> 
#include <fcntl.h>
#include <errno.h> 
#include <string.h> 
#include <sys/epoll.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include "mimic.h"
#include "eventQueue.h"


#define threadingCount 4
#define noconnection 1000
#define PORT5 5458 // the port client will be connecting to 
#define MAXEVENTS 64
int efd[threadingCount];
long unsigned int count[threadingCount];
int stop = 0;
//--------------------------------------- 
#define MAXBUF 50
//#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define MAXDATASIZE 1000
//#define ip "127.0.0.1"
#define ip "10.1.1.3"
//#define MAX_EVENTS 100
#define MAX_EVENTS 1000

struct epoll_event ev, events[MAX_EVENTS];
int nfds, epollfd;
unsigned long freq[1024];

void setnonblocking(int sock){
    int opts;
    if ((opts = fcntl(sock, F_GETFL)) < 0)
        printf("GETFL failed\n");
    opts = opts | O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opts) < 0)
        printf("SETFL failed\n");

}

int create_connect(int port ){

    struct sockaddr_in serveraddr; 

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, & (serveraddr.sin_addr)); //creating and filling the socket
    serveraddr.sin_port = htons(port); //PORT to connect to edge server
    printf("gonna connect at port: %d and %d", port, ntohs( serveraddr.sin_port));
    if (connect(sockfd, (struct sockaddr * ) & serveraddr, sizeof(serveraddr)) == -1) {
        close(sockfd);
        printf("ERROR :: client: connect %d\n", sockfd);
        exit(1);
    }
    
    return sockfd;

}

void init(int port, int ind) {

     int sofd = create_connect(port);
     setnonblocking(sofd); 
     struct epoll_event evv;
     evv.events = EPOLLOUT | EPOLLET;
     evv.data.fd = sofd; 
     epoll_ctl(efd[ind], EPOLL_CTL_ADD, sofd, &evv); 
}

void intHandler(int dummy) {
    int ii;
    
    for (ii=0;ii<threadingCount;ii++){ //1020 is the upper limit and 1018 without error of accept:too many files    
        printf("count[%d]=%lu\n", ii, count[ii]);
    
          //int pthread_create(pthread_t * pth, pthread_attr_t *att, void * (*function), void * arg);
    } 
    exit(0);
    
}
void timeprint (){
    int lll=0;
        unsigned long countprev[4];
    while(1){
        sleep(1);
        for (lll =0; lll< threadingCount; lll++){
            printf ("count[%d] = %lu \n", lll, count[lll]-countprev[lll]);
            countprev[lll] = count[lll];
        }
        printf("----\n");
    }

}

static void do_send(int fd, int ind){             
    char buff[100]="message test";   
    struct epoll_event event; int n;
    //printf ("event=%ld on fd=%d\n", events[i].events,o!
    //sleep(1);
    //printf("gonna send on %d for thread#%d\n ",fd, ind );
    if((n = send(fd, buff, sizeof(buff), 0)) == -1) 
        perror("send");

    count[ind]++;
    event.events =  EPOLLOUT | EPOLLET;
    event.data.fd = fd;  
    epoll_ctl(efd[ind], EPOLL_CTL_MOD, fd, &event);  
}
                                           
static void do_recv(int fd, int ind){
    ssize_t n;
    struct epoll_event event;
    char buf[512];
    n = read (fd, buf, sizeof buf);
    printf("message received on %d , %s\n",fd ,buf);
    event.events = EPOLLOUT | EPOLLET;
    event.data.fd = fd;  
    epoll_ctl(efd[ind], EPOLL_CTL_MOD, fd, &event);


/*	while (1){
        ssize_t count;
        char buf[512];
        struct epoll_event event;
        count = read (fd, buf, sizeof buf);
//        int n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
		if (n > 0){
			printf("fd[%d] recv len %d and msg = %s by thread#%d\n", fd, n, buf, ind);
			if (n == sizeof(buf)){
				printf("fd[%d] need recv nextloop %d\n", fd, n);
				continue;
			}
			break;
		}
		if (n == 0) {
			printf("fd[%d] close bro!\n",fd);
			close (fd);
			return ;
		}
		if (errno == EINTR) {
			printf("fd[%d] need recv again!\n", fd);
			continue;
		}
		else if (errno == EAGAIN){
			printf("fd[%d] need recv next!\n", fd);
			event.data.fd = fd;
            event.events = EPOLLIN |EPOLLET;
            int ss = epoll_ctl (efd[ind], EPOLL_CTL_ADD, fd, &event);
			break;
		}
		else {
			printf("fd[%d] close bro!\n",fd);
			close (fd);
			return ;
		}
	}*/
}

static void do_process(int fd, int events, int ind ) {
	if(!(events & (EPOLLIN | EPOLLOUT)))  {
	// if ((events[i].events & EPOLLERR) ||
    //          (events[i].events & EPOLLHUP) 
		printf ("ERROR: epoll error event bro!\n");
        close (fd);
        return;
	}
	if(events & EPOLLIN) {
		do_recv(fd, ind);
	}
	else if(events & EPOLLOUT) {
		do_send(fd, ind);
	}
}
static void so_thread_entry(void *arg)
{

    EventQueue * eq = new EventQueue();
    
    count[atoi((char *)arg)] = 0;
	//pthread_detach(pthread_self());
	struct epoll_event event;
	char thread_name[50];
	long int lastEventTime = -1;
	
	sleep(1);
	//pthread_getname_np(pthread_self(), thread_name, 50); 
	
	int n = 0, i = 0;
	struct epoll_event *events = (struct epoll_event *)calloc(MAXEVENTS, sizeof event);
	if(events == NULL) {
		printf("error in initializing epoll_events\n");
		stop = 1;
		return ;
	}
	
	while (!stop) {
		n = epoll_wait (efd[atoi((char *)arg)], events, MAXEVENTS, -1);
		for(i = 0; i < n; i++) {
		    Event e;
                    e.ms_from_start = i;
                    eq->addEvent(std::make_shared<Event>(e));
		    
//		    printf("Event on desc = %d occured for %s\n",events[i].data.fd ,thread_name);
		    do_process(events[i].data.fd, events[i].events, atoi((char *)arg));
		    
		    std::shared_ptr<Event> job;
		    if(eq->getEvent(job)){
		        if(lastEventTime > job->ms_from_start) {
		            std::cout << "ERROR:Job appears to be out of order!" <<std::endl;
		        }
		        lastEventTime = job->ms_from_start;
                        job.reset();
		    }
		    else {
		        std::cout << "ERROR:Problem retreiving event." << std::endl;
		    }
		    
		    
		}
	}
}
int main(int argc, char * argv[]) {

    //unsigned int ports [8] ={5454,5455,5456,5457,5458,5459,5460,5461};
    unsigned int ports [1024];
    int i,j,k;    
    int n;  
    memset(freq, 0,sizeof(freq));
    signal(SIGINT, intHandler);
    
    unsigned int start_port = 5454;
    for(i=0; i<1024; i++) {
        ports[i] = start_port + i;
    }

///////////////////////////////
    //pthread_t threadstime; 
    /*if (pthread_create(&threadstime, NULL, timeprint, "...") != 0)
     {
          fprintf(stderr, "error: Cannot create time thread\n");
          exit(0);
     }*/
     
    std::thread timeThread(timeprint);
    timeThread.detach(); 

    for (i=0;i<threadingCount;i++) {
        efd[i] = epoll_create1(0);
        //int efd= epoll_create1(0);
        if (efd[i] == -1) {
           perror("epoll_create1");
           exit(EXIT_FAILURE);
        }

    }
    int curport = 0;
    std::thread workers[10];
    for (i=0;i<threadingCount;i++){
        printf("threadingCount = %d \n", threadingCount);
        int socketperthread = noconnection/threadingCount ;
        printf ("gonna set %d socket per threads \n", socketperthread);
        for (j =0 ;j < socketperthread;j++ ){
            init(ports[curport], i);
            printf ("OK %d port on thread %d  \n",curport,i);
            curport ++;
        }
        int rc;
        //pthread_t tid;
        char * threadbuffer;
        threadbuffer = (char *)calloc(50, sizeof(char));
        sprintf(threadbuffer, "thread#%d", i);
        
        /*if((rc = pthread_create(&tid,NULL, so_thread_entry, (void*)i)) != 0) {
	        printf("pthread_create err \n");
	        stop = 1;
	        return -1;
        }
        pthread_setname_np(tid, threadbuffer);       
        */
        
        workers[i] = std::thread(so_thread_entry, (void*)&i);
        workers[i].detach();
        
        printf("thread created and stop = %d \n", stop); 
        sleep(1);
        
    }
       while (!stop) {

	    sleep(200);
    }
    printf("gonna close it now ...............\n");
    
    //close(sockfd); //closing the socket
    return 0;
}

