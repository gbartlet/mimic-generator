#include "eventHandler.h"
#include "connections.h"

#define MAX_BACKLOG_PER_SRV 5

/* We start 3 threads */
/*	- a server thread (takes in start/stop req, produces accepted events.)  */
/*	  out: client/serv addr (sockfd) map: addrs->connid, add sockfd		*/
/* 	- a recv thread (produces events of how much is received from each socket.) */
/*	  out: sockfd (value) map: sockfd->connid, add value		*/
/* 	- a send thread (takes in send/connect req, produces sent event). 	*/
/* 	  out: connid (value) map: none, add value		*/
        
/* We consume these. 
EventQueue* incomingFileEvents;
EventQueue* incomingAcceptedEvents;
EventQueue* incomingRECVedEvents;
EventQueue* incomingSentEvents;
        
We produce these. 
 EventQueue* serverStartnStopReq;
 EventQueue* sendReq;
        
Data management structures. 
std::unordered_map<int, Connection*> connIDToConnectionMap;
std::unordered_map<int, int> sockfdToConnIDMap;
std::unordered_map<int, int> connToSockfdIDMap;
std::priority_queue <Event, std::vector<Event>, compareEvents> waitHeap;
std::priority_queue <Event, std::vector<Event>, compareEvents> expectedClients;
*/

void EventHandler::processAcceptedEvents() {
}
void EventHandler::processSentEvents() {
}
void EventHandler::processRECVedEvents() {
}
void EventHandler::popWaitEvents() {
}
void EventHandler::processFileEvents() {
}
void EventHandler::addWait() {
}

void EventHandler::checkDeferred(int long now) {
    std::unique_ptr<Event> e_ptr = waitHeap.nextEvent();
    while(e_ptr != nullptr) {
        std::shared_ptr<Event> e_shr = std::move(e_ptr);
        dispatch(e_shr, now);
        e_shr.reset();
        e_ptr.reset();
        e_ptr = waitHeap.nextEvent();
    }
}

bool EventHandler::readyForEvent(long int connID, long int delta, long int now) {
   
    /* Make sure we have a valid socket for this event. */
    auto it0 = connToSockfdIDMap.find(connID);
    if(it0 == connToSockfdIDMap.end()) return false;

    /* Check if we're waiting on events. */
    auto it1 = connToWaitingToRecv.find(connID);
    if(it1 != connToWaitingToRecv.end()) {
        if(it1->second > 0) return false;
    }
    
    /* Check if we're waiting to send. */
    auto it2 = connToWaitingToSend.find(connID);
    if(it2 != connToWaitingToSend.end()) {
        if(it2->second > 0) return false;
    } 
    
    /* Check on the time. */
    auto it3 = connToLastCompletedEvent.find(connID);
    if(it3 != connToLastCompletedEvent.end()) {
        if(now - it3->second < delta) return false;
    }
    
    return true;
}


void EventHandler::newConnectionUpdate(int sockfd, long int connID, long int now) {
    sockfdToConnIDMap[sockfd] = connID;
    connToSockfdIDMap[connID] = sockfd;
    connToWaitingToRecv[connID] = 0;
    connToWaitingToSend[connID] = 0;
    connToLastCompletedEvent[connID] = now;
    connToEventQueue[connID] = new EventQueue();
}


void EventHandler::dispatch(std::shared_ptr<Event> dispatchJob, long int now) {
    /* 	EventQueue* incomingFileEvents;
        --> OLD: EventQueue* incomingAcceptedEvents;
        EventQueue* incomingRECVedEvents;
        EventQueue* incomingSentEvents;
    */
    switch(dispatchJob->type) {
        /* We note these as events in our connection structure. */
        case ACCEPT: {
            // XXX We should expect a connection ---> keep track of this somewhere. 
            break;
        }
        case WAIT: {
            connToWaitingToRecv[dispatchJob->conn_id] = connToWaitingToRecv[dispatchJob->conn_id] + dispatchJob->value;
            break;
        }
        case RECV: {
            // From file events. We should dispatch this.
            break;
        }
        /* We handle the connection and update our socket<->connid maps. */
        case CONNECT: {
            /* Get our address. */
            auto it = connIDToConnectionMap->find(dispatchJob->conn_id);
            if(it != connIDToConnectionMap->end()) {
                int sockfd = getIPv4TCPSock((const struct sockaddr_in *)&(it->second->src));
                if(connect(sockfd, (struct sockaddr *)&(it->second->dst), sizeof(struct sockaddr_in)) == -1) {
                    close(sockfd);
                    perror("Connecting");
                    return;
                }
                newConnectionUpdate(sockfd, dispatchJob->conn_id, now);
            }
            else {
                std::cerr << "Could not find connection info for connID " << dispatchJob->conn_id << std::endl;
                return;
            }
            break;
        }
            
        /* Send thread handles these. */
        case SEND: {
            connToWaitingToSend[dispatchJob->conn_id] = connToWaitingToSend[dispatchJob->conn_id] + dispatchJob->value;
            if(readyForEvent(dispatchJob->conn_id, dispatchJob->ms_from_last_event, now)) {
                sendReq->addEvent(dispatchJob);
            }
            else {
                waitHeap.addEvent(*dispatchJob);
            }
            break;
        }
        /* We handle these. */
        case SRV_START: {
            printf("Starting server.");
            break;
        }
        case SRV_END: {
            printf("Stopping server.");
            break;
        }
        /* Not sure how we got here. */
        default: {
            break;
        }
    }
    //dispatchJob.reset();
}

bool EventHandler::startup() {
    for(const auto& pair:*connIDToConnectionMap) {
        long int connID = pair.first;
        bool success = false;
        std::string constring = getConnString(&(pair.second->src), &(pair.second->dst), &success);
        if(success) {
            strToConnID[constring] = connID;
            std::cout << "Adding " << constring << ":" << connID << std::endl;
            constring.clear();
        }
        else {
            std::cerr << "Problem creating connection string for server map of connIDs->connection strings." << std::endl;
        }
        constring.clear();
    }
    for(const auto& pair:strToConnID) {
        std::cout << "Conn string " << pair.first << " has id " << pair.second << std::endl;
    }
    return true;
}

void EventHandler::loop(std::chrono::high_resolution_clock::time_point startTime) {
    long int now = msSinceStart(startTime);
    long int nextEventTime = (*incomingFileEvents).nextEventTime();
    
    while(isRunning.load()) {
        std::cout << "Begining of loop." << std::endl;
        
        while(nextEventTime <= now && nextEventTime >= 0) {
            std::shared_ptr<Event> job;
            //std::cout << "Event handler TRYING TO GET JOB" << std::endl;
            if((*incomingFileEvents).getEvent(job)){
                fileEventsHandledCount++;
                
                /* Check if we've processed a fair chunk (maxQueuedFileEvents/10 events) and	*/
                /* warn the FileWorker that it should top off the file event queue. 		*/
                if(fileEventsHandledCount % (maxQueuedFileEvents/10) == 0 && lastEventCountWhenRequestingForMore != fileEventsHandledCount) {
                    lastEventCountWhenRequestingForMore = fileEventsHandledCount;
                    requestMoreFileEvents->sendSignal();
                }
                std::shared_ptr<Event> dispatchJob(job);
                //std::cout << "Event handler GOT JOB " << dispatchJob->type << std::endl;
                dispatch(dispatchJob, now);
                nextEventTime = (*incomingFileEvents).nextEventTime();
                std::cout << "EVENT HANDLER: Pulled " << fileEventsHandledCount << " events. Next event time is " << nextEventTime << std::endl;
            }
            else {
                std::cout << "We think we have a job, but failed to pull it? " << std::endl;
            }
            job.reset();
        }
        std::cout << "EVENT HANDLER: Next event time is: " << nextEventTime << " Now is " << now << std::endl; 
        
        if(fileEventsHandledCount > maxQueuedFileEvents/2) {
            fileEventsHandledCount = 0;
            std::unique_lock<std::mutex> lck(fileHandlerMTX);
            std::cout << "Sending wake to fileWorker." << std::endl;
            loadMoreFileEvents = true;
            fileHandlerCV.notify_one();
            lck.unlock();
        }
                
        std::cout << "EVENT HANDLER:  Next event time is: " << nextEventTime << " It's now: " << now << std::endl;
        popWaitEvents();
        // old --> processAcceptedEvents();
        processRECVedEvents();
        processSentEvents(); 
        now = msSinceStart(startTime);
        checkDeferred(now);
        //usleep(1000 * 1000);
        
        /* If the last time we checked the time in the events queue it was empty, redo our check now. */
        if(nextEventTime < 0)
            nextEventTime = (*incomingFileEvents).nextEventTime();
            
        // Check epoll events.
        int timeout = 0;
        if(nextEventTime - now > 0)
            timeout = nextEventTime - now;
        myPollHandler->waitForEvents(timeout);
        
        /* Handle any events from poll. Could be 			*/
        /*    - a notification from send or recv threads.		*/
        /*    - a new connection to our server socket.			*/
        struct epoll_event *poll_e = (struct epoll_event*) calloc(1, sizeof(struct epoll_event));
        while(myPollHandler->nextEvent(poll_e)) {
            // XXX Handle notifications.    
            
            /* New connection to one of our servers. */
            acceptNewConnection(poll_e, now);            
        }
        free(poll_e);
        std::cout << "Relooping" << std::endl;
    }
}       

bool EventHandler::acceptNewConnection(struct epoll_event *poll_e, long int now) {
    int newSockfd = -1;
    struct sockaddr in_addr;
    int in_addr_size = sizeof(in_addr);
    int fd = poll_e->data.fd;
    
    /* Accept new connection. */ 
    newSockfd = accept(fd, &in_addr, (socklen_t*)&in_addr_size);
    
    /* Set nonblocking. */
    int status = 0;
    status = setIPv4TCPNonBlocking(newSockfd);
    if(status < 0) {
        return false;
    }
    

    /* Now figure out which connection we accpted. */
    /* Get info on the server socket we accepted on. */
    struct sockaddr sa_srv;
    unsigned int sa_len;
    sa_len = sizeof(sa_srv);
    
    // XXX We assume this is IPv4/TCP for now.
    if(getsockname(fd, (sockaddr *)&sa_srv, (unsigned int *)&sa_len) == -1) {
        perror("getsockname() failed");
        return false;
    }
    bool success = false;
    
    // XXX We assume this is IPv4/TCP for now.
    std::string connString = getConnString((const struct sockaddr_in *)&in_addr, (const struct sockaddr_in*)&sa_srv, &success);
    if(!success) return false;
    std::cout << "Got connection from: " << connString << std::endl;

    /* Map names to a conn. */
    auto it = strToConnID.find(connString);
    if(it == strToConnID.end()) {
        std::cerr << "Got connection but could not look up connID." << std::endl;
        return false; 
    }
    
    long int conn_id = it->second;
        
    /* Update our data structures. */
    newConnectionUpdate(newSockfd, conn_id, now);
    
    /* XXX Add this to the watched sockets for reads. */
    
} 

EventHandler::EventHandler(EventNotifier* loadMoreNotifier, EventQueue* fe, EventQueue* ae, EventQueue* re, EventQueue* se, EventQueue * outserverQ, EventQueue * outSendQ, ConnectionPairMap* ConnMap) {

    fileEventsHandledCount = 0;
    lastEventCountWhenRequestingForMore = 0;

    connIDToConnectionMap = ConnMap;
    incomingFileEvents = fe;
    requestMoreFileEvents = loadMoreNotifier;
    incomingAcceptedEvents = ae;
    incomingRECVedEvents = re;
    incomingSentEvents = se;
    serverStartnStopReq = outserverQ;
    sendReq = outSendQ;
    
    sockfdToConnIDMap = {};
    connToSockfdIDMap = {};
    connToWaitingToRecv = {};
    connToWaitingToSend = {};
    connToLastCompletedEvent = {};
    
    myPollHandler = new PollHandler();
}	

EventHandler::~EventHandler() {
}


/* For printing/logging only. */
std::string Connection::dstAddr() {
    socklen_t len;
    struct sockaddr addr;
    int port;
    int PORT_MAX_LEN = 5;
    char ipstr[INET6_ADDRSTRLEN];

    len = sizeof addr;
    if(getpeername(sockfd, (struct sockaddr*)&addr, &len) == -1) {
        return std::string("");
    }

    if (addr.sa_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
    } 
    else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
    }

    std::ostringstream addrStream;
    addrStream << ipstr << ":" << port;

    std::string addrStr = addrStream.str();
    return addrStr;
}


