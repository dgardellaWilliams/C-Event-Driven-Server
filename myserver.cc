/**
 * (c) 2013 Aaron M. Taylor & Devin Gardella
 *
 * This is a c++ implementation of a basic web server supporting HTML 1.0 and HTML 1.1
 */

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <netinet/in.h>
#include <queue>
#include <string.h>
#include <string>
#include <time.h>
#include <sys/stat.h>
#include <regex>
#include <thread>
#include <poll.h>

//number of clients that can be in the backlog
#define MAX_BACKLOG 10
//size of the buffer for requests
#define REQ_SIZ 2048
//size of each packet sent to the client
#define PACK_SIZ 1450
//default timeout of the HTTP 1.1 connections
#define TIMEOUT 20000

class MyServer
{
  
};

std::string rootDirectory;
std::queue<struct Request*> eventQueue;
int connectionsOpen; // the number of currently open connections

/**
 * Method declarations
 */
// This method continually listens for incoming connections and calls methods to handle them
int mainListener(int port);

// This method handles all requests, either responding with error codes or servicing client requests
void handleRequest(struct Request *curReq);

int respondToGET(struct Request *curReq);

int respondToHEAD(struct Request *curReq);

int respondToOPTIONS(struct Request *curReq);

// This method responds to unsupported or disallowed client requests with the appropriate error
int respondWithError(struct Request *curReq, std::string error);

// This method returns the header string for the current request
std::string getHeader(struct Request *curReq, std::string status);

// This method returns the content type
std::string contentTypeForFile(std::string filename);

// This method returns the UPPERCASE version of the inputted string
std::string strToUpper(std::string given);

// This method ensures that the incoming request is of the proper form
int regexGuard(std::string reqstr); //, std::regex e);

// This method ensures that the requested URI is allowed to be accessed
std::string URIGuard(std::string handle);

// This method determines the length of a file
int fileLength(std::ifstream &file);

// This method returns a string with the current date and time
const std::string currentDateTime();

// This method uses a simple herustic to return a variable timeout for a connection
int getTimeout();

// This method will cycle through requests...
int eventProcessor();

//Decides whether to spawn a thread to listen to the socket for 10 seconds more. 
int doesListenMore(Request *curReq);


/** Request Structure
 *  This contains all the information in a request, and are loaded into the event queue
 *  It encapsulates all the necessary information for a request in one structure
 */

struct Request {
  int socket;
  std::string reqstr;
  std::string method;
  std::string requestURI;
  std::string version;
  std::ifstream *file;
  int filesize;
  int filepos;
  int continues;
  
  Request(int sock, std::string rsIn):socket(sock),
				      reqstr(rsIn),
				      method(),
				      requestURI(),
				      version(),
				      file(),
				      filesize(),
				      filepos(0),
				      continues(0) {}
};

/**
 * Forever loop: 
 *   Listen for connections (Done) 
 *   Accept new connection from incoming client (Done) 
 *   Parse HTTP request (Done)
 *   Ensure well-formed request (return error otherwise) (Done) 
 *   Determine if target file exists and if permissions are set properly (return error otherwise) (Done)
 *   Transmit contents of file to connect (by performing reads on the file and writes on the socket) (Done) 
 *   Close the connection (if HTTP/1.0) (Done)
 */
int mainListener(int port)
{
  // This is the main socket that all incoming connections come through
  int mainSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  struct sockaddr_in my_addr;
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(port);
  my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  int uno = 1;
  setsockopt(mainSock,SOL_SOCKET, SO_REUSEADDR, &uno, sizeof(uno) == -1);
  if (bind(mainSock, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) exit(1);    
  listen(mainSock, MAX_BACKLOG);
  // This infinite while loop handles all server operations
  while(true)
    {
      // This is where new connections are accepted and added to the eventQueue if necessary
      int newSock = 0;
      int addrlenp = sizeof(my_addr);
      char *buf = (char*) malloc (REQ_SIZ);
      //If we reach a connection attempt. 
      if ( (newSock = accept(mainSock,(struct sockaddr *)&my_addr,(socklen_t *)&addrlenp)) > 0 ) {  
	int pos = 0;
	while((strstr(buf,"\r\n\r\n") == NULL)&&(strstr(buf,"\n\n") == NULL)) {
	  //Receive piece of client request and store it in a char*
	  pos = recv(newSock, buf+pos, REQ_SIZ, 0);
	}
	std::string reqstr(buf);
	std::cout << "\n\nHANDLING NEW CLIENT REQUEST:\n"
		  << "***************************************\n"
		  << reqstr
		  << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
	// creates a new request struct for this instance using the 2 parameter constructor
	struct Request *curReq = new Request(newSock,reqstr);
	connectionsOpen++;
	handleRequest(curReq);
	free(buf);
      }      
    }
}

// the method to continue listening to a socket for further requests
int continueListen(int socket)
{
  struct pollfd fds;
  fds.fd = socket;
  fds.events = POLLIN | POLLPRI;
  fds.revents = POLLIN | POLLPRI;

  int retval = poll(&fds, 1, getTimeout());

  std::cout << "done listening on socket: "<< socket <<" with poll call returning: "<< retval <<"\n";
  if (retval == 0) { //Time out occured
    close(socket);
    return 0;
  } else if (retval == -1) { //if there was an error
    close(socket);
    return 0;
  } else if (retval) { //Else, we have data on the socket.
    char *buf = (char*)malloc(REQ_SIZ);
    
    int pos = 0;
    while((strstr(buf,"\r\n\r\n") == NULL)&&(strstr(buf,"\n\n") == NULL)) {
      //Receive client request and store it in a string
      pos = recv(socket, buf+pos, REQ_SIZ, 0);
    }
    std::string reqstr(buf);
       
    std::cout << "\n\nHANDLING REQUEST FROM PREVIOUS CLIENT:\n"
	      << "***************************************\n"
	      << reqstr
	      << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    
    struct Request *curReq = new Request(socket,reqstr);
    handleRequest(curReq);
    free(buf);
    return 0;
  }
  std::cout << "code should never fall to here in continueListen\n";
  return 0;
}

//This is where we have a forever while loop that looks for events in the queue to process. 
int eventProcessor(){
  while (true){
    if (!eventQueue.empty()){
      respondToGET(eventQueue.front());
      eventQueue.pop();
    }
  }

  return 0;
}

/* ***************************************************************************************
 * This is the main method, spawning all the necessary starting threads for server operation
 *
 */
int main(int argc, char** argv)
{
  //rootDirectory = std::string("/home/cs-students/16amt4/cs339/server/files");
  //rootDirectory = std::string("/home/cs-students/16dpg3/distSystems/workingserver/files");

  std::string port;
  if (argc < 5) {
    //not enough parameters passed in
    std::cout << "Usage is -document_root <path> -port <int>\n";
    return 0;;
  } else { // if we got enough parameters...
    for (int i = 1; i < argc; i++) { /* We will iterate over argv[] to get the parameters stored inside.
				      * Note that we're starting on 1 because we don't need to know the 
				      * path of the program, which is stored in argv[0] */
      std::string current = std::string(argv[i]);
      if (i + 1 != argc) // Check that we haven't finished parsing already
	if (current.compare("-document_root") == 0) {
	  // We know the next argument *should* be the filename:
	  rootDirectory = std::string(argv[i + 1]);
	} else if (current.compare("-port") == 0) {
	  port = std::string(argv[i + 1]);
	} else {
	  //std::cout << "Not enough or invalid arguments, please try again.\n";
	}
    }
  }
  
  int iPort = atoi(port.c_str());
  std::thread navi(mainListener, iPort);
  std::thread mailman(eventProcessor);
  navi.join();
  mailman.join();
  exit(0);
}

/**
 * This method is dispatched called within a detatches thread and handles requests
 */
void handleRequest(struct Request *curReq) {
  //if the basic format of the request is correct... 
  if (regexGuard(curReq->reqstr)) {

    char* reqcstr = (char*)curReq->reqstr.c_str();
    // gets the string for method
    curReq->method = std::string(strtok(reqcstr," "));
    //sets the requestURI
    curReq->requestURI = std::string(strtok(NULL," "));
    //sets default path of '/' to '/index.html'
    if (curReq->requestURI.compare("/") == 0) {
      curReq->requestURI = "/index.html";
    }
    
    curReq->version = std::string(strtok(NULL," \r\n"));
    std::cout << "method: \"" << curReq->method
	      << "\" requestURI: \"" << curReq->requestURI
	      << "\" version: \"" << curReq->version << "\"\n";
    
    if ((curReq->continues = doesListenMore(curReq))) {
      std::thread (continueListen, curReq->socket).detach();
    }

    //tests for valid URI and then handles file opening
    std::string statuscode;
    
    //if we are doing an options call
    if (curReq->method.compare("OPTIONS") == 0) {
      curReq->continues = doesListenMore(curReq);
      respondToOPTIONS(curReq);
      return;
    }

    if ((statuscode = URIGuard(curReq->requestURI)).compare("200 OK") == 0) {
      std::string filepath = rootDirectory + curReq->requestURI;
      //std::cout << "full path:" << filepath << "\n";
      std::ifstream filestream(filepath);
      //file was opened appropriately
      if (filestream.is_open()) {
	curReq->file = &filestream;
	curReq->filesize = fileLength(filestream);
	//std::cout << "in handleRequest filelength is: " << fileLength(*curReq->file) << "\n";
	if ((strToUpper(curReq->method).compare("GET") == 0)) {
	  //puts these requests onto the event queue because they are more process-intensive
	  filestream.close();
	  eventQueue.push(curReq);
	  //close filestream here to reopen it in the other eventProcessor thread
	} else if ((strToUpper(curReq->method).compare("HEAD") == 0)) {
	  respondToHEAD(curReq);
	} else {
	  //method requested hasn't been implemented
	  std::string errorStr = std::string("501 Not Implemented");
	  respondWithError(curReq, errorStr);
	}	
	//A server SHOULD return the status code 405 (Method Not Allowed) if the method is known 
	//by the orgin server but not allowed for the requested resource. 
      } else {
	std::cout << "Error opening file\n";
	respondWithError(curReq, "404 File Not Found");
      }
      //ensures that the file is closed if something goes wrong
      if (filestream.is_open()) { filestream.close(); }
    } else {
      std::cout << "Failed URI GUARD\n";
      //client was trying to do something that was dissallowed by our URIGaurd
      respondWithError(curReq, statuscode);
    }
  } else {
    std::cout << "Failed RegexGuard\n";
    respondWithError(curReq, "400 Bad Request");
  }
}

//sends the file to the client a packet at a time
int respondToGET(struct Request* curReq)
{
  std::ifstream filestream(rootDirectory + curReq->requestURI, std::ifstream::in);
  //first call to this method, send header
  if ((curReq->filepos) == 0) {
    
    //open the file within this thread
    curReq->file = &filestream;

    //store an integer in end that represents the number of characters in the file.

    // This will send the header in its own packet
    std::string header = getHeader(curReq, std::string("200 OK"));
    send(curReq->socket, header.c_str(), header.length(), 0);
  }

  //initialize and resize the fileContents string to the size of a packet
  std::string fileContents;
  fileContents.resize(PACK_SIZ);
  
  // local variables to simplify implementation
  int p = curReq->filepos;
  int end = curReq->filesize;
  //place a marker where we want to start reading.
  (filestream).seekg(p);
  
  //if there is less than a packet to send resize our string to be this length
  if ((end - p) < PACK_SIZ){
    fileContents.resize((end-p));
  }
  //read at most a packet's worth of data into the string
  filestream.read(&fileContents[0], fileContents.length());
  send(curReq->socket, fileContents.c_str(), fileContents.length(), 0);
  curReq->filepos += PACK_SIZ;
  filestream.close();

  if (curReq->filepos >= end) {
    //we are done with the file, no need to add to the event queue
    //just check if we need to continue listening to the client
    if (!curReq->continues) {
      close(curReq->socket);
      connectionsOpen--;
    }
    free(curReq);
  } else {
    //not done sending, put the Request back in the Queue
    eventQueue.push(curReq);
  }
}

int respondToHEAD(struct Request *curReq)
{
  // just sends the header for the requested response
  std::cout << "Processing a request for HEAD\n";
  std::string header = getHeader(curReq, "200 OK");

  send(curReq->socket, header.c_str(), header.length(), 0);

  if (!curReq->continues) {
    close(curReq->socket);
    connectionsOpen--;
    free(curReq);
  }
}

int respondToOPTIONS(struct Request *curReq)
{
  std::cout << "Responding to OPTIONS\n";
  std::string body = std::string("")
    +"Access-Control-Allow-Methods: GET,HEAD,OPTIONS\n"
    +"Allow: GET,HEAD,OPTIONS\n"
    +"Public: GET,HEAD,OPTIONS\n";
  curReq->filesize = body.length();
  std::string response = getHeader(curReq, "200 OK");
  //adds a simple body for the browser to display
  response += body;
  send(curReq->socket, response.c_str(), response.length(), 0);
  
  if (!curReq->continues) {
    close(curReq->socket);
    connectionsOpen--;
    free(curReq);
  }
}

int respondWithError(struct Request *curReq, std::string error)
{
  // sends the error passed in
  std::cout << "Responding with an error\n";
  std::string errorbody = "<html>\n" + error + "\n</html>\n";
  curReq->filesize = errorbody.length();
  std::string response = getHeader(curReq, error);
  //adds a simple body for the browser to display
  response += errorbody;
  send(curReq->socket, response.c_str(), response.length(), 0);
  if (!curReq->continues) {
    close(curReq->socket);
    connectionsOpen--;
  }
  free(curReq);
}

std::string getHeader(struct Request *curReq, std::string status)
{
  std::string statusline = curReq->version + std::string(" ") + status + "\n";
  std::string serverline = "Server: Creation of Aaron M. Taylor and Devin P. Gardella for cs339 at Williams College\n";
  std::string dateline = std::string("Date: ") + currentDateTime() + " GMT\n";
  std::string contenttypeline = std::string("Content-type: ") + contentTypeForFile(curReq->requestURI);
  std::stringstream lenstr; // these lines are necessary to convert from an int to a string
  lenstr << curReq->filesize;
  std::string contentlengthline;
  if (strToUpper(curReq->method).compare("OPTIONS") != 0){
    contentlengthline = std::string("Content-Length: ") + lenstr.str() + std::string("\n");
  }
  //if the method is options, we have to return a body length of zero 
  else {
    contentlengthline = std::string("Content-Length: ") + std::string("0\n");
  }
  // other headers that we want to support can be added here as well

  std::string complete =   statusline
			 + serverline
			 + dateline
                         + contentlengthline + "\n";
  std::cout << complete;
  return complete;
}

int doesListenMore(Request *curReq)
{
  int version = atoi(curReq->version.substr(7,1).c_str());
  if (version == 1) {
    std::cout << "-> Should continue to listen on this socket\n";
    return 1;
  } else {
    shutdown(curReq->socket,SHUT_RD);
    std::cout << "XX Socket is being closed\n";
    return 0;
  }
}

std::string contentTypeForFile(std::string filename)
{
  //finds the position right after the '.' character and finds the length to the end of the string
  int pos = filename.find(".") + 1;
  int len = filename.length() - pos;
  std::string extstr = filename.substr(pos,len);
  //allows for case insensitive compare
  extstr = strToUpper(extstr);
  //handles various cases of official content types
  //add more cases to handle more types
  if (extstr.compare("HTML") == 0) {
    return std::string("text/html");
  } else if (extstr.compare("TXT") == 0) {
    return std::string("text/plain");
  } else if ((extstr.compare("JPG") == 0) || (extstr.compare("JPEG") == 0)) {
    return std::string("image/jpeg");
  } else if ((extstr.compare("PNG") == 0)) {
    return std::string("image/png");
  } else if (extstr.compare("GIF") == 0) {
    return std::string("image/gif");
  } else if (extstr.compare("M4R") == 0) {
    return std::string("audio/m4r");
  } else {
    return std::string("");
  }
}

//Returns the appropriate amount of time before the timeout would occur.
int getTimeout(){
  //The larger the event queue, the smaller the timeout? 
  int timeout = (3*TIMEOUT)/4 + (TIMEOUT/4)*( (100 - connectionsOpen) / 100 );
    std::cout << "timeout for connectionsOpen: "<< connectionsOpen <<" is: "<< timeout <<"\n";
    return timeout;
}

int fileLength(std::ifstream &file)
{
   if (file.is_open()) {
     file.seekg(0, std::ifstream::end);
     
     int end = file.tellg();
     file.seekg(0, std::ios::beg);
     return end;
   } else {
     std::cout << "file must be open to determine size\n";
     return -1;
  }
}

// copied in from: http://stackoverflow.com/questions/997946/
const std::string currentDateTime()
{
  time_t     now = time(0);
  struct tm  tstruct;
  char       buf[80];
  tstruct = *localtime(&now);
  strftime(buf, sizeof(buf), "%d %b %Y %X", &tstruct);
  return buf;
}

//this method ensures that the URI is valid
std::string URIGuard(std::string handle)
{
  std::string fullpath = rootDirectory + handle;
  //tests for higher directory access
  if(handle.find("..",0,2) != std::string::npos){
    std::cout << "found a \"..\" in " << handle << "\n";
    return "403 Forbidden";
  }
  struct stat s;
  if ( stat((char *) fullpath.c_str(), &s) == 0 ) {
    if (S_ISREG(s.st_mode) && (S_IROTH & s.st_mode) ) {
      // file is a regular file with universal read permissions
    } else {
      // request does not correspond to a regular file
      std::cout << "403 Forbidden\n";
      if (S_ISREG(s.st_mode)) std::cout << "IS REG";
      return "403 Forbidden";
    }
  } else {
    // file does not exist
    std::cout << "404 Not Found\n";
    return "404 Not Found";
  }
  return "200 OK";
}

//uses regex's to test whether the expression is valid
int regexGuard(std::string reqstr) 
{  
  //creates an uppercase guard of reqstr for testing the method
  char tmpbuf[reqstr.length()];
  reqstr.copy(tmpbuf,reqstr.length(),0);
  std::string tmpstr = std::string(tmpbuf);
  tmpstr = strToUpper(tmpstr);
  std::regex methodGuard = std::regex("( )*((G|g)(E|e)(T|t)|(H|h)(E|e)(A|a)(D|d))( )+(.*)");
  //std::regex methodGuard = std::regex("(GET|HEAD|OPTIONS)( )+(.*)", std::regex_constants::icase);

  if (std::regex_match (reqstr,methodGuard)) {
    std::cout << "reqstr has a valid method\n";
    //guard for the rest of the request
    std::regex guard = std::regex("(.*)( )+/(.*)( )+((H|h)(T|t)(T|t)(P|p))/1\\.(.*)");
    if (std::regex_match (reqstr,guard)) {
      return 1;
    } else {
      //Here we know that the method called was Get or Head, but it failed the HTTP part 
      // and/or the file part...
    }
    return 0;
  } else {
    //This suggest it is a method we do not know about as it was not Get or Head...
    std::regex options = std::regex("( )*(O|o)(P|p)(T|t)(I|i)(O|o)(N|n)(S|s)( )+(.*)");
    if (std::regex_match (reqstr,options)){
      std::cout << "Was an option";
      return 1;
    }
  }
  return 0;
}

// string parsing utility methods
std::string strToUpper(std::string given)
{
  char* cstr = (char *)given.c_str();
  for ( int p = 0; p < given.length(); p++) {
    cstr[p] = toupper(cstr[p]);
  }
  return std::string(cstr);
}


