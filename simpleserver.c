#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>

#define ServerURL "127.0.0.1"
#define ServerSocket 8080
#define BacklogSize 10


char pageNotFoundPage[] =
"HTTP/1.1 404 PageNotFound\r\n"
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>Page Not Found</title>\n"
"</head>\n"
"<body><center><h1>Page Not Found!</h1></center>\n"
"</body></html>\n";

char webpageHeaderOK[] = 
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n";

enum httpRequestType {
    UNKOWN, GET, POST
};

enum httpRequestType getHeaderPathAndType(char* buffer, char* outputString){
    enum httpRequestType resp;
    int firstSpaceLoct = 0;

    //Get the HTTP Request type    
    if ( !strncmp(buffer, "GET", 3) ){
        resp = GET;
        firstSpaceLoct = 4;
    }else if ( !strncmp(buffer, "POST", 4) ){
        resp = POST;
        firstSpaceLoct = 5;
    }else{
        resp = UNKOWN;
        return resp;
    }

    char* secondSpaceChar = strchr(buffer, ' ');
    secondSpaceChar = strchr(secondSpaceChar + 1, ' ');
    int secondSpaceIndex = secondSpaceChar - buffer;
    //printf("len : %d\n", secondSpaceIndex - firstSpaceLoct );

    //printf("oldpath : %s\n", outputString);

    int i;
    for( i = firstSpaceLoct; i < secondSpaceIndex; i++  ){
        outputString[i - firstSpaceLoct - 1] = buffer[i];
    }

    //printf("oldpath : %s\n", outputString);

    //return
    return resp;
}

//https://stackoverflow.com/a/5446759
int fileSize(char* filename){
    FILE *fp = NULL;
    fp = fopen(filename, "r");
    int prev = ftell(fp);
    fseek(fp, 0L, SEEK_END);
    int sz = ftell(fp);
    fseek(fp,prev,SEEK_SET); //go back to where we were
    return sz;
}

void sendFileFromDisk(char* filename, int client_FileDesc)
{
    if ( access(filename, F_OK) == 0 ){
        int fileSizeinBytes = fileSize(filename);

        //check if the file is a html

        write(client_FileDesc, webpageHeaderOK, sizeof(webpageHeaderOK) - 1);

        int file_fileDescriptor = open(filename, O_RDONLY);
        sendfile(client_FileDesc, file_fileDescriptor, NULL, fileSizeinBytes);
        close(file_fileDescriptor);

    }else{
        //404 not found
        write(client_FileDesc, pageNotFoundPage, sizeof(pageNotFoundPage) - 1);
    }
    
}


//Main function
void main(){

    //variable to hold the current server adress
	struct sockaddr_in server_Address;
    struct sockaddr_in client_Address;

    //get the size of our socketadress struct
	socklen_t sin_len = sizeof(client_Address);

    int server_FileDescriptor, client_FileDescriptor, fdimg;    
    char httpRequestBuffer[2048];

    //Check of the socket description
    //https://docs.oracle.com/cd/E19620-01/805-4041/6j3r8iu2l/index.html
    server_FileDescriptor = socket(AF_INET, SOCK_STREAM, 0);

    //https://en.wikipedia.org/wiki/File_descriptor
    if ( server_FileDescriptor < 0 )
    {
        perror("Socket Not Found");
        exit(-1);
    }
    
    //https://linux.die.net/man/2/setsockopt
    int setscoketon = 1;
    setsockopt( server_FileDescriptor, SOL_SOCKET, SO_REUSEADDR, &setscoketon, sizeof(int) );

    //set the server to lisen at localhost:8080
    server_Address.sin_family = AF_INET;
	server_Address.sin_addr.s_addr = inet_addr(ServerURL);
	server_Address.sin_port = htons(ServerSocket);

    int socketBinding = 
        bind( 
            server_FileDescriptor, 
            (struct sockaddr *) &server_Address, 
            sizeof(server_Address) 
        );

    //if it negatvie then we failed to bind
    if( socketBinding == -1)
	{
		perror("failed to bind to socket");
		close(server_FileDescriptor);
	}


    if( listen(server_FileDescriptor, BacklogSize) == -1 )
	{
		perror("failed to lisen to the communication");
		close(server_FileDescriptor);
		exit(-1);
	}

    //everything is now set, time to loop until closed
    printf("Server now online and lisening on %s:%d\n", ServerURL, ServerSocket);

    while (1)
    {
        client_FileDescriptor = accept(server_FileDescriptor, (struct sockaddr *) &client_Address, &sin_len);

        if (client_FileDescriptor == -1)
        {
            printf("Connection Failed To Connect.....\n");
            continue;
        }

        
        if( !fork() ){
            //New thread / child process

            /*
                fork creates a duplicate of the current process, 
                it does not spawn a new thread. 
                Because the new process has all the same handles (file descriptors) as what it forked from, 
                it needs to close the ones it doesn't need
            */

            //close the server on the child thread
            close(server_FileDescriptor);

            //Logs that we have a new connection
            
            //set allocate a new memory for the buffer and read from client
			memset(httpRequestBuffer, 0, 2048);
			read(client_FileDescriptor, httpRequestBuffer, 2047);

            char requestPath[32] = {'\0'};
            enum httpRequestType reqType;

            reqType = getHeaderPathAndType(httpRequestBuffer, requestPath);

            //getHeader(httpRequestBuffer, &head);

            if (reqType == GET){
                //if its get then do this !

                printf("[%s] - [GET] - [%s]\n", inet_ntoa(client_Address.sin_addr), requestPath );
                
                //check if its the index or another file
                if ( strlen(requestPath) == 0 ){
                    //index
                    sendFileFromDisk("index.html", client_FileDescriptor);

                }else{
                    //another file
                    sendFileFromDisk(requestPath, client_FileDescriptor);
                }
            
            }else{
                // sorry we only support GET!
            }


            close(client_FileDescriptor);

            exit(0);

        }
        
        close(client_FileDescriptor);
    }
    

}