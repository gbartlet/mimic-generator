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



/*
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

        // For printing/logging only.
        std::string dstAddr();

    private:
        bool outstandingEvent = false;
        bool waitingToSend = false;
        int waitingToRecv = 0;
        EventQueue* events;
};
*/

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
	EventHeap* eventsToHandle;
        
        /* Data management structures. */
        ConnectionPairMap * connIDToConnectionMap;
        stringToConnIDMap strToConnID;
        std::unordered_map<int, int long> sockfdToConnIDMap;
	std::unordered_map<std::string, int long> serverToSockfd;
	std::unordered_map<std::string, int long> serverToCounter;
	std::unordered_map<int long, std::string> connToServerString;
        std::unordered_map<long int, int> connToSockfdIDMap;
        std::unordered_map<long int, int> connToWaitingToRecv;
        std::unordered_map<long int, int> connToWaitingToSend;
	std::unordered_map<long int, int> connToDelay;
	std::unordered_map<long int, long int> connToLastPlannedEvent;
	std::unordered_map<long int, conn_state> connState;
        std::unordered_map<long int, long int> connToLastCompletedEvent;
        std::unordered_map<long int, EventHeap*>* connToEventQueue;
	std::unordered_map<long int, long int>* connTime;
	std::unordered_map<std::string, long int>* listenerTime;
        
        EventHeap waitHeap;

        void processAcceptEvents(long int);        
        void processFileEvents();
        void addWait();
        bool readyForEvent(long int connID, long int delta, long int now);
        void dispatch(Event dispatchJob, long int now);
        void newConnectionUpdate(int sockfd, long int connID, long int planned, long int now);
	void connectionUpdate(long int connID, long int planned, long int now);
        bool acceptNewConnection(struct epoll_event *poll_e, long int now);
	void getNewEvents(long int conn_id);	
    public:
        EventHandler(std::unordered_map<long int, long int>* c2time, std::unordered_map<std::string, long int>* l2time, EventQueue* fe, EventQueue* ae, EventQueue* re, EventQueue* se, EventQueue * outserverQ, EventQueue * outSendQ, ConnectionPairMap* ConnMap, std::unordered_map<long int, EventHeap*>* c2eq);
        ~EventHandler();
        bool startup();
        void loop(std::chrono::high_resolution_clock::time_point startTime);
};

#endif
