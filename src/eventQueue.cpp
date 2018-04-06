#include <stdlib.h>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <time.h>
#include "mimic.h"


// Taken from Herb Sutter's lockless queue article.
// See http://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448 for details


// Works for a single producer, single consumer.

class EventQueue {
    private:
        struct eventJob {
            eventJob(std::shared_ptr<Event> ptr): eptr(ptr), next(nullptr) {}
            std::shared_ptr<Event> eptr;
            eventJob* next;
        };
        eventJob* first;
        #ifdef __cpp_lib_atomic_is_always_lock_free 
            static_assert(std::atomic<eventJob*>::is_always_lock_free, "We can't use eventJob* as a lock-free type.");
            std::atomic<eventJob*> divider;
            std::atomic<eventJob*> last;
        #else
            printf("Exiting due to lack of atomic lock-free support. Check that your compiler is ISO C++ 2017 or 2020 compliant.");
            exit(1);
        #endif
        int numEvents = 1;
    public:
        EventQueue() {
            std::shared_ptr<Event> dummy = nullptr;
            eventJob * d = new eventJob(dummy);
            last.store(d, std::memory_order_relaxed); 
            divider.store(d, std::memory_order_relaxed);
            first = d;
        }
        ~EventQueue() {
            while(first != nullptr) {
                eventJob * tmp = first;
                first = tmp->next;
                delete tmp;
            }
        }
        void addEvent(std::shared_ptr<Event> e) {
            //last->next = new eventJob(e);
            eventJob * lastNode = last.load();
            lastNode->next = new eventJob(e);
            
            //last = last->next;
            last.store(lastNode->next);
            numEvents++;
            while(first != divider.load()) {
                eventJob * tmp = first;
                first = first->next;
                delete tmp; 
                numEvents--;
            }
            std::cout << "Num events: " << numEvents << "\n";
        }
        bool getEvent(std::shared_ptr<Event>& job) {
            eventJob * dividerNode = divider.load();
            if(dividerNode != last.load()) {
                //job = divider->next->eptr;
                if (dividerNode->next != nullptr) {
                    job = dividerNode->next->eptr;
                    //divider = divider->next;
                    divider.store(dividerNode->next);
                    return true;
                }
            }
            return false;
        }
};

void produceEventsTest(EventQueue* eq, int numEvents, int maxWait) {
    for(int i=0; i<numEvents; i++) {
        Event e;
        e.ms_from_start = 2;
        (*eq).addEvent(std::make_shared<Event>(e));
        usleep(rand() % maxWait);
    }
}

void consumeEventsTest(EventQueue* eq, int numEvents, int maxWait) {
    std::shared_ptr<Event> job;
    int totalRetrieved = 0;
    for(int i=0; i<numEvents;) {
        if((*eq).getEvent(job)) {
            assert(job->ms_from_start == 2);
            usleep(rand() % maxWait);
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
int main() {
    srand(time(NULL));
    int numEvents = 1000000;
    int maxWait = 1000;
    EventQueue* eq = new EventQueue();
    std::thread producer(produceEventsTest, eq, numEvents, maxWait);
    std::thread consumer(consumeEventsTest, eq, numEvents, maxWait);
    producer.join();
    consumer.join();
    delete eq;
    std::cout << "Success. Added and retrieved " << numEvents << " events.\n";
}
