#include <unistd.h>
#include "connections.h"
#include "eventQueue.h"
#include "serverWorker.h"
#include "mimic.h"


ServerWorker::ServerWorker(EventQueue* in, EventQueue* out) {
    // If we want the server worker as it's own thread...
    inEvents = in;
    outEvents = out;
    srvStringToSockfd = {};
}


ServerWorker::~ServerWorker() {

}

bool ServerWorker::startup(ConnectionPairMap * cIDToCMap) {

    connIDToConnectionMap = cIDToCMap;

    if ((efd = epoll_create1(0)) == -1) {
        perror("epoll_create in ServerWorker");
        return false;
    }
    
    std::cout << "Server working performing startup." << std::endl;
    /* Load connection information */
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

bool ServerWorker::handleAccept(Event * job, long int now) {
    int newSockfd = -1;
    struct sockaddr in_addr;
    int in_addr_size = sizeof(in_addr);
    
    /* Accept and make sure new socket is nonblocking. */
    newSockfd = accept(job->sockfd, &in_addr, (socklen_t*)&in_addr_size);
    int status = 0;
    status = setIPv4TCPNonBlocking(newSockfd);
    if(status < 0) {
        return false;
    }
    /* Determine which connection this is. */
    /* Get name of server socket. */
    struct sockaddr sa_srv;
    unsigned int sa_len;
    sa_len = sizeof(sa_srv);
    // XXX We assume this is IPv4/TCP for now.
    if(getsockname((int)job->sockfd, (sockaddr *)&sa_srv, (unsigned int *)&sa_len) == -1) {
        perror("getsockname() failed");
        return false;
    }
    bool success = false;
    
    // XXX We assume this is IPv4/TCP for now.
    std::string connString = getConnString((const struct sockaddr_in *)&in_addr, (const struct sockaddr_in *)&sa_srv, &success);
    if(!success) return false;
    std::cout << "Got connection from: " << connString << std::endl;
    
    /* Map names to a conn. */
    auto it = strToConnID.find(connString);
    if(it == strToConnID.end()) {
        std::cerr << "Got connection but could not look up connID." << std::endl;
        return false;
    }
    
    /* Create job about it. */
    Event e;
    e.type = ACCEPTED;
    e.conn_id = it->second;
    e.ms_from_start = now;
    e.sockfd = newSockfd;
    outEvents->addEvent(std::make_shared<Event>(e));

    /* Update our stats. */
    sockfdToConnCount[job->sockfd] = sockfdToConnCount[job->sockfd] + 1;

    return true;
}


bool ServerWorker::handleSrvStart(Event * job) {
    auto it = connIDToConnectionMap->find(job->conn_id);
    if(it == connIDToConnectionMap->end()) {
        std::cerr << "Asked to start server for connection ID " << job->conn_id << " but this does not map to a known connection." << std::endl;
        return false;
    }
    
    /* Check if we've already started a server for this dst IP and port. */
    std::string servString = getIPPortString((const struct sockaddr_in*)&(it->second->dst));
    auto it1 = srvStringToSockfd.find(servString);
    if(it1 != srvStringToSockfd.end()) {
        std::cout << "Server already started for " << servString << std::endl;
        return true;
    }
    
    int sockfd = getIPv4TCPSock((const struct sockaddr_in*)&(it->second->dst));
    if(sockfd == -1) {
        std::cerr << "ERROR: Failed to bind to " << servString << std::endl; 
        return false;
    }
    
    if(listen(sockfd, MAX_BACKLOG_PER_SRV) == -1) {
        perror("Listen failed");
        return false;
    }
    
    /* Add to epoll. */
    static struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
    ev.data.fd = sockfd;
    int stat = epoll_ctl(efd, EPOLL_CTL_ADD, sockfd, &ev);
    if(stat != 0) {
        perror("epoll_ctr, ADD");
        return false;
    }
    
    /* Update our records. */
    srvStringToSockfd[servString] = sockfd;
    sockfdToConnCount[sockfd] = 0;    
    return true;
}

bool ServerWorker::handleSrvStop(Event * job) {

}

bool ServerWorker::handleJob(Event * job, long int now) {
    if(job == nullptr) return false;
    switch(job->type) {
        case ACCEPT: std::cout << "Got accept." << std::endl;
            return handleAccept(job, now);
                    
        case SRV_START: std::cout << "Starting server." << std::endl;
            return handleSrvStart(job); 
        
        case SRV_END: std::cout << "Bring down server." << std::endl;
        case CLOSE: std::cout << "Told to close socket." << std::endl;
            return handleSrvStop(job);
            break;
        
        default:
            break;
    }
    return true;
}

void ServerWorker::loop(std::chrono::high_resolution_clock::time_point startTime) {
    while(isRunning.load()) {
        /* Get a "now" time for our loop iteration. */
        long int now = msSinceStart(startTime);
        
        /* Check if we have any defferred jobs. */
        /*long int myJobsNextTime = myJobs.nextEventTime();
        while(myJobsNextTime <= now && myJobsNextTime >= 0) {
            std::unique_ptr<Event> job = myJobs.nextEvent();
            handleJob(job.get());
            job.reset();
            myJobsNextTime = myJobs.nextEventTime();
        }*/
    
        /* Pull events from IN queue. */
        std::shared_ptr<Event> job;
        long int nextEventTime = 0;
        nextEventTime = (*inEvents).nextEventTime();
        
        while(nextEventTime <= now && nextEventTime >= 0) {
            (*inEvents).getEvent(job);
            std::cout << "SERVER WORKER We got an event." << std::endl;
            handleJob(job.get(), now);
            //job.reset();
            nextEventTime = (*inEvents).nextEventTime();
        }
        if(nextEventTime > -1) 
            std::cout << "SERVER WORKER: Next event time: " << nextEventTime << std::endl;
        
        /* Listen for incoming events. */
         usleep(1000 * 1000);
        
        /* Handle incoming events and post to OUT queue. */ 
        
    }

    /* Iterate over our map of sockets and close them. */
    for(const auto& pair:sockfdToConnCount) {
        close(pair.first);
    }
    
    exit(0);
}
