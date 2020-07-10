#include <thread> 
#include <sys/epoll.h>
#include "eventNotifier.h"
#include "pollHandler.h"

void workerThread(EventNotifier * en) {
  int epollfd;
  
  epollfd = epoll_create1(0);
  if(epollfd == -1) return;
  
  struct epoll_event event = {0};
  event.data.fd = en->myFD();
  event.events = EPOLLIN | EPOLLET;
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, en->myFD(), &event) == -1) return;
  
  
  static const int EVENTS = 20;
  struct epoll_event events[EVENTS];
  
  int dings = 0;
  while(dings < 10) {
    int count = epoll_wait(epollfd, events, EVENTS, 100);
    for(int j=0; j<count; ++j) {
      struct epoll_event *e = events + j;
      if(en->isMe(e->data.fd)) {
        std::cout << en->myName << ": Received Ding!" << std::endl;
        en->readSignal();
        dings++;
      }
    }
  
    en->sendSignal();
    
  }
}

void workerThread2(EventNotifier * en) {
  PollHandler* ph = new PollHandler();
  ph->watchForRead(en->myFD());
  
  int dings = 0;
  while(dings < 10) {
    if(en->myName == "thread 1")
      std::cout << en->myName << ": Waiting for events. " << std::endl;
   
    ph->waitForEvents(100);
   
    struct epoll_event e;
    while(ph->nextEvent(&e)) {
      if(en->myName == "thread 1")
        std::cout << en->myName << ": Got event on fd #" << int(e.data.fd) << " event fd is " << en->myFD() << std::endl;
  
      if(en->isMe(int(e.data.fd))) {
        std::cout << en->myName << ": Received Ding!" << std::endl;
        en->readSignal();
        dings++;
      }
  
    }
    
    //std::cout << en->myName << ": Sending signal. " << std::endl;
    en->sendSignal();
  
  }  
  
}


int main() {
  int fd = createEventFD();
  EventNotifier* en1 = new EventNotifier(fd, "thread 1");
  EventNotifier* en2 = new EventNotifier(fd, "thread 2");
  std::thread first(workerThread2, en1);
  usleep(500);
  std::thread second(workerThread2, en2);
  
  first.join();
  second.join();
  
  exit(0);
  
}
