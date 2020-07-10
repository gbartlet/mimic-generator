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

int compareEvents::operator()(const Event& e1, const Event& e2) {
    return e1.ms_from_start > e2.ms_from_start;
}




// Based on Herb Sutter's lockless queue article.
// See http://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448 for details


// Each EventQueue object can have a single producer and a separate single consumer.
// In practice, we probably want to have a single producer thread for multiple EventQueue, and 
// consumer threads which consume from multiple EventQueues.

//class EventQueue {
        EventQueue::EventQueue(std::string name) {
            #ifndef __cpp_lib_atomic_is_always_lock_free
                std::cout << "Exiting due to lack of atomic lock-free support. Check that your compiler is ISO C++ 2017 or 2020 compliant.\n" ;
                exit(1);
            #endif
            
            std::shared_ptr<Event> dummy = nullptr;
            eventJob * d = new eventJob(dummy);
            last.store(d, std::memory_order_relaxed); 
            divider.store(d, std::memory_order_relaxed);
            first = d;
            qName = name;
        }
        EventQueue::~EventQueue() {
            while(first != nullptr) {
                eventJob * tmp = first;
                first = tmp->next;
                delete tmp;
            }
        }
        int EventQueue::cleanUp() {
            while(first != divider.load()) {
                eventJob * tmp = first;
                first = first->next;
                std::cout << "Use count of epointer before removal of job" << tmp->eptr.use_count() << "\n";
                tmp->eptr.reset();
                delete tmp; 
                numEvents--;
            }
            return numEvents;
        }
        void EventQueue::addEvent(std::shared_ptr<Event> e) {
            //last->next = new eventJob(e);
            eventJob * lastNode = last.load();
            lastNode->next = new eventJob(e);
            
            //last = last->next;
            last.store(lastNode->next);
            numEvents++;
            cleanUp();            
            std::cout << qName << ":Num events: " << numEvents << "\n";
        }
        
        
        /* The below two functions should only be called by the consumer. */
        bool EventQueue::getEvent(std::shared_ptr<Event>& job) {
            eventJob * dividerNode = divider.load();
            if(dividerNode != last.load()) {
                //job = divider->next->eptr;
                if (dividerNode->next != nullptr) {
                    job = dividerNode->next->eptr;
                    //divider = divider->next;
                    divider.store(dividerNode->next);
                    std::cout << qName << "JOB REMOVED" << std::endl;
                    return true;
                }
            }
            return false;
        }
        long int EventQueue::nextEventTime() {
            eventJob * dividerNode = divider.load();
            if(dividerNode != last.load()) {
                if(dividerNode->next != nullptr) {
                    long int t = dividerNode->next->eptr->ms_from_start;
                    return t;
                }
            }
            std::cout << qName << "Time checked. " << std::endl;
            return -1;
        }
//}


EventHeap::EventHeap() {
}
EventHeap::~EventHeap() {
}

void EventHeap::addEvent(Event e) {
    eventHeap.push(e);
}

long int EventHeap::nextEventTime() {
    if(eventHeap.empty() == true) {
        return -1;
    }
    long int t = eventHeap.top().ms_from_start;
    return t;
}

std::unique_ptr<Event> EventHeap::nextEvent() {
    std::unique_ptr<Event> e_shr (nullptr);
    if(eventHeap.empty() == true) {
        //std::cout << "Heap empty.\n";
        return e_shr;
    }
    e_shr = std::make_unique<Event>(eventHeap.top());
    eventHeap.pop();
    return e_shr;
}





bool setNonBlocking(int sockfd) {
    int status = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
    if(status == -1) {
        return false;
    }
    return true;
}

int getSocket(DomainType domain, TransportType type, const struct sockaddr *localAddr) {
    int sockfd = -1;
    int d;
    if(domain == IPV4) d = AF_INET;
    else d = AF_INET6;
    if(type == TCP) {
        sockfd = socket(d, SOCK_STREAM, 0);
    }
    else {
        std::cout << "Non-TCP not yet supported.\n";
        sockfd = -1;
    }
    if(sockfd == -1) return -1;
    if(setNonBlocking(sockfd)) return sockfd;
    return -1;
}

bool serverUp(int sockfd) {
    return true;
}

bool connectToServer(int sockfd) {
    return true;
}

/*
void produceEventsTest(EventQueue* eq, int numEvents, int maxWait) {
    for(int i=0; i<numEvents; i++) {
        Event e;
        e.ms_from_start = 2;
        (*eq).addEvent(std::make_shared<Event>(e));
        if(i%100 == 0) {
            usleep(rand() % maxWait*1000 + 1000) ;
        }
    }
    int x = (*eq).cleanUp();
    while (x > 1) {
        std::cout << "Still cleaning up events (have " <<  x << " left).\n";
        x = (*eq).cleanUp();
        usleep(rand() % maxWait);
    }
}


void workerSpinOffTest(int maxWait, std::shared_ptr<Event> job) {
    usleep(rand() % maxWait*1000);
    assert(job->ms_from_start == 2);
    job.reset();
}


void consumeEventsTest(EventQueue* eq, int numEvents, int maxWait) {
    std::shared_ptr<Event> job;
    int totalRetrieved = 0;
    for(int i=0; i<numEvents;) {
        if((*eq).getEvent(job)) {
            assert(job->ms_from_start == 2);
            if(i%10 == 0) {
                std::shared_ptr<Event> job2(job);
                std::thread worker(workerSpinOffTest, maxWait, job2);    
                worker.detach();
            }
            usleep(rand() % maxWait);
            job.reset();
            totalRetrieved++;
            if(i%1000 == 0) {
                std::cout << "Total events retrieved so far: " << totalRetrieved << "\n";
            }
            i++;
        }
    }        
    std::cout << "Total events retrieved so far: " << totalRetrieved << "\n";
}

// STUB tester for EventQueue
int main_EventTest() {
    srand(time(NULL));
    int numEvents = 10000;
    int maxWait = 100;
    EventQueue* eq = new EventQueue();
    std::thread consumer(consumeEventsTest, eq, numEvents, maxWait);
    std::thread producer(produceEventsTest, eq, numEvents, maxWait);
    producer.join();
    consumer.join();
    delete eq;
    std::cout << "Success. Added and retrieved " << numEvents << " events.\n";
}
*/