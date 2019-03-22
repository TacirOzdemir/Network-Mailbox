#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#define MAXDATASIZE 512
#define DEFAULT_PORT "24444"
#define HOSTIP "141.135.144.245"
#define WIN32_LEAN_AND_MEAN

char msg[25];
char buf[MAXDATASIZE];

int __cdecl main(int argc, char **argv){
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;

    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);

    if(iResult != 0){
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(HOSTIP, DEFAULT_PORT, &hints, &result);

    if(iResult != 0){
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for(ptr=result; ptr!=NULL; ptr=ptr->ai_next){

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

        if(ConnectSocket == INVALID_SOCKET){
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);

        if(iResult == SOCKET_ERROR){
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if(ConnectSocket == INVALID_SOCKET){
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    // Receive until the peer closes the connection
    do{
        // Code voor msg te sturen
        printf("Geef message in: ");
        scanf("%s", &msg);
        printf("msg = %s\r\n", msg);

        send(ConnectSocket, msg, strlen(msg), 0);

        //get msg back from server
        iResult = recv(ConnectSocket, buf, MAXDATASIZE-1, 0);

        buf[iResult] = '\0';

        printf("client: sent '%s'\n",buf);

    } while(iResult > 0);

    // Cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}
