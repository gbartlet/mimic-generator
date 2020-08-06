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

void EventHandler::processAcceptEvents(long int now) {

      std::shared_ptr<Event> job;
      //std::cout << "EH:pae: Event handler TRYING TO GET JOB" << std::endl;
      while((*incomingAcceptedEvents).getEvent(job)){
	std::shared_ptr<Event> dispatchJob(job);
	if (DEBUG)
	  std::cout << "pae: Event handler GOT JOB " << EventNames[dispatchJob->type] <<" conn "<<dispatchJob->conn_id<<" event "<<dispatchJob->event_id<<std::endl;
	dispatch(dispatchJob, now);
      }
}
void EventHandler::processSentEvents() {
  /*
  for(std::unordered_map<long int, int>::iterator it = connToWaitingToSend.begin(); it != connToWaitingToSend.end(); it++)
    {
      // Make sure we re-watch for writes
      myPollHandler->watchForWrite(connToSockfdIDMap[it->first]);
    }
  */
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

void EventHandler::newConnectionUpdate(int sockfd, long int connID, long int planned, long int now) {
  sockfdToConnIDMap[sockfd] = connID;
  connToSockfdIDMap[connID] = sockfd;
  connToWaitingToRecv[connID] = 0;
  connToWaitingToSend[connID] = 0;
  if (planned > 0)
    connToLastPlannedEvent[connID] = planned;
  else
    {
      int delay = (now - connToLastPlannedEvent[connID]);
      if (delay > 0)
	connToDelay[connID] += delay;
      connToLastPlannedEvent[connID] = now;
    }
  connToLastCompletedEvent[connID] = now;
  if (DEBUG)
    std::cout<<"Conn "<<connID<<" time now "<<now<<" planned time "<<connToLastPlannedEvent[connID]<<" delay "<< connToDelay[connID]<<std::endl;
}


void EventHandler::connectionUpdate(long int connID, long int planned, long int now) {
  if (planned > 0)
    connToLastPlannedEvent[connID] = planned;
  else
    {
      connToDelay[connID] += (now - connToLastPlannedEvent[connID]);
      connToLastPlannedEvent[connID] = now;
    }
  connToLastCompletedEvent[connID] = now;
  if (DEBUG)
    std::cout<<"EConn "<<connID<<" time now "<<now<<" planned time "<<connToLastPlannedEvent[connID]<<" delay "<< connToDelay[connID]<<std::endl;
}

#define MAXLEN 1000000

void EventHandler::dispatch(std::shared_ptr<Event> dispatchJob, long int now) {
    /* 	EventQueue* incomingFileEvents;
        --> OLD: EventQueue* incomingAcceptedEvents;
        EventQueue* incomingRECVedEvents;
        EventQueue* incomingSentEvents;
    */
  char buf[MAXLEN];
  
  if (DEBUG)
    std::cout<<"EH: dispatch job type "<<EventNames[dispatchJob->type]<<std::endl;
    switch(dispatchJob->type) {
        /* We note these as events in our connection structure. */
        case ACCEPT: {
	  newConnectionUpdate(dispatchJob->sockfd, dispatchJob->conn_id, dispatchJob->ms_from_start, now);
	  myPollHandler->watchForWrite(connToSockfdIDMap[dispatchJob->conn_id]);
	  if (DEBUG)
	    std::cout<<"PH will watch for write on "<<connToSockfdIDMap[dispatchJob->conn_id]<<" for conn "<<dispatchJob->conn_id<<std::endl;
	  break;
        }
        case WAIT: {
            break;
        }
        case RECV: {
	  connectionUpdate(dispatchJob->conn_id, dispatchJob->ms_from_start, now);
	  if (DEBUG)
	    std::cout<<"RECV JOB waiting to recv "<<connToWaitingToRecv[dispatchJob->conn_id]<<" on conn "<<dispatchJob->conn_id<<" job value "<<dispatchJob->value<<std::endl;
	   connToWaitingToRecv[dispatchJob->conn_id] = connToWaitingToRecv[dispatchJob->conn_id] + dispatchJob->value;

	   while(connToWaitingToRecv[dispatchJob->conn_id] > 0)
	     {
	       if (DEBUG)
		 std::cout<<"Waiting for "<<connToWaitingToRecv[dispatchJob->conn_id]<<std::endl;
	       int n = recv(dispatchJob->sockfd, buf, MAXLEN, 0);
	       if (n > 0)
		 {
		   if (DEBUG)
		     std::cout<<"RECVd 1 "<<n<<" bytes for conn "<<dispatchJob->conn_id<<std::endl;
		   connToWaitingToRecv[dispatchJob->conn_id] -= n;
		   if (connToWaitingToRecv[dispatchJob->conn_id] < 0) // weird case
		     connToWaitingToRecv[dispatchJob->conn_id] = 0;
		   if (DEBUG)
		     std::cout<<"RECV waiting now for "<<connToWaitingToRecv[dispatchJob->conn_id]<<" conn "<<dispatchJob->conn_id<<std::endl;
		  // Check if lower than 0 or 0 move new event ahead
		   if (connToWaitingToRecv[dispatchJob->conn_id] <= 0)
		    {
		      connectionUpdate(dispatchJob->conn_id, 0, now);
		      getNewEvents(dispatchJob->conn_id);
		      break;
		    }
		 }
	       else
		 {
		   if (DEBUG)
		     std::cout<<"Will wait to RECV "<<connToWaitingToRecv[dispatchJob->conn_id]<<" for "<<dispatchJob->conn_id<<" on sock "<<dispatchJob->sockfd<<std::endl;
		   myPollHandler->watchForRead(dispatchJob->sockfd);
		   break;
		 }
	     }
            // From file events. We should dispatch this.
            break;
        }
        /* We handle the connection and update our socket<->connid maps. */
        case CONNECT: {
            /* Get our address. */
	  connToLastPlannedEvent[dispatchJob->conn_id] = dispatchJob->ms_from_start;
            auto it = connIDToConnectionMap->find(dispatchJob->conn_id);
            if(it != connIDToConnectionMap->end()) {
	        connState[dispatchJob->conn_id] = CONNECTING;
	        int sockfd = getIPv4TCPSock((const struct sockaddr_in *)&(it->second->src));
		if (DEBUG)
		  std::cout<<"Connecting on sock "<<sockfd<<" for conn "<<dispatchJob->conn_id<<" state "<<connState[dispatchJob->conn_id]<<std::endl;
                if(connect(sockfd, (struct sockaddr *)&(it->second->dst), sizeof(struct sockaddr_in)) == -1) {
		  if (DEBUG)
		    std::cout<<"Didn't connect right away\n";
		  if (errno != EINPROGRESS)
		    {
		      close(sockfd);
		      perror("Connecting");
		      return;
		    }
		  else
		    {
		      myPollHandler->watchForWrite(sockfd);
		      newConnectionUpdate(sockfd, dispatchJob->conn_id, dispatchJob->ms_from_start, now);
		    }
		}
		else
		  {
		    connState[dispatchJob->conn_id] = EST;
		    newConnectionUpdate(sockfd, dispatchJob->conn_id, dispatchJob->ms_from_start, now);
		    if (DEBUG)
		      std::cout<<"Connected successfully 1 for conn "<<dispatchJob->conn_id<<" state is now "<<connState[dispatchJob->conn_id]<<std::endl;
		    getNewEvents(dispatchJob->conn_id);
		  }
            }
            else {
                std::cerr << "Could not find connection info for connID " << dispatchJob->conn_id << std::endl;
                return;
            }
            break;
        }
            
        /* Send thread handles these. */
    case SEND: {
      connectionUpdate(dispatchJob->conn_id, dispatchJob->ms_from_start, now);
      connToWaitingToSend[dispatchJob->conn_id] += dispatchJob->value;
      if (DEBUG)
	std::cout<<"Handling SEND event waiting to send "<<connToWaitingToSend[dispatchJob->conn_id]<<" on sock "<<dispatchJob->sockfd<<std::endl;

      // Try to send
      while (connToWaitingToSend[dispatchJob->conn_id] > 0)
	{
	  int n = send(dispatchJob->sockfd, buf, connToWaitingToSend[dispatchJob->conn_id], 0);
	  if (n < 0)
	    {
	      myPollHandler->watchForWrite(dispatchJob->sockfd);
	      break;
	    }
	  else
	    {
	      connToWaitingToSend[dispatchJob->conn_id] -= n;
	      if (DEBUG)
		std::cout<<"Successfuly handled SEND event for "<<n<<" bytes\n";
	    }
	}
      if (connToWaitingToSend[dispatchJob->conn_id] < 0)
	connToWaitingToSend[dispatchJob->conn_id] = 0; // weird case
      break;
    }
        /* We handle these. */
        case SRV_START: {
	  
	  connToLastPlannedEvent[dispatchJob->conn_id] = dispatchJob->ms_from_start;
	  // Create event and put into server queue
	  connState[dispatchJob->conn_id] = LISTENING;
	  //serverStartnStopReq->addEvent(dispatchJob);
	  auto it = connIDToConnectionMap->find(dispatchJob->conn_id);
	  if(it == connIDToConnectionMap->end()) {
	     std::cerr << "Asked to start server for connection ID " << dispatchJob->conn_id << " but this does not map to a known connection." << std::endl;
	     break;
	   }
	   std::string servString = getIPPortString((const struct sockaddr_in*)&(it->second->dst));
	   int sockfd = getIPv4TCPSock((const struct sockaddr_in*)&(it->second->dst));
	   if(sockfd == -1) {
	     std::cerr << "ERROR: Failed to bind to " << servString << std::endl;
	     return;
	   }
	   if (DEBUG)
	     std::cout<<"Update listening socket "<<sockfd<<" for conn "<<dispatchJob->conn_id<<std::endl;
	   newConnectionUpdate(sockfd, dispatchJob->conn_id, dispatchJob->ms_from_start+SRV_UPSTART, now); 
	   if(listen(sockfd, MAX_BACKLOG_PER_SRV) == -1) {
	     perror("Listen failed");
	     return;
	   }
	   myPollHandler->watchForRead(sockfd);
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
  if (DEBUG)
    std::cout<<"Event handler starting\n";
    for(const auto& pair:*connIDToConnectionMap) {
        long int connID = pair.first;
        bool success = false;
        std::string constring = getConnString(&(pair.second->src), &(pair.second->dst), &success);
        if(success) {
            strToConnID[constring] = connID;
	    if (DEBUG)
	      std::cout << "Adding " << constring << ":" << connID << std::endl;
            constring.clear();
        }
        else {
            std::cerr << "Problem creating connection string for server map of connIDs->connection strings." << std::endl;
        }
        constring.clear();
    }
    for(const auto& pair:strToConnID) {
      if (DEBUG)
        std::cout << "Conn string " << pair.first << " has id " << pair.second << std::endl;
    }
    return true;
}

void EventHandler::loop(std::chrono::high_resolution_clock::time_point startTime) {
  long int now = msSinceStart(startTime);
  // Allocate a really big buffer filled with a's
  char* buf = (char*)malloc(MAXLEN);
  memset(buf, 'a', MAXLEN);
  if (DEBUG)
  std::cout<<"EH: looping, incoming file events "<<incomingFileEvents<<"\n";
  long int nextEventTime = (*incomingFileEvents).nextEventTime();
  if (DEBUG)
  std::cout<<"EH: Is running is "<<isRunning.load()<<std::endl;
  long int start = 0, end = 0;
  int sends = 0;
  
  while(isRunning.load()) {
    
    //std::cout<<"Next event time "<<nextEventTime<<" now "<<now<<std::endl;
    //std::cout <<"EH: Beginning of loop time " <<now<<std::endl;
    while(nextEventTime <= now && nextEventTime >= 0) {
      std::shared_ptr<Event> job;
      //std::cout << "EH: Event handler TRYING TO GET JOB" << std::endl;
      if((*incomingFileEvents).getEvent(job)){
                fileEventsHandledCount++;
                
                /* Check if we've processed a fair chunk (maxQueuedFileEvents/10 events) and	*/
                /* warn the FileWorker that it should top off the file event queue. 		*/
                if(fileEventsHandledCount % (maxQueuedFileEvents/10) == 0 && lastEventCountWhenRequestingForMore != fileEventsHandledCount) {
                    lastEventCountWhenRequestingForMore = fileEventsHandledCount;
                    requestMoreFileEvents->sendSignal();
                }
                std::shared_ptr<Event> dispatchJob(job);
		if (DEBUG)
                std::cout << "File Event handler GOT JOB " << EventNames[dispatchJob->type] <<" conn "<<dispatchJob->conn_id<<" event "<<dispatchJob->event_id<<" ms from start "<<dispatchJob->ms_from_start<<" value "<<dispatchJob->value<<std::endl;
                dispatch(dispatchJob, now);
                nextEventTime = (*incomingFileEvents).nextEventTime();
                //std::cout << "EVENT HANDLER: Pulled " << fileEventsHandledCount << " events. Next event time is " << nextEventTime << std::endl;
            }
            else {
	      if (DEBUG)
                std::cout << "We think we have a job, but failed to pull it? " << std::endl;
            }
            job.reset();
        }
        //std::cout << "EVENT HANDLER: Next event time is: " << nextEventTime << " Now is " << now << std::endl; 
        
        if(fileEventsHandledCount > maxQueuedFileEvents/2) {
            fileEventsHandledCount = 0;
            std::unique_lock<std::mutex> lck(fileHandlerMTX);
	    if (DEBUG)
            std::cout << "Sending wake to fileWorker." << std::endl;
            loadMoreFileEvents = true;
            fileHandlerCV.notify_one();
            lck.unlock();
        }

	long int nextHeapEventTime = eventsToHandle->nextEventTime();
	//std::cout<<"Next heap time "<<nextHeapEventTime<<" now "<<now<<std::endl;
	

	while(nextHeapEventTime <= now && nextHeapEventTime >= 0) {
	  std::shared_ptr<Event> job = eventsToHandle->nextEvent();
	  if(true){ // this was if (bool = got a job)
                std::shared_ptr<Event> dispatchJob(job);
		if (DEBUG)
		  std::cout << "Heap Event handler GOT JOB " << EventNames[dispatchJob->type] <<" conn "<<dispatchJob->conn_id<<" event "<<dispatchJob->event_id<<" ms from start "<<dispatchJob->ms_from_start<<" value "<<dispatchJob->value<<std::endl;
		if (dispatchJob->type == SEND)
		  {
		    sends++;
		    if (start == 0)
		      {
			start = msSinceStart(startTime);		       
			std::cout<<"Starting time "<<start<<std::endl;
		      }
		  }
                dispatch(dispatchJob, now);
                nextHeapEventTime = eventsToHandle->nextEventTime();
                //std::cout << "EVENT HANDLER: Pulled " << fileEventsHandledCount << " events. Next event time is " << nextEventTime << std::endl;
            }
            else {
	      if (DEBUG)
                std::cout << "We think we have a job, but failed to pull it? " << std::endl;
            }
            job.reset();
        }
	
        //std::cout << "EVENT HANDLER:  Next event time is: " << nextEventTime << " It's now: " << now << std::endl;
        popWaitEvents();
        processAcceptEvents(now);
        processRECVedEvents();
        processSentEvents(); 
        now = msSinceStart(startTime);
        checkDeferred(now);
        //usleep(1000 * 1000);
        
        /* If the last time we checked the time in the events queue it was empty, redo our check now. */
	if (sends == 2000 && end == 0)
	  {
	    end = msSinceStart(startTime);
	    std::cout<<"Ending time "<<end<<" sends "<<sends<<std::endl;
	  }
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
	  /* Figure out what we want to do with this event */
	  int fd = poll_e->data.fd;
	  long int conn_id = sockfdToConnIDMap[fd];
	  if (DEBUG)
	    std::cout<<"Got event on sock "<<fd<<" w flags "<<poll_e->events<<" epoll in "<<EPOLLIN<<" out "<<EPOLLOUT<<" on conn "<<conn_id<<" w state "<<connState[conn_id]<<std::endl;
	  if (connState[conn_id] == LISTENING && ((poll_e->events & EPOLLIN) > 0))
	    {
	      if (DEBUG)
	      std::cout<<"EH got ACCEPT event and should accept connection\n";
	      /* New connection to one of our servers. */
	      acceptNewConnection(poll_e, now);
	      connState[conn_id] = EST;
	      if (DEBUG)
	      std::cout<<"State is now "<<connState[conn_id]<<std::endl;
	      getNewEvents(conn_id);
	    }
	  if (connState[conn_id] == CONNECTING) // && (poll_e->events & EPOLLOUT > 0))
	    {
	      // Check for error if (getsockopt (socketFD, SOL_SOCKET, SO_ERROR, &retVal, &retValLen) < 0)
	      // ERROR, fail somehow, close socket
	      //if (retVal != 0) 
	      // ERROR: connect did not "go through"
	      newConnectionUpdate(fd, conn_id, 0, now);
	      connState[conn_id] = EST;
	      if (DEBUG)
	      std::cout<<"Connected successfully, conn "<<conn_id<<" state is now "<<connState[conn_id]<<std::endl;
	      getNewEvents(conn_id);
	   }
	  if (connState[conn_id] == EST && ((poll_e->events & EPOLLOUT) > 0))
	    {
	      int len = connToWaitingToSend[conn_id];
	      if (DEBUG)
	      std::cout<<"EH possibly got SEND event for conn "<<conn_id<<" flags "<<poll_e->events<<" epollout "<<EPOLLOUT<<" comparison "<<((poll_e->events & EPOLLOUT) > 0)<<" should send "<<len<<std::endl;
	      /* New connection to one of our servers. */
	      if (len > 0)
		{
		  if (DEBUG)
		  std::cout<<"Waiting to send "<<connToWaitingToSend[conn_id]<<" on socket "<<fd<<std::endl;
		  int n = send(fd, buf, len, 0);
		  if (n > 0)
		    {
		      if (DEBUG)
		      std::cout<<"Successfully handled SEND for "<<n<<" bytes\n";
		      connToWaitingToSend[conn_id] -= n;
		      if (connToWaitingToSend[conn_id] > 0)
			{
			  if (DEBUG)
			    std::cout<<"Still have to send "<<connToWaitingToSend[conn_id]<<" bytes\n";
			  myPollHandler->watchForWrite(fd);
			}
		      else
			{
			  connectionUpdate(conn_id, 0, now);
			  getNewEvents(conn_id);
			}
		    }
		}
	    }
	  if (connState[conn_id] == EST && ((poll_e->events & EPOLLIN) > 0))
	    {
	      if (DEBUG)
		std::cout<<"Possibly handling a RECV event for "<<conn_id<<" on sock "<<fd<<std::endl;
	      int n = recv(fd, buf, MAXLEN, 0);
	      if (DEBUG)
		std::cout<<"RECVd 2 "<<n<<" bytes for conn "<<conn_id<<std::endl;
	      if (n > 0)
		{
		  connToWaitingToRecv[conn_id] -= n;
		  if (connToWaitingToRecv[conn_id] < 0) // weird case
		     connToWaitingToRecv[conn_id] = 0;
		  if (DEBUG)
		    std::cout<<"RECV waiting now for "<<connToWaitingToRecv[conn_id]<<" on conn "<<conn_id<<std::endl;
		  // Check if lower than 0 or 0 move new event ahead
		  if (connToWaitingToRecv[conn_id] <= 0)
		    {
		      connectionUpdate(conn_id, 0, now);
		      getNewEvents(conn_id);
		    }
		}
	    }
        }
        free(poll_e);
        //std::cout << "Relooping" << std::endl;
  }
}

void EventHandler::getNewEvents(long int conn_id)
{
  EventQueue* e = (*connToEventQueue)[conn_id];
  std::shared_ptr<Event> job;
  // Get only one job
  while(e->getEvent(job))
    {
      std::shared_ptr<Event> dispatchJob(job);
      job->sockfd = connToSockfdIDMap[conn_id];
      	if (DEBUG)
      std::cout << "Event handler moved new JOB " << EventNames[dispatchJob->type] <<" conn "<<dispatchJob->conn_id<<" event "<<dispatchJob->event_id<<" for time "<<job->ms_from_start<<" to send "<<job->value<<" now moved to time "<<(job->ms_from_start+connToDelay[conn_id])<<" because of delay "<<connToDelay[conn_id]<<std::endl;
      job->ms_from_start += connToDelay[conn_id];
      eventsToHandle->addEvent(*dispatchJob);
      if (job->type == RECV)
	break;
    }
  // Here we could perhaps close the connection Jelena
}

bool EventHandler::acceptNewConnection(struct epoll_event *poll_e, long int now) {
    int newSockfd = -1;
    struct sockaddr in_addr;
    int in_addr_size = sizeof(in_addr);
    int fd = poll_e->data.fd;
    
    /* Accept new connection. */ 
    newSockfd = accept(fd, &in_addr, (socklen_t*)&in_addr_size);
    if (DEBUG)
      std::cout<<"Accepted connection\n";
    /* Set nonblocking. */
    int status = 0;
    status = setIPv4TCPNonBlocking(newSockfd);
    	if (DEBUG)
    std::cout<<"EH setting nonblocking on socket "<<newSockfd<<std::endl;
    if(status < 0) {
        return false;
    }
    


    /* Now figure out which connection we accepted. */
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
    newConnectionUpdate(newSockfd, conn_id, 0, now);
    /* XXX Add this to the watched sockets for reads. */
    if (DEBUG)
    std::cout<<"Updated new sock "<<newSockfd<<" for connection "<<conn_id<<std::endl;
    return true; // Jelena    
}

EventHandler::EventHandler(EventNotifier* loadMoreNotifier, EventQueue* fe, EventQueue* ae, EventQueue* re, EventQueue* se, EventQueue * outserverQ, EventQueue * outSendQ, ConnectionPairMap* ConnMap, std::unordered_map<long int, EventQueue*>* c2eq) {

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
    connToEventQueue = c2eq;
    connState = {};
    connToWaitingToRecv = {};
    connToWaitingToSend = {};
    connToLastCompletedEvent = {};

    eventsToHandle = new EventHeap();
    myPollHandler = new PollHandler();
}	

EventHandler::~EventHandler() {
}


/* For printing/logging only. 
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

*/

