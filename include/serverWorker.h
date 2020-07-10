#ifndef SERVERWORKER_H
#define SERVERWORKER_H 
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
#include "connections.h"
#include "eventQueue.h"
#include "mimic.h"

#define MAX_BACKLOG_PER_SRV 5

class ServerWorker {
    private:
        /*	- a server thread (takes in start/stop req, produces accepted events.)  */
        /*	  out: client/serv addr (sockfd) map: addrs->connid, add sockfd		*/
        EventQueue* inEvents; 
        EventQueue* outEvents;
        stringToConnIDMap strToConnID;
        std::unordered_map<int, long int> sockfdToConnCount;
        std::unordered_map<std::string, int> srvStringToSockfd;
        ConnectionPairMap * connIDToConnectionMap;
        EventHeap myJobs;
        int efd;
        bool handleJob(Event *job, long int now);
        bool handleAccept(Event * job, long int now);
        bool handleSrvStart(Event * job); 
        bool handleSrvStop(Event * job);

    public:
        ServerWorker(EventQueue* inEvents, EventQueue* outEvents);
        ~ServerWorker();
        bool startup(ConnectionPairMap * connIDToConnectionMap);
        void loop(std::chrono::high_resolution_clock::time_point startTime);
};

#endif
