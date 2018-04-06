#include <stdlib.h>
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
    public:
        EventQueue() {
            std::shared_ptr<Event> dummy = nullptr;
            eventJob * d = new eventJob(dummy);
            last.store(d, std::memory_order_relaxed); 
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
            last.load()->next = new eventJob(e);
            
            //last = last->next;
            last = last.load()->next;
            
            while(first != divider) {
                eventJob * tmp = first;
                first = first->next;
                delete tmp;
            }
        }
        bool getEvent(std::shared_ptr<Event> job) {
            if(divider.load() != last.load()) {
                //job = divider->next->eptr;
                job = divider.load()->next->eptr;
                //divider = divider->next;
                divider = divider.load()->next;
                return true;
            }
            return false;
        }
};

// STUB tester for EventQueue
int main() {
    Event e1, e2, e3, e4;
    e1.ms_from_start = 2;
    e2.ms_from_start = 3;
    e3.ms_from_start = 2;
    e4.ms_from_start = 1;
    EventQueue eq;
    eq.addEvent(std::make_shared<Event>(e1));
}
