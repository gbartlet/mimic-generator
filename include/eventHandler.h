#ifndef EVENTHANDLER_H
#define EVENTHANDLER_H 
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <time.h>
#include <string>
#include <sstream>
#include <fcntl.h>
#include <vector>
#include <queue> 
#include <unordered_map>
#include "eventQueue.h"
#include "eventNotifier.h"
#include "pollHandler.h"
#include "mimic.h"

#define MAX_BACKLOG_PER_SRV 5



class Connection {
    public:
        bool isServer = true;
        
        EventType lastEvent = NONE;
    
        long timeLastEventCompleted = -1;
    
        // The type of connection this is
        TransportType conn_type;

        // The socket we're using for this connection.
        int sockfd = -1;
        
        // Our address for this connection.
        struct sockaddr our_address;
        // Other side's address.
        struct sockaddr their_address;
        
        // Not strictly necessary to keep track of the conn_id here,
        // but may make things easier to debug/check.
        long int conn_id;
        
        bool blocked();
        bool nextEvent(std::shared_ptr<Event>& e);

        /* For printing/logging only. */
        std::string dstAddr();

    private:
        bool outstandingEvent = false;
        bool waitingToSend = false;
        int waitingToRecv = 0;
        EventQueue* events;
};

class EventHandler {
    private:
        /* We start 3 threads */
        /*	- a server thread (takes in start/stop req, produces accepted events.)  */
        /*	  out: client/serv addr (sockfd) map: addrs->connid, add sockfd		*/
        /* 	- a recv thread (produces events of how much is received from each socket.) */
        /*	  out: sockfd (value) map: sockfd->connid, add value		*/
        /* 	- a send thread (takes in send/connect req, produces sent event). 	*/
        /* 	  out: connid (value) map: none, add value		*/

        long int fileEventsHandledCount;       
        long int lastEventCountWhenRequestingForMore;

        /* We listen to eventNotifiers and listen as servers for clients. */
        PollHandler* myPollHandler;

        /* We consume these. */
        EventNotifier* requestMoreFileEvents;
        EventQueue* incomingFileEvents;
        EventQueue* incomingAcceptedEvents;
        EventQueue* incomingRECVedEvents;
        EventQueue* incomingSentEvents;
        
        /* We produce these. */
        EventQueue* serverStartnStopReq;
        EventQueue* sendReq;
        
        /* Data management structures. */
        ConnectionPairMap * connIDToConnectionMap;
        stringToConnIDMap strToConnID;
        std::unordered_map<int, int long> sockfdToConnIDMap;
        std::unordered_map<long int, int> connToSockfdIDMap;
        std::unordered_map<long int, int> connToWaitingToRecv;
        std::unordered_map<long int, int> connToWaitingToSend;
        std::unordered_map<long int, long int> connToLastCompletedEvent;
        std::unordered_map<long int, EventQueue*> connToEventQueue;
        
        EventHeap waitHeap;

        void processAcceptedEvents();        
        void processSentEvents();
        void processRECVedEvents();
        void popWaitEvents();
        void processFileEvents();
        void addWait();
        void checkDeferred(int long now);
        bool readyForEvent(long int connID, long int delta, long int now);
        void dispatch(std::shared_ptr<Event> dispatchJob, long int now);
        void newConnectionUpdate(int sockfd, long int connID, long int now);
        bool acceptNewConnection(struct epoll_event *poll_e, long int now);
        
    public:
        EventHandler(EventNotifier* loadMoreNotifier, EventQueue* fe, EventQueue* ae, EventQueue* re, EventQueue* se, EventQueue * outserverQ, EventQueue * outSendQ, ConnectionPairMap* ConnMap);
        ~EventHandler();
        bool startup();
        void loop(std::chrono::high_resolution_clock::time_point startTime);
};

#endif