#ifndef CONNECTIONS_H
#define CONNECTIONS_H
#include "mimic.h"

enum conn_state {LISTENING, CONNECTING, EST, DONE};

typedef std::unordered_map<std::string, long int> stringToConnIDMap;

std::string getConnString(const struct sockaddr_in* src, const struct sockaddr_in* dst, bool* success);
std::string getIPPortString(const struct sockaddr_in* sa);

bool cmpSockAddrIn(const sockaddr_in* a, const sockaddr_in* b);
                            
struct connectionPair {
  struct sockaddr_in src; 
  struct sockaddr_in dst; 
  connectionPair(std::string srcIP, int sport, std::string dstIP, int dport);    
  bool operator==(const connectionPair a) const;
};         

typedef std::unordered_map<long int, std::shared_ptr<connectionPair>> ConnectionPairMap;

// Some short socket helpers.
int setIPv4TCPNonBlocking(int sockfd);
int getIPv4TCPSock(const struct sockaddr_in * sa);



#endif



