#ifndef SENDWORKER_H
#define SENDWORKER_H 
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

#define MAX_BACKLOG_PER_SRV 5

class SendWorker {
    private:
        /* We consume these. */
        EventQueue* inEvents;
        
        /* We produce these. */
        EventQueue* outEvents;
                 
    public:
        SendWorker(EventQueue* inEvents, EventQueue* outEvents);
        ~SendWorker();
        void loop();
};

#endif
