#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <regex>
#include <iostream>

using namespace std;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

char *removeHTTPHeader(char *buffer, int &bodySize) {
    char *t = strstr(buffer, "\r\n\r\n");
    t = t + 4;

    for (auto it = buffer; it != t; ++it) {
        ++bodySize;
    }

    return t;
}

void printData(const int &socketfd, const int &bSize) {

    char buffer[bSize];
    ssize_t bRec;

    bRec = recv(socketfd, buffer, bSize, 0);
    int bodySize = 0;

    char *t = removeHTTPHeader(buffer, bodySize);
    bodySize = bRec - bodySize;

    cout.write(t, bodySize);
    memset(buffer, 0, bSize);

    while ((bRec = recv(socketfd, buffer, bSize,0)) > 0) {
        cout.write(buffer, bRec);
        memset(buffer, 0, bSize);
    }
}

bool sendAll(int sockfd, void *buffer, size_t len)
{
    char *pbuffer = (char*) buffer;
    while (len > 0)
    {
        int i = send(sockfd, pbuffer, len, 0);
        if (i < 1) return false;
        pbuffer += i;
        len -= i;
    }
    return true;
}

string* parseUrl(const string &url){
    string protocol, host, port, path;
    size_t found = url.find_first_of(":");

    //parse protocol
    if(found < 6){
        protocol = url.substr(0,found);
    }

    //parse host
    string hostStart;
    if(found > url.length() || protocol.length() == 0){
        hostStart = url.substr(0);
    }
    else{
        hostStart = url.substr(found+3);
    }
    size_t hostEnd = hostStart.find_first_of(":/");
    if(hostEnd > url.length()){
        host = hostStart.substr(0,url.length());
    }
    else{
        host = hostStart.substr(0,hostEnd);
    }


    //parse port
    size_t portStart = hostStart.find_first_of(":");
    size_t portEnd = hostStart.find_first_of("/") -1;
    if(portStart < url.length()){
        port = hostStart.substr(portStart + 1, (portEnd-portStart));
    }
    else{
        port = "80";
    }

    //find path
    size_t pathStart = hostStart.find_first_of("/");
    if(pathStart < url.length()){
        path = hostStart.substr(pathStart);
    }



    string* portion = new string[4];
    portion[3] = protocol;
    portion[2] = host;
    portion[1] = port;
    portion[0] = path;

    return portion;

}

int main(int argc, char *argv[]) {

    if(argc < 2){
        error((char*)"URL required!");
    }

    string url = argv[1];
    const string *urlPortion = parseUrl(url);

    //initialize variables
    char* headerFormat = (char*)"Host: %s\r\n";
    char* CRLF = (char*) "\r\n";
    int sockfd;
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    char* path = const_cast<char *>(urlPortion[0].c_str());

    //assemble request line
    char* requestLine = new char[17 + strlen(path)];
    strcpy(requestLine, "GET ");
    strcat(requestLine, path);
    strcat(requestLine, " HTTP/1.1\r\n");

    char* host = const_cast<char*>(urlPortion[2].c_str());


    //check if port# provided
    char* port = const_cast<char*> (urlPortion[1].c_str());

    size_t bufferLen = strlen(headerFormat) + strlen(host) + strlen(path) + 1;
    char *buffer = (char *) malloc(sizeof(char) * bufferLen);

    char request[strlen(requestLine) + strlen(buffer) + strlen(CRLF)];
    strcpy(request, requestLine);
    snprintf(buffer, bufferLen, headerFormat, host);
    strcat(request, buffer);
    strcat(request, CRLF);

    //cerr << request << endl;

    free(buffer);
    buffer = NULL;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    //fall get addrinfo
    if((status = getaddrinfo(host, port, &hints, &servinfo)) != 0){
        char* errorMsg = new char[strlen("Error getaddinfo error: ") + strlen(gai_strerror(status)) + strlen("\n")];
        strcpy(errorMsg, "Error getaddinfo error: ");
        strcat(errorMsg, gai_strerror(status));
        error((char*) errorMsg);
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, 0);

    if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0)
        error((char*) "Connect");


    // release getaddrinfo memory
    freeaddrinfo(servinfo);
    servinfo = NULL;

    int n = sendAll(sockfd, request, strlen(request));
    if (n < 0)
        error((char*) "Send");

    printData(sockfd, 1024);

    //shutdown socket
    shutdown(sockfd, SHUT_WR);
    return 0;
}
