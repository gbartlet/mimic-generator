#include "connections.h"
#include <sstream> 


connectionPair::connectionPair(std::string srcIP, int sport, std::string dstIP, int dport) {
    src.sin_family = dst.sin_family = AF_INET;                                    
    inet_pton(AF_INET, srcIP.c_str(), &src.sin_addr);                                                      
    src.sin_port = htons(sport);                                        
    inet_pton(AF_INET, dstIP.c_str(), &dst.sin_addr);
    dst.sin_port = htons(dport);    
}

bool connectionPair::operator==(const connectionPair a) const {                                      
    if((cmpSockAddrIn(&src, &(a.src))) && (cmpSockAddrIn(&dst, &(a.dst)))) return true;
    return false;       
}


bool cmpSockAddrIn(const sockaddr_in* a, const sockaddr_in* b) {
    //if (std::memcmp(a, b, sizeof(struct sockaddr_in)) == 0) return true;        
    if(a->sin_family == b->sin_family) {
        if(ntohl(a->sin_addr.s_addr) == ntohl(b->sin_addr.s_addr)) {
            if(a->sin_port == b->sin_port) {
                return true;
            }
        }
    }
    return false;       
}

std::string getIPPortString(const struct sockaddr_in* sa) {
    /* XXX Should use this in getConnString */
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sa->sin_addr), str, INET_ADDRSTRLEN);
    int port = ntohs(sa->sin_port);
    
    std::ostringstream stringStream;
    stringStream.clear();
    
    stringStream << str << ":" << port;
    return stringStream.str();
}

               
std::string getConnString(const struct sockaddr_in* src, const struct sockaddr_in* dst, bool* success) {
    /* XXX should set success based off of result. */
    *success = true;

    char srcStr[INET_ADDRSTRLEN];
    char dstStr[INET_ADDRSTRLEN];
    
    inet_ntop(AF_INET, &(src->sin_addr), srcStr, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(dst->sin_addr), dstStr, INET_ADDRSTRLEN);
    std::ostringstream stringStream;
    stringStream.clear();

    int sport= 0,  dport  = 0;

    if (src->sin_family == AF_INET) {
        sport = ntohs(src->sin_port);
        dport = ntohs(dst->sin_port);
    }

    stringStream << srcStr << ":" << sport << "," << dstStr << ":" << dport;
    
    std::cout << "String stream: " << stringStream.str() << std::endl;

    return stringStream.str();
    return "";
}

int setIPv4TCPNonBlocking(int sockfd) {
    int status = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
    if(status == -1) {
        perror("Had trouble getting non-blocking socket."); 
        return(-1);
    }
    return status;
}

int getIPv4TCPSock(const struct sockaddr_in * sa) {
    /* Get non-blocking socket. */
    int s = socket(AF_INET, SOCK_STREAM, 0);
    setIPv4TCPNonBlocking(s);

    if(s == -1) 
        perror("Set sockopt failed.");

    /* If we were given an address, bind to it. */
    if(sa != NULL) {
        if(bind(s, (const struct sockaddr *)sa, sizeof(struct sockaddr_in)) <0) {
            perror("bind failed.");
            return(-1);
        }
    }

    return s;
}
