#include <ctime>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>

enum EventType {
                ACCEPT, 	/* As a server, accept a connection from a client. */
                RECV, 		/* Accept/wait for {value}bytes of  data. */
                WAIT,		/* Wait {value}ms before next event. */
                SEND, 		/* Send {value}bytes of data. */
                SRV_START,	/* Bring up a server. */ 
                SRV_END 	/* Bring down a server. */
                };
                        
class Event {
    int * sockptr;
    long int ms_from_start;	 /* Keep track of event time relative to start in ms. */
    EventType type;        
    long int conn_id;
    struct sockaddr_in target_addr;
    int target_port;		/* What this target means is dependent on what type of event this is. */
    long int value;		/* What this value holds depends on what type of event this is. */
};



