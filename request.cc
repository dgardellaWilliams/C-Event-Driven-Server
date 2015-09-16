/**
 * (c) 2013 Aaron M. Taylor & Devin Gardella
 *
 * This class holds all the necessary information for each request in
 * they are to be stored in the event queue
 */

#include <stdlib.h>
#include <string.h>
#include <string>

#include <request.h>

#define REQ_SIZ 1600

class Request {
  int socket;
  std::string reqstr;
  int finished;
public:
  Request( int socketIn, std::string requestIn );
  int getSocket();
  // need a method to get the next step
  
  // this class should keep track of what has been completed
  // and what still needs to be completed
  std::string getRequest();
  // tells whether the response to this request has been completed
  int isFinished();

private:
  int parse(std::string reqStrIn);
  std::string strToUpper(std::string given);
};

void Request::newRequest( int socketIn, std::string requestIn )
{
  socket = socketIn;
  reqstr = requestIn;
  finished = 0;
}

int Request::getSocket()
{ return socket; }

std::string Request::getRequest()
{ return reqstr; }

int request::isFinished()
{ return finished; }

//returns 0 if a request is invalid??  
//returns 1 if a request is valid and does the request??                   
int Request::parse(std::string request)
{
  std::string temp = request.copy((char *)malloc(1600));
  //A request line has three parts, seperated by spaces: a method name,
  //the local path of the requested resource, and the version of HTTP being used.

  std::string space = " ";
  int pos = 0;
  
  if (temp.length() == 0){
    //something horrible went wrong. I don't think this can happen.
  }

  //method = all characters of temp before the first space           
  std::string method = temp.substr(0, (pos = temp.find(space)));
  //erase that part of temp, including the space               
  temp.erase(0,pos + 1);

  //if there is only a request
  if (temp.length() == 0) {
    return 0;
  }

  //link = all characters of temp before the second space       
  std::string linkUrl = temp.substr(0, (pos = temp.find(space,pos)));
  temp.erase(0,pos + 1);

  //So we got two things...
  if (temp.length() == 0){
    
  }
  
  int x = std::min((temp.find(space)), (temp.find("\n")));
  std::string version = temp.substr(0, x);

  //now we have the method, the link they want and the type called.    
  
  printf("%s\n", method.c_str());
  printf("%s\n", linkUrl.c_str());
  printf("%s\n", version.c_str());

  if (method.compare(std::string("get")) != 0){
    printf("%s", "Is not Get");
  }
  
  //to make this more accurate, deal with having extra spaces inbetween elements,
  //deal with the case where one of these could fail (i.e. temp is nil before we find type
  //or if there are no spaces <- important one).

  //A server SHOULD return the status code 405 (Method Not Allowed) if the method is known 
  //by the orgin server but not allowed for the requested resource. 

  //A server should return the status code 501 (Not Implemented) if the method is unrecognized
  //or not implemented by the origin server

  //The methods GET and HEAD MUST be supported by all general-purpose servers.
  //The other methods are optional.

  return 0;
}

// string parsing utility methods

std::string Request::strToUpper(std::string given)
{
  char* cstr = (char *)given.c_str();
  for ( int p = 0; p < given.length(); p++) {
    cstr[p] = toupper(cstr[p]);
  }
  return std::string(cstr);
}
