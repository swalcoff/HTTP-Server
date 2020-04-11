#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <queue>
#include <semaphore.h>

#define PORT 8080

#include <iostream>
using namespace std;

// GLOBAL VARIABLES:
//queue of clients
queue <int> sockQ;
// semaphore
sem_t mutex;
sem_t logMut;
//log file info
char *logFile;
bool logBool = false;
bool cacheBool = false;
// offset
int offset = 0;

//cache objects
class cacheFile {
	public:
		char *fileName = new char[50];
		char *fileContents;
};

//queue of files i.e. the cache
queue <cacheFile> cache;

void logGet(char *cmd, char *file, char *error, bool isInCache)
{
	char buff[4000] = "";
	if (error == NULL)
	{
		strcat(buff, cmd);
		strcat(buff, " ");
		strcat(buff, file);
		strcat(buff, " ");
		strcat(buff, "length 0");
		if(cacheBool)
		{
			if(isInCache)
			{
				strcat(buff, " [was in cache]\n");
			} else
			{
				strcat(buff, " [was not in cache]\n");
			}
		}
		strcat(buff, "========\n");
	} else 
	{
		strcat(buff, "FAIL: ");
		strcat(buff, cmd);
		strcat(buff, " ");
		strcat(buff, file);
		strcat(buff, " ");
		strcat(buff, error);
		strcat(buff, "\n");
		strcat(buff, "========\n");
	}
	int fD;
	fD = open(logFile, O_CREAT | O_RDWR, 0666);
	sem_wait(&logMut);
	offset += pwrite(fD, buff, strlen(buff), offset);
	sem_post(&logMut);
	close(fD);
}
void logPut(char *cmd, char *file, char* len, char *error, char *buff2, bool isInCache)
{
	int size = stoi(len);
	sprintf(len, "%d", size);
	int tempOff;
	char *buff = new char[size * 4];
	if (error != NULL)
	{
		strcat(buff, "FAIL: ");
		strcat(buff, "cmd");
		strcat(buff, " ");
		strcat(buff, file);
		strcat(buff, " ");
		strcat(buff, error);
		strcat(buff, "\n");
		strcat(buff, "========\n");
	} else 
	{
		strcat(buff, cmd);
		strcat(buff, " ");
		strcat(buff, file);
		strcat(buff, " length ");
		strcat(buff, len);
		if(cacheBool)
		{
			if(isInCache)
			{
				strcat(buff, " [was in cache]");
			} else
			{
				strcat(buff, " [was not in cache]");
			}
		}
		strcat(buff, "\n");
		int fD = open(logFile, O_CREAT | O_RDWR, 0666);
		int j = 0;
		char *padded = new char[20];
		char *temp = new char[100];
		for(int i = 0; i < (size + size % 20)/20; i++)
		{
			sprintf(padded, "%08d", i * 20);
			strcat(padded, " ");
			strcat(buff, padded);

			while(((j + 1) % 20 != 0) && j < (size -1))
			{
				sprintf(temp, "%02x", buff2[j]);
				strcat(temp, " ");
				strcat(buff, temp);
				j++;
			}
			sprintf(temp, "%02x", buff2[j]);
			strcat(temp, "\n");
			strcat(buff, temp);
			j++;
			
		}
		strcat(buff, "========\n");

		sem_wait(&logMut);
		tempOff = offset;
		offset += strlen(buff);
		sem_post(&logMut);
		tempOff += pwrite(fD, buff, strlen(buff), tempOff);
		close(fD);
	}
}

cacheFile putCache(char *fileName, int client_socket, char *charSize, int intSize, bool isInCache)
{
	if(false)
	{
		send(client_socket, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n", 
			strlen("HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n"), 0);
		if(logBool)
		{
			logPut((char *)"PUT", fileName, charSize, (char *)"HTTP/1.1 --- response 403", NULL, false);
		}
	}else
	{
		char * logBuff = new char[intSize];
		logBuff[0] = '\0';
		char fileBuffer[32000];
		int rtemp = 0;
		while (rtemp < intSize)
		{
			rtemp += recv(client_socket, fileBuffer, 32000, 0);
			// write(bytesRead, fileBuffer, strlen(fileBuffer));
			strcat(logBuff, fileBuffer);
			memset(fileBuffer, 0, 32000);
		}
		if(logBool)
		{
			logPut((char *)"PUT", fileName, charSize, NULL, logBuff, isInCache);
		}
		send(client_socket, "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n", 
				strlen("HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n"), 0);
		cacheFile retFile;
		strcpy(retFile.fileName, fileName);
		retFile.fileContents = new char[intSize];
		retFile.fileContents = logBuff;
		return retFile;
	}
}

cacheFile getCache(char *fileName, int client_socket, bool isInCache)
{
	int rtemp;
	char fileBuffer[32000];
	int bytesRead = open(fileName, O_RDWR);
	//send error if file does not exist
	if(bytesRead < 0)
	{	
		send(client_socket, "HTTP/1.1 404 File Not Found\r\nContent-Length: 0\r\n\r\n", 
			strlen("HTTP/1.1 404 File Not Found\r\nContent-Length: 0\r\n\r\n"), 0);
		if(logBool) 
		{	
			logGet((char *)"GET", fileName, (char *)"HTTP/1.1 --- response 404", false);
		}
		close(bytesRead);
		close(client_socket);
		cacheFile retFile;
		retFile.fileName = (char *)"NULL";
		return retFile;
		// return NULL;
	}

	//get file size
	struct stat st;
	stat(fileName, &st);
	int size = st.st_size;
	//create and send response header
	char int_str[32] = {0};
	sprintf(int_str, "%d", size);
	char response[100] = "HTTP/1.1 200 OK\r\nContent-Length: ";
	strcat(response, int_str);
	strcat(response, "\r\n\r\n");
	send(client_socket, response, strlen(response), 0);

	if(logBool) 
	{
		logGet((char *)"GET", fileName, NULL, isInCache);
	}
	//iteratively send contents of file
	// char * logBuff = new char[size];
	cacheFile retFile;
	// retFile.fileName[0] = '\0';
	strcpy(retFile.fileName, fileName);
	retFile.fileContents = new char[size];
	retFile.fileContents[0] = '\0';
	while((rtemp = read(bytesRead, fileBuffer, sizeof(fileBuffer))) > 0)
	{
		send(client_socket, fileBuffer, rtemp, 0);
		strcat(retFile.fileContents, fileBuffer);
		memset(fileBuffer, 0, 32000);
	}
	memset(fileBuffer, 0, bytesRead);
	close(bytesRead);
	return retFile;
}

void removeFromCache(cacheFile fObj)
{
	cache.pop();
	//write to disk!!
	int bytesRead = open(fObj.fileName, O_CREAT | O_RDWR, 0666);
	write(bytesRead, fObj.fileContents, strlen(fObj.fileContents));
	close(bytesRead);
}

void cacheOp(char *fileName, int client_socket, char *cmd, char *charSize, int intSize)
{
	int cacheSize = cache.size();
	int i = cacheSize;
	cacheFile temp;
	cacheFile thisFile;
	bool isInCache = false;
	//search queue for fileName
	while( i > 0 )
	{
		temp = cache.front();
		cache.pop();
		if (strcmp(fileName, temp.fileName) == 0)
		{
			thisFile = temp;
			isInCache = true;
		} else
		{
			cache.push(temp);
		}
		i--;
	}
	if ( !isInCache && cacheSize >= 4)
	{
		//if it's not already in the queue and the cache is full, make space
		removeFromCache(cache.front());
	}
	//create new cacheFile using thisFile
	if(strcmp(cmd, "PUT") == 0)
	{
		thisFile = putCache(fileName, client_socket, charSize, intSize, isInCache);
	} else if (strcmp(cmd, "GET") == 0)
	{
		if (isInCache)
		{
			//get file size
			int size = strlen((char *)thisFile.fileContents);
			//create and send response header
			char int_str[32] = {0};
			sprintf(int_str, "%d", size);
			char * response = new char[size + 100];
			strcat(response, "HTTP/1.1 200 OK\r\nContent-Length: ");
			strcat(response, int_str);
			strcat(response, "\r\n\r\n");
			strcat(response, thisFile.fileContents);
			send(client_socket, response, strlen(response), 0);
			// send(client_socket, thisFile.fileContents, sizeof(thisFile.fileContents), 0);
			if(logBool) 
			{
				logGet((char *)"GET", fileName, NULL, isInCache);
			}
		} else
		{
			thisFile = getCache(fileName, client_socket, isInCache);
			if(strcmp(thisFile.fileName, "NULL") == 0)
			{
				return;
			}
		}
	}
	//add to cache into free space

	cache.push(thisFile);
}

//thread function for get and put operations
// run infinitely so threads never stop checking to see if queue is empty.
void* threadFunc(void* arg)
{	
	while(true)
	{
		// down on mutex and retrieve client socket
		sem_wait(&mutex);
		int client_socket = sockQ.front();
		sockQ.pop();

		char buff[4000] = {0};
		int reader = 0;
		reader = read(client_socket, buff, 4000);

		if(reader < 1){
			continue;
		}

		printf("%s\n", buff);
		//tokenize by newline symbol to get each line
		char * tokens[50] = {0};
		char *tok = strtok(buff, "\n");

		for(int i = 0; tok != 0; i++){
			tokens[i] = tok;
			tok = strtok(0, "\n");
		}

		// tokenize line one for request and file name
		char * fstLine[50] = {0};
		tok = strtok(tokens[0], " ");
		for(int i = 0; tok != 0; i++){
			fstLine[i] = tok;
			tok = strtok(0, " ");
		}
		char * command = fstLine[0];
		char * name = fstLine[1];

		// char nonpName = fstLine[1];


		// checks that the name is 27 char long and uses allowed symbols
		int length = strlen(name);
		bool nameCheck = true;
		// if(length != 27)
		// {
		// 	nameCheck = false;
		// }
		// for(int i = 0; i < length; i++){
		// 	int ascii = int(name[i]);
		// 	if(!((65 <= ascii && ascii <= 90) or (97 <= ascii && ascii <= 122) or 
		// 					(48 <= ascii && ascii <= 57) or ascii == 95 or ascii == 45))
		// 	{
		// 		nameCheck = false;
		// 		i = length;
		// 	} 
		// }

		if(!nameCheck)
		{
			send(client_socket, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n", 
					strlen("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n"), 0);
			close(client_socket);
			if(logBool) 
				{	
					logGet(command, name, (char *)"HTTP/1.1 --- response 400", false);
				}
			continue;
		}

		char fileBuffer[32000];
		int bytesRead = 0;
		int rtemp = 0;


		//checks the type of request; either PUT or GET.
		if(strcmp(command, "GET") == 0)
		{
			if(cacheBool)
			{
				// all cache operations happen here
				cacheOp(name, client_socket, command, NULL, 0);
			} else
			{
				bytesRead = open(name, O_RDWR);
				//send error if file does not exist
				if(bytesRead < 0)
				{	
					send(client_socket, "HTTP/1.1 404 File Not Found\r\nContent-Length: 0\r\n\r\n", 
						strlen("HTTP/1.1 404 File Not Found\r\nContent-Length: 0\r\n\r\n"), 0);
					if(logBool) 
					{	
						logGet(command, name, (char *)"HTTP/1.1 --- response 404", false);
					}
					close(bytesRead);
					close(client_socket);
					continue;
				}

				//get file size
				struct stat st;
				stat(name, &st);
				int size = st.st_size;
				//create and send response header
				char int_str[32] = {0};
				sprintf(int_str, "%d", size);
				char response[100] = "HTTP/1.1 200 OK\r\nContent-Length: ";
				strcat(response, int_str);
				strcat(response, "\r\n\r\n");
				send(client_socket, response, strlen(response), 0);
				if(logBool) 
				{
					logGet(command, name, NULL, false);
				}
				//iteratively send contents of file
				while((rtemp = read(bytesRead, fileBuffer, sizeof(fileBuffer))) > 0)
				{
					send(client_socket, fileBuffer, rtemp, 0);
					memset(fileBuffer, 0, 32000);
				}
				memset(fileBuffer, 0, bytesRead);
				close(bytesRead);
			}
		} else if(strcmp(command, "PUT") == 0)
		{	
			// tokenize fifth line for the PUT size
			char * fthLine[50] = {0};
			tok = strtok(tokens[4], " ");
			for(int i = 0; tok != 0; i++)
			{
				fthLine[i] = tok;
				tok = strtok(0, " ");
			}
			char * charSize = fthLine[1];
			int intSize = stoi(charSize);
			if(cacheBool)
			{
				//all cache operations
				cacheOp(name, client_socket, command, charSize, intSize);
			} else
			{
				// open file, receive message, and write to file or send error code
				bytesRead = open(name, O_CREAT | O_RDWR, 0666);
				if(bytesRead < 0)
				{
					send(client_socket, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n", 
						strlen("HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n"), 0);
					if(logBool) 
					{	
						logPut(command, name, charSize, (char *)"HTTP/1.1 --- response 403", NULL, false);
					}
				}else
				{
					char * logBuff = new char[intSize];
					while (rtemp < intSize)
					{
						rtemp += recv(client_socket, fileBuffer, 32000, 0);
						write(bytesRead, fileBuffer, strlen(fileBuffer));
						strcat(logBuff, fileBuffer);
						memset(fileBuffer, 0, 32000);
					}
					if(logBool)
					{	
						logPut(command, name, charSize, NULL, logBuff, false);
					}
					send(client_socket, "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n", 
							strlen("HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n"), 0);
				}
				close(bytesRead);
			}
		} else
		{
			send(client_socket, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n", 
					strlen("HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n"), 0);
			if(logBool) 
				{	
					logGet(command, name, (char *)"HTTP/1.1 --- response 500", false);
				}
		}
		close(client_socket);
	}
	return NULL;
}

int main(int argc, char **argv) {
	int server_fd, client_socket, intTSize = 4;
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	char *charTSize;

	//check command line arguments
	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			if(argv[i][1] == 'N')
			{
				charTSize = argv[i+1];
				intTSize = stoi(charTSize);
			} else if (argv [i][1] == 'l')
			{
				logFile = argv[i+1];
				logBool = true;
			} else if (argv[i][1] == 'c')
			{
				cacheBool = true;
			}
		}
	}

	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );

	if(bind(server_fd, (struct sockaddr *)&address, addrlen) < 0)
	{
		perror("failure to bind"); 
        exit(EXIT_FAILURE);
	}

	// number of threads
 	int numThreads = intTSize;
 	// Thread ID:
	pthread_t *tids  = new pthread_t[numThreads];
	// intialize mutex
 	sem_init(&mutex, 0, 0);
 	sem_init(&logMut, 0, 1);
	// create each thread
	for(int i = 0; i < 1; i++)
	{
		pthread_create(&tids[i], NULL, threadFunc, &i);
	}

	if(listen(server_fd, 3)<0){
		perror("listen failure"); 
        exit(EXIT_FAILURE);
	}

	while(true)
	{
		if ((client_socket = accept(server_fd, (struct sockaddr *)&address,  
                   (socklen_t*)&addrlen))<0) 
		{
	        perror("accept"); 
	        continue; 
		}
		sockQ.push(client_socket);
	    sem_post(&mutex);
	}
	return 0;
}
