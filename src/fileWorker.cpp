#include "connections.h"
#include "eventQueue.h"
#include "fileWorker.h"
#include "mimic.h"


FileWorker::FileWorker(EventNotifier* loadMoreNotifier, EventQueue* out, EventQueue* accept, std::unordered_map<long int, EventHeap*>* c2eq, std::string& ipFile, std::string& connFile, std::vector<std::string>& eFiles, bool useMMapFlag) {
  
    fileEventsAddedCount = 0;
    useMMap = useMMapFlag;
    ConnectionEQ = c2eq;
    
    /* Deal with our notifier where the EventHandler can prompt us to load more events. */
    loadEventsNotifier = loadMoreNotifier;
    loadEventsPollHandler = new PollHandler();
    //loadEventsPollHandler->watchForRead(loadMoreNotifier->myFD());
    
    /* Get a shortterm heap so we can internally reorder connection start/stop events with events from event files. */
    shortTermHeap = new EventHeap();
    
    /* Queue of events for the EventHandler. */
    outEvents = out;
    acceptEvents = accept;
    IPListFile = ipFile;
    connectionFile = connFile;
    eventsFiles = eFiles;


    /* Open our events files. */    
    for(currentEventFile = eventsFiles.begin(); currentEventFile != eventsFiles.end(); ++currentEventFile) {
        try {
            std::ifstream* f = new std::ifstream(*currentEventFile, std::ios::in);
            eventsIFStreams.push_back(f);
        }
        catch(std::ios_base::failure& e) {
            std::cerr << e.what() << std::endl;
        }
    }
    
    /* If we should use mmap, mmap our files now. */
    if(useMMap) {
        for(currentEventFile = eventsFiles.begin(); currentEventFile != eventsFiles.end(); ++currentEventFile) {
           /* Get the file size. */
           struct stat st;
           stat((*currentEventFile).c_str(), &st);
           size_t filesize = st.st_size;
           
           int filefd = open((*currentEventFile).c_str(), O_RDONLY, 0);
           assert(filefd != -1);
           
           void* mmappedData = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE | MAP_POPULATE, filefd, 0);
           assert(mmappedData != MAP_FAILED);
           
           mmapedFiles.push_back(mmappedData);
           mmapToSize[mmappedData] = filesize;
           
           madvise(mmappedData, filesize, POSIX_MADV_SEQUENTIAL);
        }    
    }

    eventIFStreamsItr = eventsIFStreams.begin();
    mmappedFilesItr = mmapedFiles.begin();
    currentEventFile = eventsFiles.begin();
}    

FileWorker::~FileWorker() {
    for(eventIFStreamsItr = eventsIFStreams.begin(); eventIFStreamsItr != eventsIFStreams.end(); ++eventIFStreamsItr) {
        (*eventIFStreamsItr)->close();
    }
}

std::string FileWorker::trim(const std::string& str, const std::string& whitespace) {
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return "";
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

ConnectionPairMap * FileWorker::getConnectionPairMap() {
    return &connIDToConnectionPairMap;
}

/* Potentially may want to move to an mmapping strategy. */
std::vector <std::vector <std::string>> FileWorker::loadFile(std::istream* infile, int numFields, int numRecords) {

    std::vector <std::vector <std::string>> data;
    
    int i = 0;
    bool noLimit = false;
    if(numRecords <= 0) noLimit = true;

    while(infile->good()) {	
        std::string s;
        if(!std::getline(*infile, s)) break;
        std::istringstream ss(s);
        std::vector <std::string> record;
        
        while(ss) {
            
            std::string s;
            if(!std::getline(ss, s, ',')) break;
            record.push_back(trim(s));

        }
        if(record.size() == numFields) {
            data.push_back(record);
        }
        i = i+1;
        if(i >= numRecords && !noLimit) break;
    }
    return(data);
}

std::vector <std::vector <std::string>> FileWorker::loadMMapFile(void * mmapData, int numFields, int numRecords) {
    std::vector <std::vector <std::string>> data;
    
    int i = 0;
    bool noLimit = false;
    if(numRecords <= 0) noLimit = true;
    
    char *buff_end = (char *) mmapData + mmapToSize[mmapData];
    char *begin = (char *)mmapData, *end = NULL;
    
    while((end = static_cast<char*>(memchr(begin,'\n',static_cast<size_t>(buff_end-begin)))) != NULL) {
        std::vector <std::string> record;
        std::string bufPart;
        bufPart.assign(begin,end);
        std::istringstream ss(bufPart);
        
        while(ss) {
            std::string s;
            if(!std::getline(ss, s, ',')) break;
            record.push_back(trim(s));
        }
        
        if(record.size() == numFields) {
            data.push_back(record);
        }
        else {
	  std::cerr << "ERROR: Not enough fields in line to process: " <<record.size()  << std::endl;
        }
        i = i + 1;
        if(i >= numRecords && !noLimit) break;

        if(end != buff_end) {
            begin = end+1;
        }
        else break;
    }
    //std::cout<<"Loaded "<<i<<" records\n";
    return data;
}


bool FileWorker::isMyIP(std::string IP) {
    std::unordered_set<std::string>::const_iterator got = myIPs.find(IP);
    
    if(got == myIPs.end()) return false;
    return true;
}

bool FileWorker::isMyConnID(long int connID) {
    std::unordered_set<long int>::const_iterator got = myConnIDs.find(connID);
    
    if(got == myConnIDs.end()) return false;
    return true;
}

void FileWorker::loadEvents() {
  if (DEBUG)
    std::cout << "Loading events." << std::endl;
    
    int eventsProduced = 0;
    int eventsToGet = maxQueuedFileEvents;
    
    std::vector <std::vector <std::string>> eventData; 
    if(useMMap) {
        if (DEBUG)
	  std::cout << "Using MMAP" << std::endl;
        eventData = loadMMapFile(*mmappedFilesItr, 8, eventsToGet);
    }
    else {
        //std::vector <std::vector <std::string>> eventData = loadFile(currentEventIFStream, 8, eventsToGet);
        eventData = loadFile(*eventIFStreamsItr, 8, eventsToGet);
    }
    
    /* We've probably reached the end of the file. */
    eventIFStreamsItr++;
    mmappedFilesItr++;
    if(eventIFStreamsItr >= eventsIFStreams.end() || mmappedFilesItr >= mmapedFiles.end()) {
      for(eventIFStreamsItr = eventsIFStreams.begin(); eventIFStreamsItr != eventsIFStreams.end(); ++eventIFStreamsItr) {
	(*eventIFStreamsItr)->clear();
	(*eventIFStreamsItr)->seekg(0, std::ios::beg);
      }
      eventIFStreamsItr = eventsIFStreams.begin();
      mmappedFilesItr = mmapedFiles.begin();
      if(loopedCount == 0) {
	/* This is the first time we've looped, record the duration. */
	loopDuration = lastEventTime;
      }
      loopedCount = loopedCount + 1;
      
      for(std::vector<int>::size_type i = 0; i != eventData.size(); i++) {
	// Check if this is CONN record or event record
	if (eventData[i][0] == "CONN")
	  {
	    long int connID;
	    try {
	      connID = std::stol(eventData[i][2].c_str());
	    }
	    catch(...){
	      perror("Problem with connData line, continuing.");
	      continue;
	    }
	    std::string src = trim(eventData[i][3]);
	    int sport = std::stoi(eventData[i][4].c_str());
	    std::string dst = trim(eventData[i][6]);
	    int dport = std::stoi(eventData[i][7].c_str());
	    if (DEBUG)
	      std::cout << "Check if IP '" << src << "' and '" << dst << "' are in my connections." << std::endl;
	    if(isMyIP(src) || isMyIP(dst)) {
	      /* Add this connid to our ids.*/
	      if (DEBUG)
		std::cout << "Adding " << connID << " to my connection ids." << std::endl;
	      myConnIDs.insert(connID);
	      
	      /* Fill out connIDToConnectionPairMap */
	      connectionPair cp = connectionPair(src, sport, dst, dport);
	      connIDToConnectionPairMap[connID] = std::make_shared<connectionPair>(cp);
	      
	      /* Add an event to start this connection. */
	      Event e;
	      e.conn_id = connID;
	      e.event_id = -1;
	      e.value = -1;
	      e.ms_from_start = 0;
	      e.ms_from_last_event = 0;
	      if(isMyIP(src)) {
                e.ms_from_start = stod(eventData[i][1])*1000;
                e.type = CONNECT;
		if (DEBUG)
		  std::cout<<"Adding connect event for conn "<<e.conn_id<<"\n";
		(*ConnectionEQ)[e.conn_id] = new EventHeap();
	      }
	      else {
                /* XXX Have we started a server for this IP:port yet? If not, add event. */
		e.ms_from_start = std::max((long int)(std::stod(eventData[i][1].c_str()) * 1000 - SRV_UPSTART), (long int) 0);
		e.type = SRV_START;
		if (DEBUG)
		  std::cout<<"Adding server event for conn "<<e.conn_id<<"\n";
		(*ConnectionEQ)[e.conn_id] = new EventHeap();
	      }
	      shortTermHeap->addEvent(e);
	    }
	    src.clear();
	    dst.clear();
	  }
	else if(eventData[i][0] == "EVENT")
	  {
	    //std::cout<<"Check conn id "<<eventData[i][1].c_str()<<std::endl;
	    if(isMyConnID(std::stol(eventData[i][1].c_str()))) {
	      //std::cout<<"My conn\n";
	      Event e;
	      e.conn_id = std::stol(eventData[i][1].c_str());
	      e.event_id = std::stol(eventData[i][2].c_str());      
	      e.value = std::stoi(eventData[i][5].c_str()); 
	      e.ms_from_last_event = (long int)(std::stod(eventData[i][6].c_str()) * 1000);
	      e.ms_from_start = (long int)(std::stod(eventData[i][7].c_str()) * 1000) + loopedCount * loopDuration;
	      
	      /* Type of event - send and recieve. */
	      if (DEBUG)
		std::cout<<"Check ip "<<eventData[i][3]<<" type "<<eventData[i][3]<<std::endl;
	      if(isMyIP(eventData[i][3])) {
		//std::cout<<"My ip\n";
		if(eventData[i][4].compare("SEND")==0) e.type = SEND;
		else e.type = RECV;
		//std::cout<<"Data type "<<e.type<<std::endl;
		//else {
		//    if(eventData[i][3].compare("SEND")==0) e.type = RECV;
		//    else e.type = WAIT;
		//}
		//std::cout << "Have event with time of " << e.ms_from_start << std::endl;
		
		if (DEBUG)
		  std::cout<<"Event for conn "<<e.conn_id<<" event id "<<e.event_id<<" type "<<EventNames[e.type]<<" value "<<e.value<<std::endl;
		(*ConnectionEQ)[e.conn_id]->addEvent(e);
		//shortTermHeap->addEvent(e);
		eventsProduced = eventsProduced + 1;
	      }
	      lastEventTime = std::stod(eventData[i][7].c_str()) * 1000 + loopedCount * loopDuration;
	    }
	  }
      }
    }
    if (DEBUG)
      std::cout << "Loaded " << eventsProduced << " events from file"<<std::endl;
    shortTermHeap->print();
}

bool FileWorker::startup() {
    if (DEBUG)
      std::cout<<"File worker starting\n";
    /* Check that we can read our connection, IP and events files. */

    /* Load our IPs. */
    myIPs = {};
    std::ifstream infile;
    try {
        infile.open(IPListFile.c_str(), std::ios::in);
    }
    catch (std::ios_base::failure& e) {
        std::cerr << e.what() << std::endl;
    }
    std::vector <std::vector <std::string>> ipData = loadFile(&infile, 1, -1);
    for(std::vector<int>::size_type i = 0; i != ipData.size(); i++) {
        if (DEBUG)
	  std::cout << "IP in file: '" << ipData[i][0] <<"'"<< std::endl;
        std::string ip = trim(ipData[i][0]);
        myIPs.insert(ip);
    }    
    infile.close();
    
    /* Load our connections. 
    myConnIDs = {};
    connIDToConnectionPairMap = {};
    try {
        infile.open(connectionFile.c_str(), std::ios::in);
    }
    catch(std::ios_base::failure& e) {
        std::cerr << e.what() << std::endl;
    }
    std::vector <std::vector <std::string>> connData = loadFile(&infile, 8, -1);
    if (DEBUG)
      std::cout<<"Conn data size "<<connData.size()<<std::endl;
    for(std::vector<int>::size_type i = 0; i != connData.size(); i++) {
        long int connID;
        try {
            connID = std::stol(connData[i][2].c_str());
        }
        catch(...){
            perror("Problem with connData line, continuing.");
            continue;
        }
        std::string src = trim(connData[i][3]);
        int sport = std::stoi(connData[i][4].c_str());
        std::string dst = trim(connData[i][6]);
        int dport = std::stoi(connData[i][7].c_str());
	if (DEBUG)
	  std::cout << "Check if IP '" << src << "' and '" << dst << "' are in my connections." << std::endl;
        if(isMyIP(src) || isMyIP(dst)) {
            // Add this connid to our ids.
	    if (DEBUG)
	      std::cout << "Adding " << connID << " to my connection ids." << std::endl;
            myConnIDs.insert(connID);
            
            // Fill out connIDToConnectionPairMap 
            connectionPair cp = connectionPair(src, sport, dst, dport);
            connIDToConnectionPairMap[connID] = std::make_shared<connectionPair>(cp);
            
            // Add an event to start this connection. 
            Event e;
            e.conn_id = connID;
            e.event_id = -1;
            e.value = -1;
            e.ms_from_start = 0;
            e.ms_from_last_event = 0;
            if(isMyIP(src)) {
                e.ms_from_start = stod(connData[i][1])*1000;
                e.type = CONNECT;
		  if (DEBUG)
		    std::cout<<"Adding connect event for conn "<<e.conn_id<<"\n";
		(*ConnectionEQ)[e.conn_id] = new EventHeap();
            }
            else {
          
	      e.ms_from_start = std::max((long int)(std::stod(connData[i][1].c_str()) * 1000 - SRV_UPSTART), (long int) 0);
	      e.type = SRV_START;
	      if (DEBUG)
		std::cout<<"Adding server event for conn "<<e.conn_id<<"\n";
	      (*ConnectionEQ)[e.conn_id] = new EventHeap();
            }
	    shortTermHeap->addEvent(e);                                                     
        }
        src.clear();
        dst.clear();
    }
    infile.close();

    */
    /* Set ourselves up for the first event file.*/
    /* XXX Should check if our event files are time ordered. */
    loadEvents();
    return true;
}

void FileWorker::loop(std::chrono::high_resolution_clock::time_point startTime) {
    long int nextET = -1;
    if (DEBUG)
      std::cout<<"FW looping heap has "<<shortTermHeap->getLength()<<" events\n";
    shortTermHeap->print();
    while(isRunning.load()) {
        nextET = shortTermHeap->nextEventTime();
	if (DEBUG)
	  std::cout << "Pulling from our heap, next event time in heap is: " << nextET << " Last event time: " << lastEventTime << std::endl;
        while(nextET <= lastEventTime && nextET > -1) {
	  Event e = shortTermHeap->nextEvent();
	  if (DEBUG)
	    std::cout << "Have event to add of type " << EventNames[e.type] <<" time "<<nextET<<std::endl;
	  if (DEBUG)
	    std::cout << "Adding event with time: " << shortTermHeap->nextEventTime() << " time of last event added " << lastEventTime <<  std::endl;
	  std::shared_ptr<Event> e_shr = std::make_shared<Event>(e);
	  outEvents->addEvent(e_shr);
	  e_shr.reset();
	  fileEventsAddedCount++;
	  if(fileEventsAddedCount > maxQueuedFileEvents) {
	    fileEventsAddedCount = 0;
	    break;
	  }
	  nextET = shortTermHeap->nextEventTime();
        }
        
        /* Maybe we should give it a rest for a bit. */
        loadEventsPollHandler->waitForEvents();
        
        struct epoll_event e;
        if(isRunning.load() && loadEventsPollHandler->nextEvent(&e)) {
	  if (DEBUG)
            std::cout << "Got notification to load more events." << std::endl;
            while(loadEventsPollHandler->nextEvent(&e)) {
	      if (DEBUG)
                std::cout << "Got load notification from loadEventsNotifier." << std::endl;
                loadEventsNotifier->readSignal();
            }
            loadEvents();
        }
    }     
}

