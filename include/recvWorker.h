#ifndef RECVWORKER_H
#define RECVWORKER_H 
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
#include "mimic.h"

class RecvWorker {
    private:
        /*	- a server thread (takes in start/stop req, produces accepted events.)  */
        /*	  out: client/serv addr (sockfd) map: addrs->connid, add sockfd		*/
        
        /* We consume these. */
        EventQueue* inEvents;
        
        /* We produce these. */
        EventQueue* outEvents;
                 
    public:
        RecvWorker(EventQueue* inEvents, EventQueue* outEvents);
        ~RecvWorker();
        void loop();
};

#endif
