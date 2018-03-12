#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include "mimic.h"

class EventHandler {
    public:
        EventHandler();
        void next_event(long int conn_id);
    private:
        const string * filename;
        ofstream eventsstream;
};


EventHandler::EventHandler(const string & filename) {

}


// STUB testing event 
int main() {
    Event e;
}
