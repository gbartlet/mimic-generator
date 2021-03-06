#ifndef MIMIC_H
#define MIMIC_H

#include <ctime>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <condition_variable>
#include "connections.h"


/* Mutexes and condition variables. */
extern std::mutex fileHandlerMTX;
extern std::condition_variable fileHandlerCV;
extern bool loadMoreFileEvents;
bool returnLoadMoreFileEvents();

#define maxQueuedFileEvents 1000000


extern std::atomic<bool> isRunning;

enum EventType {
                ACCEPT, 	/* We should expect to accept a connection from a client. */
                ACCEPTED, 	/* We accepted a connection from a client. */
                CONNECT,	/* As a client, connect to a server. */
                CONNECTED, 	/* We successfully connected to a server. */
                CLOSE,		/* Socket should be closed or is closed.	*/
                RECV, 		/* Wait for {value}bytes of  data. */
                RECVD, 		/* Accepted {value}bytes of data. */
                WAIT,		/* Wait {value}ms before next event. */
                SEND, 		/* Send {value}bytes of data. */
                SENT,		/* We successfully sent {value}bytes of data. */
                SRV_START,	/* Bring up a server. */ 
                SRV_STARTED, 	/* Server has been started. */
                SRV_END, 	/* Bring down a server. */
                NONE		/* Dummy value. */
                };

enum TransportType {
                TCP,
                UDP
                };
            
enum DomainType {
    IPV4,
    IPV6
};

class Event {
    public:
        int sockfd = -1;
        long int ms_from_start = 1;	 /* Keep track of event time relative to start in ms. */
        long int ms_from_last_event = -1;
        EventType type = NONE;        
        long int conn_id = -1;
        long int event_id = -1;
        long int value = 0;		/* What this value holds depends on what type of event this is. */
};


long int msSinceStart(std::chrono::high_resolution_clock::time_point startTime);

#endif



