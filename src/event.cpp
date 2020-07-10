#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <queue> 
#include <thread>
#include "mimic.h"

template<typename T, typename ...Args> std::unique_ptr<T> make_unique( Args&& ...args ) {
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}

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
        std::unique_ptr<Event> next_event();
    private:
        std::ofstream eventsstream;
        std::priority_queue <Event, std::vector<Event>, compareEvents > eventHeap;
};

EventHandler::EventHandler() {
}

void EventHandler::add_event(Event e) {
    eventHeap.push(e);
}

std::unique_ptr<Event> EventHandler::next_event() {
    std::unique_ptr<Event> e_shr (nullptr);
    if(eventHeap.empty() == true) {
        std::cout << "Heap empty.\n";
        return e_shr;
    }
    e_shr = make_unique<Event>(eventHeap.top());
    eventHeap.pop();
    return e_shr;
}

class FileHandler {
    public:
        FileHandler(const EventHandler * eh);
        void manage_files();
    private:
        void process_conns();
        const std::string * filename;    
};

FileHandler::FileHandler(const EventHandler * eh) {

};

void FileHandler::manage_files() {
};

void FileHandler::process_conns() {
};

// STUB testing event 
int eventmain() {
    std::cout << "hi\n";
    Event e1, e2, e3, e4;
    std::unique_ptr<Event> eptr1, eptr2;
    
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
    while(eptr1 != nullptr) {
        std::cout << "Top: " << eptr1->ms_from_start << "\n";
        eptr1 = eh.next_event();
    }
    std::exit(1);
}
