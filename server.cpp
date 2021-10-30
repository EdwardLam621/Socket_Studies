#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/stat.h>
#include <ctime>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sstream>

using namespace std;

string trim(string& str)
{
    string s = str;
    size_t first = str.find_first_not_of(' ');
    if(first<str.length()){
        s = str.substr(first,str.length());
    }
    size_t last = s.find_last_not_of(' ');
    if(last<s.length()){
        s = str.substr(0, last+1);
    }
    return s;
}

char* stringToChar(string s){
    char *c = new char[s.length()+1];
    strcpy(c, s.c_str());
    return c;
}

string getCurrentPath()
{
   char buff[FILENAME_MAX];
   getcwd(buff, FILENAME_MAX);
   string current_working_dir(buff);
   return current_working_dir;
}

string configPath(string p){

    //trim possible spaces
    string request = trim(p);

    //trim request
    ssize_t pathStart = request.find_first_of("/");
    string undonePath = request.substr(pathStart, request.length());

    ssize_t pathEnd = undonePath.find_first_of(" ");
    string path = undonePath.substr(0, pathEnd);

    //trim
    string dir = getCurrentPath();
    dir+= path;
    if(dir.find_last_of("/") == dir.length() - 1){
        dir += "index.html";
    }
    return dir;
}

string parseFileExt(const string& s){
    size_t i = s.rfind('.', s.length());
    if (i != string::npos) {
        string n = (s.substr(i+1, s.length() - i));
        if(n.compare("txt") == 0) return "text/plain";
        if(n.compare("html") == 0) return "text/html";
        if(n.compare("htm") == 0) return "text/htm";
        if(n.compare("css") == 0) return "text/css";
        if(n.compare("jpg") == 0) return "text/jpeg";
        if(n.compare("jpeg") == 0) return "text/jpeg";
        if(n.compare("png") == 0) return "text/png";
    }
    return("unknown");
}

int codeHandler(string request){

    //if not GET method
    if(request.find("POST") == 0
        || request.find("PUT") == 0
        || request.find("HEAD") == 0
        || request.find("DELETE") == 0
        || request.find("PATCH") == 0
        || request.find("OPTIONS") == 0){
        return 501;
    }

    //if not any of the method
    else if(request.find("GET") != 0)
        return 400;


    else if(request.find("..") == 0)
        return 400;

    else if(request.length()<=0)
        return 400;

    //find if file exists
    string p = configPath(request);
    ifstream ifile(p);
    if(!ifile.good()) return 404;
    ifile.close();

    return 200;
}

string getStatus(int c){
    if(c == 200) return "OK";
    if(c == 501) return "Not Implemented";
    if(c == 400) return "Bad Request";
    if(c == 404) return "Not Found";
    else return "Unknown";
}

string getData(string path){
    ifstream inFile;
    inFile.open(path); //open the input file

    stringstream strStream;
    strStream << inFile.rdbuf(); //read the file
    string str = strStream.str(); //str holds the content of the file

    return str;
}

char* assembleResponseHeader(int code, char* status, string path){

    //status
    char *p = "HTTP/1.1 ";

    //code
    string c = to_string(code);
    char * pc = new char[c.length() + 1];
    strcpy(pc, c.c_str());
    strcat(pc, " ");

    //status
    char *s = new char[strlen(status) + 4];
    strcpy(s, status);
    strcat(s, "\r\n");

    //time
    time_t now = time(0);

    //trim next line symbol from stat time
    string time = ctime(&now);
    time.erase(time.find_last_not_of("\n")+1);

    //copy status to buffer
    char *httpStat = new char[strlen(p) + strlen(pc) + strlen(s) + time.length() + strlen("\r\n") + 1];
    strcpy(httpStat,p);
    strcat(httpStat,pc);
    strcat(httpStat,s);
    strcat(httpStat,stringToChar(time));
    strcat(httpStat,"\r\n");

    char* detail;
    char* pa = new char [strlen(stringToChar(path))+1];
    strcpy(pa, stringToChar(path));

    //if 200, include path message
    if(code == 200){
        struct stat buf;
        stat(pa, &buf);


        string s = to_string(buf.st_size);
        char* size = new char[s.length()];
        strcpy(size,stringToChar(s));

        //assemble message
        detail = new char[strlen("Content-Length: ")
                          + strlen("Content-Type: ")
                          + strlen("Last-Modified: ")
                          + strlen(size)
                          + strlen(stringToChar(parseFileExt(path)))
                          + strlen(ctime(&buf.st_mtime))
                          + 12];
        strcpy(detail,"Content-Length: ");
        strcat(detail, size);
        strcat(detail, "\r\n");
        strcat(detail, "Content-Type: ");
        strcat(detail, stringToChar(parseFileExt(path)));
        strcat(detail, "\r\n");
        strcat(detail, "Last-Modified: ");
        strcat(detail, ctime(&buf.st_mtime));
        strcat(detail, "\r\n");
        free(size);
    }

    //else return code and status
    else{
        detail = new char[c.length() + strlen(s) + 1];
        strcpy(detail, stringToChar(c));
        strcat(detail," ");
        strcat(detail, status);
    }

    char* buff = new char [strlen(httpStat) + strlen(detail) + strlen("\r\n") + 1];
    strcpy(buff,httpStat);
    strcat(buff, "\r\n");
    strcat(buff,detail);


    return buff;
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

bool sendAll(int sockfd, char *buffer, size_t len)
{
    char *pbuffer = (char*) buffer;
    while (len > 0)
    {
        int i = send(sockfd, pbuffer, len,0);
        if (i < 1) return false;
        if(i == 0) fprintf(stderr, "ERROR: unexpected termination\n");
        pbuffer += i;
        len -= i;
    }
    return true;
}

int recvAll(const int &socketfd, char *buffer, const int &bSize) {
    char* pbuffer = buffer;
    size_t toread = bSize;
    ssize_t remain = 1;

    while (remain > 0)
    {
        remain = recv(socketfd, pbuffer, toread, 0);
        if(remain < 0)
            return remain;
        if(remain == 0) fprintf(stderr, "ERROR: unexpected termination\n");
        if(remain < toread){
            remain = toread;
        }
        remain -= toread;
        pbuffer += remain;
    }
    return bSize;
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    char* end;
    struct sockaddr_in serv_addr;
    int bufsize = 131072;

    int n;
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }


    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error((char *) "ERROR opening socket");
    memset((char *) &serv_addr,0, sizeof(serv_addr));
    portno = strtol(argv[1], &end, 10);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);
    if (::bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        error((char *) "ERROR on binding");
    }
    if (listen(sockfd,5) < 0){
        error((char *) "ERROR on listening");
    }

    bool listening = true;

    while(listening){
        char buffer[bufsize];
        newsockfd = accept(sockfd, (struct sockaddr *) NULL, NULL);
        if (newsockfd < 0)
            error((char *) "ERROR on accept");
        memset(buffer,0, sizeof(buffer));
        buffer[bufsize] = '\0';

        //recv loop
        n = recvAll(newsockfd,buffer, sizeof(buffer));

        if(n < 0){
            fprintf(stderr,"Failed receving from socket");
        }

        //parse request
        string path = configPath(buffer);
        int code = codeHandler(buffer);
        string status = getStatus(code);

        if (n < 0) fprintf(stderr,"ERROR reading from socket");

        printf("Request: %s", buffer);

        string body;
        if(code == 200){
            body = getData(path);
        }

        char *sc = assembleResponseHeader(code,stringToChar(status),path);

        char* newbuffer = new char[strlen(sc) + body.length() + strlen("\r\n")];
        memset(newbuffer, 0, sizeof(newbuffer));
        strcat(newbuffer, sc);
        strcat(newbuffer, stringToChar(body));
        strcat(newbuffer,"\r\n");

        printf("Response: %s\n==========\n", newbuffer);

        n = sendAll(newsockfd,newbuffer, strlen(newbuffer));
        if (n < 0) fprintf(stderr,"ERROR writing to socket");

        memset(buffer,0, sizeof(buffer));
        memset(newbuffer,0, sizeof(newbuffer));

        close(newsockfd);
    }



    return 0;
}