#include "performanceTests.h"
#include <sys/stat.h>
#include <iostream>
#include <iomanip>

enum Role {SERVER,CLIENT};

void usage(char* progName) {
  std::cout << progName << "[-F|-C|-E|-r {server | client}|-c CONNCOUNT |-e EVENTCOUNT|-s MAXSIZESEND]" << std::endl
       << "\t -h \t prints this message." << std::endl 
       << "\t [-F|-C|-E] \t Which tests to perform. Default = all" << std::endl
       << "\t\t -F \t Perform file event test: Create files and read events. " << std::endl
       << "\t\t -C \t Perform connection test: Create servers and connectione events. " << std::endl
       << "\t\t -E \t Perform full event test: Like -C, but adds sends and receives events. " << std::endl
       << "\t [-r|-c|-e|-s] \t Options For connnection and event tests." << std::endl
       << "\t\t -r {client|server} IP \t Role to play, and IP of other player. All tests are client/server paired" << std::endl 
       << "\t\t -c CONNCOUNT \t Number of simultaneous connections to manage " << std::endl
       << "\t\t -e EVENTCOUNT \t Number of simultaneous events to manage " << std::endl
       << "\t\t -s MAXSIZESEND \t Size of send/receive chunks (only used in -E tests). " << std::endl
       << std::endl;
       exit(1);
}

void eventHandlerTest(std::string remoteHost, Role myRole) {
  int notifierFD = createEventFD();
  EventNotifier* loadMoreNotifier = new EventNotifier(notifierFD, "Test file notifier.");
  EventQueue * acceptQ = new EventQueue("Accept events");
  EventQueue * fileQ = new EventQueue("File events.");
  EventQueue * recvQ = new EventQueue("Received events");
  EventQueue * sentQ = new EventQueue("Sent events");
  EventQueue * serverQ = new EventQueue("Sever start/stop events");
  EventQueue * sendQ = new EventQueue("Send events.");
  
  ConnectionPairMap ConnIDtoConnectionPairMap = {};
    
  EventHandler* eh = new EventHandler(loadMoreNotifier, fileQ, acceptQ, recvQ, sentQ, serverQ, sendQ, &ConnIDtoConnectionPairMap);
  
  std::chrono::high_resolution_clock::time_point startPoint = std::chrono::high_resolution_clock::now();
  
  int startPort = 5000;
  int duration = 30;
  int connections = 40;
  int connectionsPerServer = 10;
  int servers = connections/connectionsPerServer;
  srand(5);
  
  std::string myIP;
  // XXX Change to take in IPs.
  if(myRole == SERVER) {
    myIP = "10.1.1.2";
    remoteHost = "10.1.1.3";
  }
  else {
    // XXX Make b client.
    myIP = "10.1.1.3";
    remoteHost = "10.1.1.2";
  }
  
  std::string dst;
  std::string src;
  if(myRole == CLIENT) {
    dst = remoteHost;
    src = myIP;
  }
  else {
    dst = myIP;
    src = remoteHost;
  }
  
  
  for(int i=0; i < connections; i++) {
    int sport = startPort + rand() % (servers-1);
    int dport = startPort + i;
    std::cout << src << ":" << sport << "->" << dst << ":" << dport;
    connectionPair cp = connectionPair(src, sport, dst, dport);  
    ConnIDtoConnectionPairMap[i] = std::make_shared<connectionPair>(cp);
  }

  eh->startup();
  std::thread eventHandlerThread(&EventHandler::loop, eh, startPoint);
  isRunning.store(false);
  eventHandlerThread.join();
}

void fileWorkerTest(std::string& tmpDir) {
  createDummyFiles(tmpDir);
  std::string ipFileName = "";
  ipFileName.append(tmpDir);
  ipFileName.append("/myips.txt");
  
  std::string connFileName = ""; 
  connFileName.append(tmpDir);
  connFileName.append("/conns.txt");
  
  std::string events1FileName = "";
  events1FileName.append(tmpDir);
  events1FileName.append("/events1.txt");
  std::string events2FileName = "";
  events2FileName.append(tmpDir);
  events2FileName.append("/events2.txt");
  std::vector<std::string> eventsFiles;
  eventsFiles.push_back(events1FileName);
  eventsFiles.push_back(events2FileName);
  
  EventQueue * fileQ = new EventQueue("Test File Worker: Out Events");
  int notifierFD = createEventFD();
  EventNotifier* loadMoreNotifier = new EventNotifier(notifierFD, "Test file notifier.");
  FileWorker* fw = new FileWorker(loadMoreNotifier, fileQ,ipFileName,connFileName,eventsFiles);

  fw->startup();

  std::chrono::high_resolution_clock::time_point startPoint = std::chrono::high_resolution_clock::now();
  std::thread fileWorkerThread(&FileWorker::loop, fw, startPoint);
  
  
  int eventsToPull = 1000000;
  int numEventsPulled = 0;
  int lastEventCountWhenRequestingForMore = 0;
  while(numEventsPulled < eventsToPull) {
    std::shared_ptr<Event> job;
    if((*fileQ).getEvent(job)){
      numEventsPulled++;
      std::cout << numEventsPulled << std::endl;
      job.reset();      
    }
    /*else {
      std::cout << "UNDERFLOW IN EVENTS!!!!!! (pulled " <<numEventsPulled << " events)" << std::endl;
    }*/
    if(numEventsPulled % (maxQueuedFileEvents/10) == 0 && lastEventCountWhenRequestingForMore != numEventsPulled) {
      lastEventCountWhenRequestingForMore = numEventsPulled;
      std::cout << "Requesting more events get loaded. Pulled " << numEventsPulled << " events." << std::endl;
      loadMoreNotifier->sendSignal();
    }
  }
  
  isRunning.store(false);
  
  fileWorkerThread.join();  
  std::cout << std::setprecision(5);
  std::cout << "Pulled " << eventsToPull << " events in: " << msSinceStart(startPoint)/1000 << " s" << std::endl;
}

void createDummyFiles(std::string& tmpDir) {
  // Create files (ipFile, connFile, eventFile)
  errno = 0;
  const int dir_err = mkdir(tmpDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (dir_err == -1 && errno != EEXIST) {
    perror("Error creating directory.");
    exit(1);
  }
  
  // ipFile
  std::string ipFileName = "";
  ipFileName.append(tmpDir);
  ipFileName.append("/myips.txt");
  std::ofstream ipFile;
  ipFile.open(ipFileName.c_str(), std::ios::out);
  ipFile << "5.5.5.5" << std::endl;
  ipFile << "6.6.6.6" << std::endl;
  ipFile.close();
  
  // connFile
  std::string connFileName = "";  
  connFileName.append(tmpDir);
  connFileName.append("/conns.txt");
  std::ofstream connFile;
  connFile.open(connFileName.c_str(), std::ios::out);
  float stime = 0;
  for(int i=0; i<500; i++) {
    float r = 0.001 + static_cast <float> (std::rand())/(static_cast <float> (RAND_MAX/(.1-0.001)));
    connFile << "CONN," << stime << "," << i << ",5.5.5.5,5005,->,1.1.1.1,5005" << std::endl;
    stime = stime + r;
  }  
  connFile.close();
  
  
  // eventsFile
  std::string events1FileName = "";
  events1FileName.append(tmpDir);
  events1FileName.append("/events1.txt");
  std::ofstream events1File;
  std::string events2FileName = "";
  events2FileName.append(tmpDir);
  events2FileName.append("/events2.txt");
  std::ofstream events2File;
  events1File.open(events1FileName.c_str(), std::ios::out);
  events2File.open(events2FileName.c_str(), std::ios::out);
  stime = 0;
  for(int i=0; i<10; i++) {
    float r = 0.001 + static_cast <float> (std::rand())/(static_cast <float> (RAND_MAX/(.1-0.001)));
    events1File << "EVENT," << i << ",3,5.5.5.5,SEND,100," << stime << "," << r << std::endl;
    stime = stime+r;
  }
  for(int i=0; i<10; i++) {
    float r = 0.001 + static_cast <float> (std::rand())/(static_cast <float> (RAND_MAX/(.1-0.001)));
    events2File << "EVENT," << i << ",3,5.5.5.5,SEND,100," << stime << "," << r << std::endl;
    stime = stime+r;
  }
  events1File.close();
  events2File.close();
}

std::atomic<bool> isRunning = false;
std::mutex fileHandlerMTX;
std::condition_variable fileHandlerCV;
bool loadMoreFileEvents = true;

int main(int argc, char* argv[]) {
  bool testAllFlag = true, testFileEventsFlag = false, testConnectionsFlag = false, testEventsFlag = false;
  Role myRole;
  std::string remoteHost = "";
  
  // Parse arguments.
  for(int i=1; i<argc; i++) {
    std::string arg = argv[i];
    if((arg == "-F") || (arg == "-C") || (arg == "-E") || (arg == "-h") || (arg == "-r") || (arg == "-c") || (arg == "-e") || (arg == "-s")) {
      // These don't take arguments.
      if((arg == "-F") || (arg == "-C") || (arg == "-E") || (arg == "-h")) {
        if(arg == "-F") {
          testAllFlag = false;
          testFileEventsFlag = true;
        }
        else if(arg == "-C") {
          testAllFlag = false;
          testConnectionsFlag = true;
        }
        else if(arg == "-E") {
          testAllFlag = false;
          testEventsFlag = true;
        }
        else if(arg == "-h") {
          usage(argv[0]);
        }
      }
      // The rest take arguments.
      else {
        // So make sure we have the argument.
        if(i+1 < argc) {
          if(arg == "-r") {
            // -r takes 2 arguments.
            if(i+2 < argc) {
              if(strstr(argv[i+1], "S") || strstr(argv[i+1], "s")) {
                std::cout << "Host is server." << std::endl;
                myRole = SERVER;
              }
              else
                myRole = CLIENT;
              remoteHost = argv[i+2];
              i = i+2;
              
            }
            else {
              std::cerr << arg << "-r takes 2 arguments: {client|server}{IP of other host}" << std::endl;
              exit(-1);
            }
          }
          else {
            // XXX rest not implemented yet.
            std::cout << "Option ignored. Not implemented yet." << std::endl;
          }
        }
        else {
          std::cerr << arg << " requires argument." << std::endl;
          exit(-1);
        }
      }
    }
    else {
      std::cerr << arg << " is an unknown option." << std::endl;
    }
  }

  if(testAllFlag || testFileEventsFlag) {
    isRunning.store(true);
    std::string tmpDir = "/tmp/mimic-tests";
    fileWorkerTest(tmpDir);
  }
  
  std::cout << testConnectionsFlag << std::endl;
  
  if(testAllFlag || testConnectionsFlag || testFileEventsFlag) {
    if(testAllFlag || testConnectionsFlag) {
      isRunning.store(true);
      std::cout << "event handler" << std::endl;
      eventHandlerTest(remoteHost, myRole);
    }
    if(testAllFlag || testEventsFlag) {
      isRunning.store(true);
      eventHandlerTest(remoteHost, myRole);
    }
  }
  
  exit(1);
}
