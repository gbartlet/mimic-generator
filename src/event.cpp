#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <queue> 
#include "mimic.h"

class compareEvents {
    public:
        int operator()(const Event& e1, const Event& e2) {
            return e1.ms_from_start > e2.ms_from_start;
        }
};

class EventHandler {
    public:
        EventHandler();
        void add_event(Event e);
        std::shared_ptr<Event> next_event();
    private:
        const std::string * filename;
        std::ofstream eventsstream;
        std::priority_queue <Event, std::vector<Event>, compareEvents > eventHeap;
};

EventHandler::EventHandler() {
}

void EventHandler::add_event(Event e) {
    eventHeap.push(e);
}

std::shared_ptr<Event> EventHandler::next_event() {
    std::shared_ptr<Event> e_shr (nullptr);
    if(eventHeap.empty() == true) {
        std::cout << "Heap empty.\n";
        return e_shr;
    }
    e_shr = std::make_shared<Event>(eventHeap.top());
    eventHeap.pop();
    return e_shr;
}

// STUB testing event 
int main() {
    std::cout << "hi\n";
    Event e1, e2, e3, e4;
    std::shared_ptr<Event> eptr1, eptr2;
    
    e1.ms_from_start = 2;
    e2.ms_from_start = 3;
    e3.ms_from_start = 2;
    e4.ms_from_start = 1;
    
    EventHandler eh;
    eh.add_event(e1);
    eh.add_event(e2);
    eh.add_event(e3);
    eh.add_event(e4);
    
    eptr1 = eh.next_event();
    if(eptr1 == nullptr) {
        std::cout << "No events left in heap.\n"; 
    }
    else {
        std::cout << "Top: " << eptr1->ms_from_start << "\n";
    }
    eptr2 = eptr1;
    std::cout << "Refence count for eptr1:" << eptr1.use_count() << "\n";
    std::exit(1);
}
