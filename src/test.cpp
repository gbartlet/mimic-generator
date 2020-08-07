#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include "mimic.h"
#include "eventQueue.h"
#include "eventHandler.h"
#include "fileWorker.h"
#include "serverWorker.h"
#include "connections.h"
#define START_PORT 5000

std::atomic<bool> isRunning = false;
std::mutex fileHandlerMTX;
std::condition_variable fileHandlerCV;
bool loadMoreFileEvents = true;

void serverSocketsThread(std::string serverIP, int numConns, EventQueue* eq) {
    int sockets[numConns]; 
    int efd;
    

    if ((efd = epoll_create1(0)) == -1) {
        perror("epoll_create");
        exit(-1);
    }

    for(int i=0; i<numConns; i++) {
        
        /* Get non-blocking socket. */
        sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        int status = fcntl(sockets[i], F_SETFL, fcntl(sockets[i], F_GETFL, 0) | O_NONBLOCK);
        if(status == -1) {
            std::cerr << "Had trouble getting non-blocking socket." << std::endl;
            exit(-1);
        }
        
        /* Bind. */
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        inet_pton(AF_INET, serverIP.c_str(), &(sa.sin_addr));
        sa.sin_port = htons(START_PORT + i);
        std::cout << "Binding to port " << START_PORT + i << std::endl;
        if(bind(sockets[i], (struct sockaddr *)&sa, sizeof(sa)) <0) {
            perror("bind failed.");
            exit(-1);
        }
        
        /* Listen. */
        if(listen(sockets[i], 3) == -1) {
            perror("listen failed.");
            exit(-1);
        }

        /* Add to epoll. */
        static struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
        ev.data.fd = sockets[i];
        int stat = epoll_ctl(efd, EPOLL_CTL_ADD, sockets[i], &ev);
        if(stat != 0) {
            perror("epoll_ctr, ADD");
            exit(-1);
        }    
    }

    /* Event loop. */
    int MAX_EPOLL_EVENTS_PER_RUN = numConns*2;
    struct epoll_event *events;
    events = (struct epoll_event *)calloc(MAX_EPOLL_EVENTS_PER_RUN, sizeof(struct epoll_event));
    if(!events) {
        perror("Calloc epoll events.");
        exit(-1);
    }
    while(isRunning.load()) {
        int nfds = epoll_wait(efd, events, MAX_EPOLL_EVENTS_PER_RUN, 5000);
        if (nfds < 0){
            perror("Error in epoll_wait!");
            exit(-1);
        }        
        for(int i = 0; i < nfds; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                /* There was an error. */
                std::cerr << "There is an error with a listening socket." << std::endl;
	    }
	    else {
                int fd = events[i].data.fd;
                Event e;
                e.ms_from_start = 0;
                struct sockaddr in_addr;
                int in_addr_size = sizeof(in_addr);
                e.sockfd = accept(fd, &in_addr, (socklen_t*)&in_addr_size);
                std::cout << "Accepted client." << std::endl;
                int status = fcntl(e.sockfd, F_SETFL, fcntl(e.sockfd, F_GETFL, 0) | O_NONBLOCK);
                /* Add this as a socket event to watch. */
                if(e.sockfd != -1 && status != -1) (*eq).addEvent(std::make_shared<Event>(e));
            }
        }
    }
    std::cout << "Server thread quitting." << std::endl;
    for(int i=0; i<numConns; i++) { 
        close(sockets[i]);
    } 
    free(events);
}

int readFromSocket(int sockfd, bool* done) {
    int count = 0;
    *done = false;
    std::cout << std::endl << "In read:" << std::endl;
    while(1) {
        int c;
        char buf[10];
        bzero(buf, sizeof(buf));
        c = read(sockfd, buf, sizeof buf);
        std::cout << "RECVD: " << buf << std::endl;
        if(c == -1) {
            if(errno == EAGAIN) {
                return count;
            }
            *done = true;
            return count;
        }
        else if (c == 0) {
            /* End of file. */
            *done = true;
            return count;
        }
        count = count + c;
        std::cout << "Looping in recv." << std::endl;
    }
}

int writeToSocket(int sockfd, bool* done, int count) {
    *done = false;
    return(send(sockfd, malloc(count * sizeof(char *)), count, 0));
}

void connectionHandlerThread(int numConns, EventQueue* eq) {
    std::shared_ptr<Event> job;
    int efd;

    if ((efd = epoll_create1(0)) == -1) {
        perror("epoll_create");
        exit(-1);
    }

    int MAX_EPOLL_EVENTS_PER_RUN = numConns*2;
    struct epoll_event *events;
    events = (struct epoll_event *)calloc(MAX_EPOLL_EVENTS_PER_RUN, sizeof(struct epoll_event));

    while(isRunning.load()) {
        /* Grab any accepted sockets. */
        while((*eq).getEvent(job)) {
            std::cout << "Second thread received new socket." << std::endl;
            static struct epoll_event ev;
            
            ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
            ev.data.fd = job->sockfd;
            int stat = epoll_ctl(efd, EPOLL_CTL_ADD, job->sockfd, &ev);
            job.reset();
        }
        
        /* Wait for reads. */
        int nfds = epoll_wait(efd, events, MAX_EPOLL_EVENTS_PER_RUN, 0);
        if (nfds < 0){
            perror("Error in epoll_wait!");
            exit(-1);
        }
        for(int i = 0; i < nfds; i++) {
            std::cout << "Got " << nfds << " events." << std::endl;
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                /* There was an error. */
            }
            else if(events[i].events & EPOLLIN) {
                bool done = true;
                int count = 0;
                count = readFromSocket(events[i].data.fd, &done);
            }
            else if(events[i].events & EPOLLOUT) {
                bool done = true;
                int count = 5000;
                count = writeToSocket(events[i].data.fd, &done, count);
            }
        }
    }   
    std::cout << "Read/write thread quitting." << std::endl;     
}

int main(int argc, char* argv[]) {
    std::string serverIP = "10.1.1.2";
    int numThreads = 1;
    int numConns = 1000;
    bool isServer = false;
    bool roleFlag = false;
    
    std::string ipFile = "";
    std::string connFile = "";
    
    std::string eventFile = "";
    
    for(int i=1; i<argc; ++i) {
      std::string arg = argv[i];
        
        /* We have an option. */
      if((arg.starts_with("-"))) {
            if(i+1 < argc) {
                if((arg == "-t")) {
                    try {
                        if(arg == "-t") {
                            numThreads = std::stoi(argv[i+1]);
                            i++;
                        }
                    }
                    catch(std::invalid_argument& e) {
                        std::cerr << "Failed to convert argument for " << arg  << std::endl;
                        exit(-1);
                    }
                }
                else if(arg == "-s") {
                    serverIP = argv[i+1];
                    i++;
                }
                else if(arg == "-i") {
                    ipFile = argv[i+1];
                    i++;
                }
                else if(arg == "-c") {
                    connFile = argv[i+1];
                    i++;
                }
		else if(arg == "-e") {
                    eventFile = argv[i+1];
                    i++;
                }
            }
            else {
                std::cerr << arg << " requires argument." << std::endl;
                exit(-1);
            }
        }
        
        /* Arg tells us what role we should play. (
        else if((arg == "-C") || (arg == "-S")) {
            if(roleFlag) {
                std::cerr << "Given both -C and -S: choose one roll Server (-S) or Client (-C)" << std::endl;
                exit(-1);
            }
            roleFlag = true;
            if(arg == "-S") isServer = true;
        }*/
        
        /* We don't recognize this argument. */
        else {
	  std::cerr << "Usage: " << argv[0] << " {-i IPFile} {-c connFile} {-e eventFile}  wrong arg " <<arg<<std::endl;
        }
    }
    
    if(ipFile == "") {
        std::cerr << "We need an IPFile argument." << std::endl;
        exit(-1);
    }
        
    // Testing File Worker
    // Event notifier & poll for FileWorker.
    int notifierFD = createEventFD();
    EventNotifier* loadMoreNotifier = new EventNotifier(notifierFD, "Test file notifier.");
    EventQueue * fileQ = new EventQueue("File events.");
    EventQueue * fileQ2 = new EventQueue("File events 2.");

    notifierFD = createEventFD();
    EventNotifier * acceptNotifier = new EventNotifier(notifierFD, "Test accept notifier.");
    EventQueue * acceptQ = new EventQueue("Accept events");
    
    notifierFD = createEventFD();
    EventNotifier * receivedNotifier = new EventNotifier(notifierFD, "Test received notifier.");
    EventQueue * recvQ = new EventQueue("Received events");
    EventQueue * sentQ = new EventQueue("Sent events");
    EventQueue * serverQ = new EventQueue("Sever start/stop events");
    EventQueue * sendQ = new EventQueue("Send events.");
    std::unordered_map<long int, EventHeap*> c2eq;
    std::unordered_map<long int, EventHeap*> c2eq2;

    std::cout<<"Conn file "<<connFile<<" event file "<<eventFile<<std::endl;
    //std::string ipFile = "/users/gbartlet/mimic-generator/testFiles/b-ips.txt";
    //connFile = "testconn.csv";
    //std::string connFile2 = "testconn2.csv";
    std::vector<std::string> eFiles, eFiles2;
    eventFile = "evconn.csv";
    eFiles.push_back(eventFile);
    //eventFile = "events2.csv";
    //eFiles2.push_back(eventFile);
    
    
    FileWorker* fw = new FileWorker(loadMoreNotifier, fileQ, acceptQ, &c2eq, ipFile, connFile, eFiles);
    fw->startup();
    ConnectionPairMap * ConnIDtoConnectionPairMap = fw->getConnectionPairMap();
    //FileWorker* fw2 = new FileWorker(loadMoreNotifier, fileQ2, acceptQ, &c2eq2, ipFile, connFile2, eFiles2);
    //fw2->startup();
    //ConnectionPairMap * ConnIDtoConnectionPairMap2 = fw2->getConnectionPairMap();

    EventHandler* eh = new EventHandler(loadMoreNotifier, fileQ, acceptQ, recvQ, sentQ, serverQ, sendQ, ConnIDtoConnectionPairMap, &c2eq);
    eh->startup();
    //EventHandler* eh2 = new EventHandler(loadMoreNotifier, fileQ2, acceptQ, recvQ, sentQ, serverQ, sendQ, ConnIDtoConnectionPairMap2, &c2eq2);
    //eh2->startup();
    
    //ServerWorker* sw = new ServerWorker(serverQ, acceptQ);
    //sw->startup(ConnIDtoConnectionPairMap);
    
    isRunning.store(true);    
    
    // Start all our threads.
    std::chrono::high_resolution_clock::time_point startPoint = std::chrono::high_resolution_clock::now();
    /* File worker. */
    std::thread fileWorkerThread(&FileWorker::loop, fw, startPoint);
    //std::thread fileWorkerThread2(&FileWorker::loop, fw2, startPoint);
    
    /* Server Woker. */
    //std::thread serverWorkerThread(&ServerWorker::loop, sw, startPoint);                     
        
    /* Event Handler. */
    std::thread eventHandlerThread(&EventHandler::loop, eh, startPoint);
    //std::thread eventHandlerThread2(&EventHandler::loop, eh2, startPoint);
    
    usleep(10000000 * 10);
    
    isRunning.store(false);
    fileWorkerThread.join();
    //fileWorkerThread2.join();
    //serverWorkerThread.join();
    eventHandlerThread.join();
    //eventHandlerThread2.join();
    EventQueue* eq = new EventQueue();
    std::thread connThread(connectionHandlerThread,numConns, sendQ);
    connThread.join();
    exit(0);






    // Testing server worker.
    EventQueue * in = new EventQueue();
    EventQueue * out = new EventQueue();
    stringToConnIDMap map;

    ServerWorker* sh = new ServerWorker(in, out);
    
    
    isRunning.store(true);
    std::thread serverThread(&ServerWorker::loop, sh, startPoint);
    Event e;
    e.ms_from_start = 20;
    e.type = SRV_START;
    in->addEvent(std::make_shared<Event>(e));
    usleep(1000000 * 10);
    isRunning.store(false);
    serverThread.join();
    exit(1);

    // test connection map.
    /*std::unordered_map<long int, connectionPair*> connIDToConnectionPairMap = {};
    std::unordered_map<std::string, long int> stringToConnID = {};
    connectionPair c = connectionPair("10.1.1.2", 85, "10.1.1.0", 55);
    connectionPair a = connectionPair("10.1.1.2", 85, "10.1.1.0", 55);
    
    if(a==c) {
        std::cout << "Connection pair a and connection pair c are the same." << std::endl;
    }
     
    bool x=true;
    
    connIDToConnectionPairMap[0] = &c;
    stringToConnID[getConnString(&(c.src), &(c.dst), &x)] = 352;

    std::string str = getConnString(&(c.src), &(c.dst), &x);
    try {
        std::cout << "The c conn id is " << stringToConnID.at("bob") << std::endl;
    }
    catch(std::out_of_range) {
        std::cout << "Caught exception." << std::endl;
    }

    exit(1);

    */
    int connsPerThread = int(numConns/numThreads);
    if(isServer) std::cout << "Running as server (binding to " << serverIP << ") ";
    else std::cout << "Running as client (connecting to " << serverIP << ") ";
    std::cout << numThreads << " threads. " << numConns << " connections. " << std::endl;
    std::cout << "Running " << connsPerThread << " connections per thread." << std::endl;

    isRunning.store(true);
    if(isServer) {
        EventQueue* eq = new EventQueue();
        std::thread serverThread(serverSocketsThread,serverIP, numConns, eq);
        std::thread connThread(connectionHandlerThread,numConns, eq);
        //serverThread.detach();
        //connThread.detach();
        usleep(1000000 * 60);
        std::cout << "Timing out and quitting." << std::endl;
        isRunning.store(false);
        serverThread.join();
        connThread.join();
    }

}
