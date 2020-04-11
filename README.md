# HTTP-Server
Implemented in C++, this is an HTTP server that is multiple thread capable. Each thread can run synchronously. This server can maintain a cache and logfile for said cache to increase efficiency of client requested reads and writes. Cache and logfile are optional. The client may also choose the number of threads. This server runs on port 8080 by default. Source code is available in httpserver.cpp. **Windows or linux is highly recommended**.

## Instructions:
First, clone project or download ServerApp.exe

### Server Side
Store ServerApp.exe in its own folder so that all information uploaded by the client is stored safely in said folder. Run ServerApp.exe in terminal using this command:  
`./ServerApp`
  
If you'd like to compile manually:  
`clang++ -std=gnu++11 -Wall -Wextra -Wpedantic -Wshadow httpserver.cpp`
  
**FLAGS:**  
**-N \<numThreads\>**: controls the number of threads to be run on the server. default of 4 threads if this flag is not specifed.  
**-l**: enables logging. If logging is active, log records will contain the type of request, whether the page was in cache when the request is received, and contents of the file in hex.  
**-c**: enables caching.  
  
### Client Side
Type these commands in terminal...  
   
**PUT request:**  
`curl -T localfile http://localhost:8080 --request-target filename -v`  
where localfile is the file you'd like to send and filename is name you'd like to give to the targetfile in the server. If the file already exists, it will be overwriten.  
  
**GET request:**  
`curl http://localhost:8080 --request-target filename -v`  
where filename is the name of the file you'd like to retrieve.  
  
For both types of requests, appropriate http status codes will be returned and filename should either be a txt or pdf file.
