#ifndef FILEWORKER_H
#define FILEWORKER_H 
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <string>
#include <string.h>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <vector>
#include <queue> 
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include "connections.h"
#include "eventNotifier.h"
#include "pollHandler.h"
#include "eventQueue.h"
#include "mimic.h"

#define MAX_BACKLOG_PER_SRV 5


class FileWorker {
    private:
        /* Notifier so that EventHandler can request more events be loaded. */
        EventNotifier* loadEventsNotifier;
        PollHandler* loadEventsPollHandler;
    
        long int fileEventsAddedCount;
    
        /* File worker thread - takes in files and prodcues queue of events. */
        EventQueue* outEvents;
        std::vector <std::vector <std::string>> loadFile(std::istream* infile, int numFields=3, int numRecords=-1);
        std::vector <std::vector <std::string>> loadMMapFile(void * mmapData, int numFields, int numRecords);
        bool isMyIP(std::string IP);
        bool isMyConnID(long int connID);
        void loadEvents();

        std::unordered_set<std::string> myIPs;
        std::unordered_set<long int> myConnIDs;
        std::unordered_map<long int, long int> connIDToLastEventTimeMap;
        std::unordered_map<std::string, bool> srvStringToStartBoolMap;
        EventHeap * shortTermHeap;
        
        /* Event filenames. */
        std::vector<std::string>::iterator currentEventFile;
        std::vector<std::string> eventsFiles;
        
        /* Event IF Stream. */
        std::ifstream currentEventIFStream;
        std::vector<std::ifstream*>::iterator eventIFStreamsItr;
        std::vector<std::ifstream*> eventsIFStreams;
        
        /* Event MMaps (if we're using mmap, bool useMMap). */
        std::vector<void *> mmapedFiles;
        std::vector<void *>::iterator mmappedFilesItr;
        std::unordered_map<void *, int> mmapToSize;
        
        std::string connectionFile;
        std::string IPListFile;
        std::string trim(const std::string& str, const std::string& whitespace = " \t");
        int loopedCount = 0;
        long int loopDuration = 0;
        long int loopEventCount = 0;
        long int lastEventTime = 0;
        
        bool useMMap;

    public:
        FileWorker(EventNotifier* loadMoreNotifier, EventQueue* out, std::string& ipFile, std::string& connFile, std::vector<std::string>& eFiles, bool useMMap=true);
        ~FileWorker();
        bool startup();
        void loop(std::chrono::high_resolution_clock::time_point startTime);
        ConnectionPairMap connIDToConnectionPairMap; 
        ConnectionPairMap * getConnectionPairMap();
};

#endif
