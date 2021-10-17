#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <ctype.h>
#include <cstdio>
#include <iostream>
#include "../PeerToPeer/HashMap_Client.cpp"

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 27016
#define MAX_USERNAME 25
#define MAX_MESSAGE 400
#define MAX_ADDRESS 50
#define MAX_DIRECTLY_MESSAGE 510
#define MAX_MESSAGE_DIRECTLY 484

#define MAX_DIRECTLY_CONNECTIONS 10

#define SERVER_IP_ADDRESS "127.0.0.1"

#define SAFE_DELETE_HANDLE(a) if(a){CloseHandle(a);}

// Initializes WinSock2 library
// Returns true if succeeded, false otherwise.
bool InitializeWindowsSockets();
void EndOfProgram();

HANDLE FinishSignal;
HANDLE FinishThreadSignal;
HANDLE StartMainSignal;
HANDLE StartSendMessageSignal;
HANDLE FinishSignal_Directly;

SOCKET connectSocket;  // socket used to communicate with server

SOCKET connectSockets_directly[MAX_DIRECTLY_CONNECTIONS];  // sockets used to communicate with other clients
int count_connect_sockets;

SOCKET listenSocket;  // Socket used for listening for other clients 
SOCKET acceptedSockets[MAX_DIRECTLY_CONNECTIONS];  // Socket used for communication with other client
int counter_accepted_clients;

CRITICAL_SECTION critical_section_hash_map;
CRITICAL_SECTION critical_section_std;
CRITICAL_SECTION critical_section_server;
CRITICAL_SECTION critical_section_connect_directly;
CRITICAL_SECTION critical_section_accept_directly;


struct Client_Information_Directly
{
	unsigned char my_username[MAX_USERNAME];
	unsigned char client_username[MAX_USERNAME];
	unsigned char message[MAX_MESSAGE];
	unsigned char listen_address[MAX_ADDRESS];
	unsigned int listen_port;
};

struct Directly_Message {
	unsigned char message[MAX_DIRECTLY_MESSAGE];
	unsigned char flag[2];
};

struct Element* HashMap[MAX_DIRECTLY_CONNECTIONS];

DWORD WINAPI thread_function(LPVOID parametri) {

	int iResult;
	char recvbuf[DEFAULT_BUFLEN];

	while (WaitForSingleObject(FinishThreadSignal, 1) == WAIT_TIMEOUT) {

		EnterCriticalSection(&critical_section_server);
		iResult = recv(connectSocket, recvbuf, DEFAULT_BUFLEN, 0);
		LeaveCriticalSection(&critical_section_server);
		if (iResult > 0)
		{
			EnterCriticalSection(&critical_section_std);
			printf("%s\n", recvbuf);
			LeaveCriticalSection(&critical_section_std);
		}
		else if (iResult == 0)
		{
			EnterCriticalSection(&critical_section_std);
			printf("\nIzgubljena je konekcija sa serverom...\n");
			LeaveCriticalSection(&critical_section_std);
			ReleaseSemaphore(FinishSignal, 2, NULL);
			break;
		
		}
		else
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK) {

				continue;
			}
			else {

				EnterCriticalSection(&critical_section_std);
				printf("\nIzgubljena je konekcija sa serverom...\n");
				LeaveCriticalSection(&critical_section_std);
				ReleaseSemaphore(FinishSignal, 2, NULL);
				break;
				
			}
		}

	}

	return 0;
}

DWORD WINAPI function_recv_directly(LPVOID parametri) {

	int iResult;
	char recvbuf[DEFAULT_BUFLEN];
	Client_Information_Directly *client_informations;

	while (true) {

		EnterCriticalSection(&critical_section_server);
		iResult = recv(connectSocket, recvbuf, DEFAULT_BUFLEN, 0);
		LeaveCriticalSection(&critical_section_server);
		if (iResult > 0)
		{
			client_informations = (Client_Information_Directly*)recvbuf;

			if (strcmp((char*)client_informations->listen_address, "*\0") == 0) {

				EnterCriticalSection(&critical_section_std);
				printf("%s\n", client_informations->message);
				LeaveCriticalSection(&critical_section_std);

			}
			else if (strcmp((char*)client_informations->listen_address, "/\0") == 0) {

				EnterCriticalSection(&critical_section_std);
				printf("%s\n", client_informations->message);
				LeaveCriticalSection(&critical_section_std);

				ReleaseSemaphore(StartMainSignal, 1, NULL);

			}
			else {

				if (count_connect_sockets >= MAX_DIRECTLY_CONNECTIONS) {

					EnterCriticalSection(&critical_section_std);
					printf("Nema mesta za nove klijente!\n");
					LeaveCriticalSection(&critical_section_std);

				}
				else {

					EnterCriticalSection(&critical_section_std);
					printf("Povezivanje sa zeljenim klijentom je u toku...");
					LeaveCriticalSection(&critical_section_std);

					Client_Information_Directly *client_informations_1 = (Client_Information_Directly*)parametri;
					strcpy((char*)client_informations_1->my_username, (char*)client_informations->my_username);
					strcpy((char*)client_informations_1->client_username, (char*)client_informations->client_username);
					strcpy((char*)client_informations_1->message, (char*)client_informations->message);
					strcpy((char*)client_informations_1->listen_address, (char*)client_informations->listen_address);
					client_informations_1->listen_port = client_informations->listen_port;

				}

				ReleaseSemaphore(StartSendMessageSignal, 1, NULL);

			}

		}
		else if (iResult == 0)
		{
			EnterCriticalSection(&critical_section_std);
			printf("\nIzgubljena je konekcija sa serverom...\n");
			LeaveCriticalSection(&critical_section_std);

			ReleaseSemaphore(FinishSignal_Directly, 3, NULL);
			ReleaseSemaphore(FinishSignal, 1, NULL);
			break;

		}
		else
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK) {

				continue;
			}
			else {

				EnterCriticalSection(&critical_section_std);
				printf("\nIzgubljena je konekcija sa serverom...\n");
				LeaveCriticalSection(&critical_section_std);

				ReleaseSemaphore(FinishSignal_Directly, 3, NULL);
				ReleaseSemaphore(FinishSignal, 1, NULL);
				break;
				
			}
		}

	}

	return 0;
}

DWORD WINAPI function_send_message_directly(LPVOID parametri) {

	Client_Information_Directly *information = (Client_Information_Directly*)parametri;
	char *directly_message = (char*)malloc(MAX_MESSAGE_DIRECTLY);
	Directly_Message directly_message_packet;
	int iResult;
	unsigned long mode = 1;

	HANDLE semaphores[2] = { FinishSignal_Directly, StartSendMessageSignal };
	while (WaitForMultipleObjects((DWORD)2, semaphores, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {

		// create a socket
		EnterCriticalSection(&critical_section_connect_directly);
		connectSockets_directly[count_connect_sockets] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (connectSockets_directly[count_connect_sockets] == INVALID_SOCKET)
		{
			LeaveCriticalSection(&critical_section_connect_directly);
			EnterCriticalSection(&critical_section_std);
			//printf("socket failed with error: %ld\n", WSAGetLastError());
			printf("Pokusajte ponovo!\n");
			LeaveCriticalSection(&critical_section_std);

			ReleaseSemaphore(StartMainSignal, 1, NULL);
			continue;
		}
		LeaveCriticalSection(&critical_section_connect_directly);

		// create and initialize address structure
		sockaddr_in serverAddress;
		serverAddress.sin_family = AF_INET;								// IPv4
		serverAddress.sin_addr.s_addr = inet_addr((char*)information->listen_address);
		serverAddress.sin_port = htons(information->listen_port);

		// connect to other client specified in serverAddress and socket connectSocket
		EnterCriticalSection(&critical_section_connect_directly);
		if (connect(connectSockets_directly[count_connect_sockets], (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
		{
			EnterCriticalSection(&critical_section_std);
			printf("Povezivanje sa zeljenim klijentom nije uspelo, jer on vise nije dostupan.\n");
			LeaveCriticalSection(&critical_section_std);

			closesocket(connectSockets_directly[count_connect_sockets]);
			connectSockets_directly[count_connect_sockets] = INVALID_SOCKET;
			LeaveCriticalSection(&critical_section_connect_directly);

			ReleaseSemaphore(StartMainSignal, 1, NULL);
			continue;

		}
		else {

			count_connect_sockets++;
			LeaveCriticalSection(&critical_section_connect_directly);

			EnterCriticalSection(&critical_section_std);
			printf("Konekcija je uspesno ostvarena!\n");
			LeaveCriticalSection(&critical_section_std);

			ClientData *newClient = (ClientData*)malloc(sizeof(ClientData));
			strcpy((char*)newClient->name, (char*)information->client_username);
			strcpy((char*)newClient->address, (char*)information->listen_address);
			newClient->port = information->listen_port;
			strcpy((char*)newClient->socket_type, "0\0");
			EnterCriticalSection(&critical_section_hash_map);
			AddValueToHashMap(HashMap, newClient);
			//ShowHashMap(HashMap);
			LeaveCriticalSection(&critical_section_hash_map);

			strcpy((char*)directly_message_packet.flag, "0\0");
			strcpy((char*)directly_message_packet.message, (char*)information->my_username);
			EnterCriticalSection(&critical_section_connect_directly);
			iResult = send(connectSockets_directly[count_connect_sockets - 1], (char*)&directly_message_packet, sizeof(directly_message_packet), 0);
			LeaveCriticalSection(&critical_section_connect_directly);
			if (iResult == SOCKET_ERROR)
			{
				EnterCriticalSection(&critical_section_std);
				//printf("send failed with error: %d\n", WSAGetLastError());
				printf("Klijent vise nije dostupan!\n");
				LeaveCriticalSection(&critical_section_std);

				EnterCriticalSection(&critical_section_connect_directly);
				closesocket(connectSockets_directly[count_connect_sockets - 1]);
				connectSockets_directly[count_connect_sockets - 1] = INVALID_SOCKET;
				count_connect_sockets--;
				LeaveCriticalSection(&critical_section_connect_directly);

				EnterCriticalSection(&critical_section_hash_map);
				RemoveValueFromHashMap(HashMap, newClient->name);
				//ShowHashMap(HashMap);
				LeaveCriticalSection(&critical_section_hash_map);

				ReleaseSemaphore(StartMainSignal, 1, NULL);
				continue;

			}

			EnterCriticalSection(&critical_section_std);
			getchar();
			printf("Unesite poruku:\n");
			fgets(directly_message, MAX_MESSAGE_DIRECTLY, stdin);
			LeaveCriticalSection(&critical_section_std);

			directly_message[strlen(directly_message) - 1] = directly_message[strlen(directly_message)];
			strcpy((char*)directly_message_packet.flag, "1\0");
			sprintf((char*)directly_message_packet.message, "[%s]:%s", information->my_username, directly_message);

			EnterCriticalSection(&critical_section_connect_directly);
			iResult = send(connectSockets_directly[count_connect_sockets - 1], (char*)&directly_message_packet, sizeof(directly_message_packet), 0);
			LeaveCriticalSection(&critical_section_connect_directly);
			if (iResult == SOCKET_ERROR)
			{
				EnterCriticalSection(&critical_section_std);
				//printf("send failed with error: %d\n", WSAGetLastError());
				printf("Poruka nije poslata, jer klijent vise nije dostupan!\n");
				LeaveCriticalSection(&critical_section_std);
				
				EnterCriticalSection(&critical_section_connect_directly);
				closesocket(connectSockets_directly[count_connect_sockets - 1]);
				connectSockets_directly[count_connect_sockets - 1] = INVALID_SOCKET;
				count_connect_sockets--;
				LeaveCriticalSection(&critical_section_connect_directly);

				EnterCriticalSection(&critical_section_hash_map);
				RemoveValueFromHashMap(HashMap, newClient->name);
				//ShowHashMap(HashMap);
				LeaveCriticalSection(&critical_section_hash_map);
				
				ReleaseSemaphore(StartMainSignal, 1, NULL);
				continue;
				
			}
			else {

				EnterCriticalSection(&critical_section_std);
				printf("Poruka je uspesno poslata zeljenom klijentu!\n");
				LeaveCriticalSection(&critical_section_std);

			}

			EnterCriticalSection(&critical_section_connect_directly);
			iResult = ioctlsocket(connectSockets_directly[count_connect_sockets - 1], FIONBIO, &mode);
			LeaveCriticalSection(&critical_section_connect_directly);
			if (iResult != NO_ERROR) {

				//EnterCriticalSection(&critical_section_std);
				//printf("ioctlsocket failed with error: %ld\n", iResult);
				//LeaveCriticalSection(&critical_section_std);

				EnterCriticalSection(&critical_section_connect_directly);
				closesocket(connectSockets_directly[count_connect_sockets - 1]);
				connectSockets_directly[count_connect_sockets - 1] = INVALID_SOCKET;
				count_connect_sockets--;
				LeaveCriticalSection(&critical_section_connect_directly);

				EnterCriticalSection(&critical_section_hash_map);
				RemoveValueFromHashMap(HashMap, newClient->name);
				//ShowHashMap(HashMap);
				LeaveCriticalSection(&critical_section_hash_map);

				ReleaseSemaphore(StartMainSignal, 1, NULL);
				continue;

			}


		}

		ReleaseSemaphore(StartMainSignal, 1, NULL);
	}

	free(directly_message);

	return 0;

}

DWORD WINAPI function_accept_clients(LPVOID parametri) {

	Directly_Message *directly_message;
	char recvbuf[DEFAULT_BUFLEN];
	int iResult = 0;
	unsigned long mode = 1;

	// Set listenSocket in listening mode
	iResult = listen(listenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		//printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		return 1;
	}

	iResult = ioctlsocket(listenSocket, FIONBIO, &mode);
	if (iResult != NO_ERROR) {
		//printf("ioctlsocket failed with error: %ld\n", iResult);
		closesocket(listenSocket);
		return 1;
	}

	fd_set readfds;
	FD_ZERO(&readfds);

	timeval timeVal;
	timeVal.tv_sec = 1;
	timeVal.tv_usec = 0;


	while (WaitForSingleObject(FinishSignal, 1) == WAIT_TIMEOUT) {

		EnterCriticalSection(&critical_section_accept_directly);
		if (counter_accepted_clients < MAX_DIRECTLY_CONNECTIONS) {
			FD_SET(listenSocket, &readfds);
		}

		for (int a = 0; a < counter_accepted_clients; a++) {
			FD_SET(acceptedSockets[a], &readfds);
		}

		iResult = select(0, &readfds, NULL, NULL, &timeVal);
		if (iResult == 0) {
			LeaveCriticalSection(&critical_section_accept_directly);
			continue;					 
		}
		else if (iResult == SOCKET_ERROR) {

			//printf("select failed with error: %ld\n", WSAGetLastError());
			LeaveCriticalSection(&critical_section_accept_directly);
			return 1;

		}
		else {

			if (FD_ISSET(listenSocket, &readfds) && counter_accepted_clients < MAX_DIRECTLY_CONNECTIONS) {

				// Struct for information about connected client
				sockaddr_in clientAddr;
				int clientAddrSize = sizeof(struct sockaddr_in);

				FD_CLR(listenSocket, &readfds);

				// New connection request is received. Add new socket in array on first free position.
				acceptedSockets[counter_accepted_clients] = accept(listenSocket, (struct sockaddr *)&clientAddr, &clientAddrSize);
				if (acceptedSockets[counter_accepted_clients] == INVALID_SOCKET)
				{
					//printf("accept failed with error: %d\n", WSAGetLastError());
					closesocket(listenSocket);
					LeaveCriticalSection(&critical_section_accept_directly);
					return 1;

				}

				FD_CLR(listenSocket, &readfds);

				iResult = ioctlsocket(acceptedSockets[counter_accepted_clients], FIONBIO, &mode);
				if (iResult != NO_ERROR) {
					//printf("ioctlsocket failed with error: %ld\n", iResult);

					for (int j = 0; j < MAX_DIRECTLY_CONNECTIONS; j++)
					{
						EnterCriticalSection(&critical_section_hash_map);
						struct Element *tempClientElement = HashMap[j];
						LeaveCriticalSection(&critical_section_hash_map);
						while (tempClientElement)
						{
							sockaddr_in socketAddress;
							int socketAddress_len = sizeof(struct sockaddr_in);
							if (getpeername(acceptedSockets[counter_accepted_clients], (sockaddr *)&socketAddress, &socketAddress_len) == -1)
							{
								break;
							}
							char tempClientAddress[MAX_ADDRESS];
							inet_ntop(AF_INET, &socketAddress.sin_addr, tempClientAddress, INET_ADDRSTRLEN);

							if ((strcmp(tempClientAddress, (const char*)tempClientElement->clientData->address) == 0) && ((unsigned int)ntohs(socketAddress.sin_port) == tempClientElement->clientData->port))
							{
								EnterCriticalSection(&critical_section_hash_map);
								RemoveValueFromHashMap(HashMap, tempClientElement->clientData->name);
								//ShowHashMap(HashMap);
								LeaveCriticalSection(&critical_section_hash_map);
								break;
							}
							tempClientElement = tempClientElement->nextElement;
						}
					}

					closesocket(counter_accepted_clients);
					acceptedSockets[counter_accepted_clients] = INVALID_SOCKET;

				}
				else {

					counter_accepted_clients++;
				}

			}


			for (int i = 0; i < counter_accepted_clients; i++) {
				if ((FD_ISSET(acceptedSockets[i], &readfds)) == 1) {

					FD_CLR(acceptedSockets[i], &readfds);

					int iResult = recv(acceptedSockets[i], recvbuf, DEFAULT_BUFLEN, 0);
					if (iResult > 0)
					{
						directly_message = (Directly_Message*)recvbuf;
						
						if (strcmp((char*)directly_message->flag, "0\0") == 0) {

							struct sockaddr_in socketAddress;
							int socketAddress_len = sizeof(socketAddress);

							if (getpeername(acceptedSockets[i], (sockaddr *)&socketAddress, &socketAddress_len) == -1)
							{
								break;
							}

							char client_address[MAX_ADDRESS];
							inet_ntop(AF_INET, &socketAddress.sin_addr, client_address, INET_ADDRSTRLEN);

							ClientData *newClient = (ClientData*)malloc(sizeof(ClientData));
							unsigned char name[MAX_USERNAME];
							strcpy((char*)name, (char*)directly_message->message);
							strcpy((char*)newClient->name, (char*)name);
							strcpy((char*)newClient->address, (char*)client_address);
							newClient->port = (unsigned int)ntohs(socketAddress.sin_port);
							strcpy((char*)newClient->socket_type, "1\0");

							EnterCriticalSection(&critical_section_hash_map);
							AddValueToHashMap(HashMap, newClient);
							//ShowHashMap(HashMap);
							LeaveCriticalSection(&critical_section_hash_map);

						}
						else if(strcmp((char*)directly_message->flag, "1\0") == 0){

							EnterCriticalSection(&critical_section_std);
							printf("%s\n", directly_message->message);
							LeaveCriticalSection(&critical_section_std);

						} 
						else {

							continue;
						}
						

					}
					else if (iResult == 0)
					{
						for (int j = 0; j < MAX_DIRECTLY_CONNECTIONS; j++)
						{
							EnterCriticalSection(&critical_section_hash_map);
							struct Element *tempClientElement = HashMap[j];
							LeaveCriticalSection(&critical_section_hash_map);
							while (tempClientElement)
							{
								sockaddr_in socketAddress;
								int socketAddress_len = sizeof(struct sockaddr_in);
								if (getpeername(acceptedSockets[i], (sockaddr *)&socketAddress, &socketAddress_len) == -1)
								{
									break;
								}
								char tempClientAddress[MAX_ADDRESS];
								inet_ntop(AF_INET, &socketAddress.sin_addr, tempClientAddress, INET_ADDRSTRLEN);

								if ((strcmp(tempClientAddress, (const char*)tempClientElement->clientData->address) == 0) && ((unsigned int)ntohs(socketAddress.sin_port) == tempClientElement->clientData->port))
								{
									EnterCriticalSection(&critical_section_hash_map);
									RemoveValueFromHashMap(HashMap, tempClientElement->clientData->name);
									//ShowHashMap(HashMap);
									LeaveCriticalSection(&critical_section_hash_map);
									break;
								}
								tempClientElement = tempClientElement->nextElement;
							}
						}

						closesocket(acceptedSockets[i]);
						for (int j = i; j < counter_accepted_clients - 1; j++)
						{
							acceptedSockets[j] = acceptedSockets[j + 1];
						}
						acceptedSockets[counter_accepted_clients - 1] = INVALID_SOCKET;
						counter_accepted_clients--;
						i--;


					}
					else
					{
						if (WSAGetLastError() == WSAEWOULDBLOCK) {

							continue;
						}
						else {

							for (int j = 0; j < MAX_DIRECTLY_CONNECTIONS; j++)
							{
								EnterCriticalSection(&critical_section_hash_map);
								struct Element *tempClientElement = HashMap[j];
								LeaveCriticalSection(&critical_section_hash_map);
								while (tempClientElement)
								{
									sockaddr_in socketAddress;
									int socketAddress_len = sizeof(struct sockaddr_in);
									if (getpeername(acceptedSockets[i], (sockaddr *)&socketAddress, &socketAddress_len) == -1)
									{
										break;
									}
									char tempClientAddress[MAX_ADDRESS];
									inet_ntop(AF_INET, &socketAddress.sin_addr, tempClientAddress, INET_ADDRSTRLEN);

									if ((strcmp(tempClientAddress, (const char*)tempClientElement->clientData->address) == 0) && ((unsigned int)ntohs(socketAddress.sin_port) == tempClientElement->clientData->port))
									{
										EnterCriticalSection(&critical_section_hash_map);
										RemoveValueFromHashMap(HashMap, tempClientElement->clientData->name);
										//ShowHashMap(HashMap);
										LeaveCriticalSection(&critical_section_hash_map);
										break;
									}
									tempClientElement = tempClientElement->nextElement;
								}
							}

							closesocket(acceptedSockets[i]);
							for (int j = i; j < counter_accepted_clients - 1; j++)
							{
								acceptedSockets[j] = acceptedSockets[j + 1];
							}
							acceptedSockets[counter_accepted_clients - 1] = INVALID_SOCKET;
							counter_accepted_clients--;
							i--;


						}

					}

				}

			}

		}

		LeaveCriticalSection(&critical_section_accept_directly);

	}


	return 0;
}

DWORD WINAPI function_recv_connectSockets_directly(LPVOID parametri) {

	int iResult = 0;
	char recvbuf[DEFAULT_BUFLEN];

	fd_set readfds;
	FD_ZERO(&readfds);

	timeval timeVal;
	timeVal.tv_sec = 1;
	timeVal.tv_usec = 0;


	while (WaitForSingleObject(FinishSignal_Directly, 1) == WAIT_TIMEOUT) {

		EnterCriticalSection(&critical_section_connect_directly);
		if (count_connect_sockets == 0) {
			LeaveCriticalSection(&critical_section_connect_directly);
			continue;
		}

		for (int a = 0; a < count_connect_sockets; a++) {
			FD_SET(connectSockets_directly[a], &readfds);
		}

		iResult = select(0, &readfds, NULL, NULL, &timeVal);
		if (iResult == 0) {

			LeaveCriticalSection(&critical_section_connect_directly);
			continue;
		}
		else if (iResult == SOCKET_ERROR) {

			//printf("select failed with error: %ld\n", WSAGetLastError());
			LeaveCriticalSection(&critical_section_connect_directly);
			return 1;

		}
		else {

			for (int i = 0; i < count_connect_sockets; i++) {
				if ((FD_ISSET(connectSockets_directly[i], &readfds)) == 1) {

					FD_CLR(connectSockets_directly[i], &readfds);

					int iResult = recv(connectSockets_directly[i], recvbuf, DEFAULT_BUFLEN, 0);
					if (iResult > 0)
					{
						EnterCriticalSection(&critical_section_std);
						printf("%s\n", recvbuf);
						LeaveCriticalSection(&critical_section_std);

					}
					else if (iResult == 0)
					{
						for (int j = 0; j < MAX_DIRECTLY_CONNECTIONS; j++)
						{
							EnterCriticalSection(&critical_section_hash_map);
							struct Element *tempClientElement = HashMap[j];
							LeaveCriticalSection(&critical_section_hash_map);
							while (tempClientElement)
							{
								sockaddr_in socketAddress;
								int socketAddress_len = sizeof(struct sockaddr_in);
								if (getpeername(connectSockets_directly[i], (sockaddr *)&socketAddress, &socketAddress_len) == -1)
								{
									break;
								}
								char tempClientAddress[MAX_ADDRESS];
								inet_ntop(AF_INET, &socketAddress.sin_addr, tempClientAddress, INET_ADDRSTRLEN);

								if ((strcmp(tempClientAddress, (const char*)tempClientElement->clientData->address) == 0) && ((unsigned int)ntohs(socketAddress.sin_port) == tempClientElement->clientData->port))
								{
									EnterCriticalSection(&critical_section_hash_map);
									RemoveValueFromHashMap(HashMap, tempClientElement->clientData->name);
									//ShowHashMap(HashMap);
									LeaveCriticalSection(&critical_section_hash_map);
									break;
								}
								tempClientElement = tempClientElement->nextElement;
							}
						}

						closesocket(connectSockets_directly[i]);
						for (int j = i; j < count_connect_sockets - 1; j++)
						{
							connectSockets_directly[j] = connectSockets_directly[j + 1];
						}
						connectSockets_directly[count_connect_sockets - 1] = INVALID_SOCKET;
						count_connect_sockets--;
						i--;


					}
					else
					{
						if (WSAGetLastError() == WSAEWOULDBLOCK) {

							continue;
						}
						else {

							for (int j = 0; j < MAX_DIRECTLY_CONNECTIONS; j++)
							{
								EnterCriticalSection(&critical_section_hash_map);
								struct Element *tempClientElement = HashMap[j];
								LeaveCriticalSection(&critical_section_hash_map);
								while (tempClientElement)
								{
									sockaddr_in socketAddress;
									int socketAddress_len = sizeof(struct sockaddr_in);
									if (getpeername(connectSockets_directly[i], (sockaddr *)&socketAddress, &socketAddress_len) == -1)
									{
										break;
									}
									char tempClientAddress[MAX_ADDRESS];
									inet_ntop(AF_INET, &socketAddress.sin_addr, tempClientAddress, INET_ADDRSTRLEN);

									if ((strcmp(tempClientAddress, (const char*)tempClientElement->clientData->address) == 0) && ((unsigned int)ntohs(socketAddress.sin_port) == tempClientElement->clientData->port))
									{
										EnterCriticalSection(&critical_section_hash_map);
										RemoveValueFromHashMap(HashMap, tempClientElement->clientData->name);
										//ShowHashMap(HashMap);
										LeaveCriticalSection(&critical_section_hash_map);
										break;
									}
									tempClientElement = tempClientElement->nextElement;
								}
							}

							closesocket(connectSockets_directly[i]);
							for (int j = i; j < count_connect_sockets - 1; j++)
							{
								connectSockets_directly[j] = connectSockets_directly[j + 1];
							}
							connectSockets_directly[count_connect_sockets - 1] = INVALID_SOCKET;
							count_connect_sockets--;
							i--;


						}

					}

				}

			}

		}

		LeaveCriticalSection(&critical_section_connect_directly);

	}

	return 0;
}


int main()
{

	Client_Information_Directly *client_information_for_thread = (Client_Information_Directly*)malloc(sizeof(Client_Information_Directly));

	HANDLE thread;
	DWORD thread_id;

	HANDLE thread_directly_recv;
	DWORD thread_directly_recv_id;

	HANDLE thread_send_message_directly;
	DWORD thread_send_message_directly_id;

	HANDLE thread_for_accept_clients;
	DWORD thread_for_accept_clients_id;

	HANDLE thread_recv_on_connectSockets;
	DWORD thread_recv_on_connectSockets_id;

	FinishSignal = CreateSemaphore(NULL, 0, 2, NULL);
	FinishThreadSignal = CreateSemaphore(NULL, 0, 1, NULL);
	FinishSignal_Directly = CreateSemaphore(NULL, 0, 3, NULL);
	StartMainSignal = CreateSemaphore(NULL, 1, 1, NULL);
	StartSendMessageSignal = CreateSemaphore(NULL, 0, 1, NULL);
	

	if (FinishThreadSignal && FinishSignal && StartSendMessageSignal && FinishSignal_Directly && StartMainSignal) {

		InitializeCriticalSection(&critical_section_hash_map);
		InitializeCriticalSection(&critical_section_std);
		InitializeCriticalSection(&critical_section_server);
		InitializeCriticalSection(&critical_section_connect_directly);
		InitializeCriticalSection(&critical_section_accept_directly);
		thread = CreateThread(NULL, 0, &thread_function, NULL, CREATE_SUSPENDED, &thread_id);
		thread_directly_recv = CreateThread(NULL, 0, &function_recv_directly, client_information_for_thread, CREATE_SUSPENDED, &thread_directly_recv_id);
		thread_send_message_directly = CreateThread(NULL, 0, &function_send_message_directly, client_information_for_thread, CREATE_SUSPENDED, &thread_send_message_directly_id);
		thread_for_accept_clients = CreateThread(NULL, 0, &function_accept_clients, NULL, CREATE_SUSPENDED, &thread_for_accept_clients_id);
		thread_recv_on_connectSockets = CreateThread(NULL, 0, &function_recv_connectSockets_directly, NULL, CREATE_SUSPENDED, &thread_recv_on_connectSockets_id);

	}
	else {

		SAFE_DELETE_HANDLE(FinishSignal);
		SAFE_DELETE_HANDLE(StartSendMessageSignal);
		SAFE_DELETE_HANDLE(FinishThreadSignal);
		SAFE_DELETE_HANDLE(FinishSignal_Directly);
		SAFE_DELETE_HANDLE(StartMainSignal);

		SAFE_DELETE_HANDLE(thread);
		SAFE_DELETE_HANDLE(thread_directly_recv);
		SAFE_DELETE_HANDLE(thread_send_message_directly);
		SAFE_DELETE_HANDLE(thread_for_accept_clients);
		SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);

		DeleteCriticalSection(&critical_section_hash_map);
		DeleteCriticalSection(&critical_section_std);
		DeleteCriticalSection(&critical_section_server);
		DeleteCriticalSection(&critical_section_connect_directly);
		DeleteCriticalSection(&critical_section_accept_directly);

		return 1;

	}

	struct Message_For_Client
	{
		unsigned char sender[MAX_USERNAME];
		unsigned char receiver[MAX_USERNAME];
		unsigned char message[MAX_MESSAGE];
		unsigned char listen_address[MAX_ADDRESS];
		unsigned int listen_port;
		unsigned char flag[2];
	};

	connectSocket = INVALID_SOCKET;

	for (int i = 0; i < MAX_DIRECTLY_CONNECTIONS; i++) {
		connectSockets_directly[i] = INVALID_SOCKET;
	}
	count_connect_sockets = 0;

	listenSocket = INVALID_SOCKET;
	for (int i = 0; i < MAX_DIRECTLY_CONNECTIONS; i++) {
		acceptedSockets[i] = INVALID_SOCKET;
	}
	counter_accepted_clients = 0;

	// variable used to store function return value
	int iResult;

	if (InitializeWindowsSockets() == false)
	{
		// we won't log anything since it will be logged
		// by InitializeWindowsSockets() function
		return 1;
	}

	// Prepare address information structures
	addrinfo *resultingAddress = NULL;
	addrinfo hints;

	memset(&hints, 0, sizeof(hints));  
	hints.ai_family = AF_INET;       // IPv4 address
	hints.ai_socktype = SOCK_STREAM; // Provide reliable data streaming
	hints.ai_protocol = IPPROTO_TCP; // Use TCP protocol
	hints.ai_flags = AI_PASSIVE;     

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, "0", &hints, &resultingAddress);
	if (iResult != 0)
	{
		//printf("getaddrinfo failed with error: %d\n", iResult);
		SAFE_DELETE_HANDLE(thread);
		SAFE_DELETE_HANDLE(thread_directly_recv);
		SAFE_DELETE_HANDLE(thread_send_message_directly);
		SAFE_DELETE_HANDLE(thread_for_accept_clients);
		SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
		EndOfProgram();
		free(client_information_for_thread);
		return 1;
	}

	listenSocket = socket(AF_INET,      // IPv4 address famly
		SOCK_STREAM,  // stream socket (TCP)
		IPPROTO_TCP); // TCP

	if (listenSocket == INVALID_SOCKET)
	{
		//printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		SAFE_DELETE_HANDLE(thread);
		SAFE_DELETE_HANDLE(thread_directly_recv);
		SAFE_DELETE_HANDLE(thread_send_message_directly);
		SAFE_DELETE_HANDLE(thread_for_accept_clients);
		SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
		EndOfProgram();
		free(client_information_for_thread);
		return 1;
	}

	// Setup the TCP listening socket - bind port number and local address to socket
	iResult = bind(listenSocket, resultingAddress->ai_addr, (int)resultingAddress->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		//printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		SAFE_DELETE_HANDLE(thread);
		SAFE_DELETE_HANDLE(thread_directly_recv);
		SAFE_DELETE_HANDLE(thread_send_message_directly);
		SAFE_DELETE_HANDLE(thread_for_accept_clients);
		SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
		EndOfProgram();
		free(client_information_for_thread);
		return 1;
	}

	// Since we don't need resultingAddress any more, free it
	freeaddrinfo(resultingAddress);

	struct sockaddr_in socketAddress;
	int socketAddress_len = sizeof(socketAddress);
	// Ask getsockname to fill in this socket's local adress
	if (getsockname(listenSocket, (sockaddr *)&socketAddress, &socketAddress_len) == -1)
	{
		//printf("getsockname() failed.\n");
		SAFE_DELETE_HANDLE(thread);
		SAFE_DELETE_HANDLE(thread_directly_recv);
		SAFE_DELETE_HANDLE(thread_send_message_directly);
		SAFE_DELETE_HANDLE(thread_for_accept_clients);
		SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
		EndOfProgram();
		free(client_information_for_thread);
		return -1;
	}
	InetPton(AF_INET, TEXT(SERVER_IP_ADDRESS), &socketAddress.sin_addr.s_addr);

	// Print the IP address and local port
	//printf("Local(listen) IP address is: %s\n", inet_ntoa(socketAddress.sin_addr));
	//printf("Local(listen) port is: %d\n", (int)ntohs(socketAddress.sin_port));

	Message_For_Client packet;
	strcpy((char*)packet.listen_address, inet_ntoa(socketAddress.sin_addr));
	packet.listen_port = (int)ntohs(socketAddress.sin_port);

	// create a socket
	connectSocket = socket(AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP);

	if (connectSocket == INVALID_SOCKET)
	{
		//printf("socket failed with error: %ld\n", WSAGetLastError());
		SAFE_DELETE_HANDLE(thread);
		SAFE_DELETE_HANDLE(thread_directly_recv);
		SAFE_DELETE_HANDLE(thread_send_message_directly);
		SAFE_DELETE_HANDLE(thread_for_accept_clients);
		SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
		EndOfProgram();
		free(client_information_for_thread);
		return 1;
	}

	// create and initialize address structure
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;								// IPv4
	serverAddress.sin_addr.s_addr = inet_addr(SERVER_IP_ADDRESS);
	serverAddress.sin_port = htons(DEFAULT_PORT);

	// connect to server specified in serverAddress and socket connectSocket
	if (connect(connectSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
	{
		//printf("Unable to connect to server.\n");
		SAFE_DELETE_HANDLE(thread);
		SAFE_DELETE_HANDLE(thread_directly_recv);
		SAFE_DELETE_HANDLE(thread_send_message_directly);
		SAFE_DELETE_HANDLE(thread_for_accept_clients);
		SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
		EndOfProgram();
		free(client_information_for_thread);
		return 1;
	}

	unsigned char sender[MAX_USERNAME];
	unsigned char receiver[MAX_USERNAME];
	char *message = (char*)malloc(MAX_MESSAGE);
	bool directly_communication = false;
	strcpy((char*)packet.flag, "1\0");
	unsigned char communication_type[MAX_USERNAME];
	char recvbuf[DEFAULT_BUFLEN];

	printf("Zdravo! Da biste ostvarili komunikaciju sa ostalim klijntima morate da se registrujete.\n");
	while (true) {

		printf("Unesite Vase korisnicko ime:\n");
		scanf("%s", &sender);
		strcpy((char*)packet.sender, (char*)sender);
		strcpy((char*)packet.receiver, "*\0");
		strcpy((char*)packet.message, "*\0");
		iResult = send(connectSocket, (char*)&packet, sizeof(packet), 0);
		if (iResult == SOCKET_ERROR)
		{
			//printf("send failed with error: %d\n", WSAGetLastError());
			printf("Server vise nije dostupan!");
			
			SAFE_DELETE_HANDLE(thread);
			SAFE_DELETE_HANDLE(thread_directly_recv);
			SAFE_DELETE_HANDLE(thread_send_message_directly);
			SAFE_DELETE_HANDLE(thread_for_accept_clients);
			SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
			SAFE_DELETE_HANDLE(FinishThreadSignal);
			EndOfProgram();
			free(client_information_for_thread);
			free(message);
			return 0;

		}

		// Receive data until the client shuts down the connection
		iResult = recv(connectSocket, recvbuf, DEFAULT_BUFLEN, 0);
		if (iResult > 0)
		{
			recvbuf[iResult] = '\0';
			if (strcmp(&recvbuf[0], "1") == 0) {
				printf("\nUspesno ste se registrovali!\n");
				break;
			}
			else if (strcmp(&recvbuf[0], "0") == 0) {
				printf("\nVec postoji klijent sa username-om koji ste uneli. Pokusajte opet:\n");
			}
			else {
				printf("\nPokusajte ponovo:\n");
			}

		}
		else if (iResult == 0)  //connection was closed gracefully
		{
			//printf("Connection with server closed.\n");
			printf("Server vise nije dostupan!\n");
			SAFE_DELETE_HANDLE(thread);
			SAFE_DELETE_HANDLE(thread_directly_recv);
			SAFE_DELETE_HANDLE(thread_send_message_directly);
			SAFE_DELETE_HANDLE(thread_for_accept_clients);
			SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
			SAFE_DELETE_HANDLE(FinishThreadSignal);  // ok
			EndOfProgram();
			free(client_information_for_thread);
			free(message);
			return 0;

		}
		else  // there was an error during recv
		{
			//printf("recv failed with error: %d\n", WSAGetLastError());
			printf("Server vise nije dostupan!\n");
			SAFE_DELETE_HANDLE(thread);
			SAFE_DELETE_HANDLE(thread_directly_recv);
			SAFE_DELETE_HANDLE(thread_send_message_directly);
			SAFE_DELETE_HANDLE(thread_for_accept_clients);
			SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
			SAFE_DELETE_HANDLE(FinishThreadSignal);
			EndOfProgram();
			free(client_information_for_thread);
			free(message);
			return 0;

		}

	}

	strcpy((char*)packet.flag, "2\0");

	unsigned long mode = 1;
	iResult = ioctlsocket(connectSocket, FIONBIO, &mode);
	if (iResult != NO_ERROR) {

		printf("ioctlsocket failed with error: %ld\n", iResult);
		
		SAFE_DELETE_HANDLE(thread);
		SAFE_DELETE_HANDLE(thread_directly_recv);
		SAFE_DELETE_HANDLE(thread_send_message_directly);
		SAFE_DELETE_HANDLE(thread_for_accept_clients);
		SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
		SAFE_DELETE_HANDLE(FinishThreadSignal);
		EndOfProgram();
		free(client_information_for_thread);
		free(message);
		return 0;
	}

	EnterCriticalSection(&critical_section_hash_map);
	InitializeHashMap(HashMap);
	LeaveCriticalSection(&critical_section_hash_map);

	ResumeThread(thread);
	ResumeThread(thread_for_accept_clients);

	EnterCriticalSection(&critical_section_std);
	printf("Trenutan nacin komunikacije sa klijentima je preko servera.\n");
	LeaveCriticalSection(&critical_section_std);
	while (WaitForSingleObject(FinishSignal, 110) == WAIT_TIMEOUT) {

		do {

			if (WaitForSingleObject(FinishSignal, 1) != WAIT_TIMEOUT) {
				break;
			}

			EnterCriticalSection(&critical_section_std);
			printf("Da li zelite direktno da komunicirate sa klijentima? (da/ne)\n");
			scanf("%s", &communication_type);
			LeaveCriticalSection(&critical_section_std);

			for (int i = 0; communication_type[i]; i++) {
				communication_type[i] = tolower(communication_type[i]);
			}

			if (strcmp((const char*)communication_type, "da") == 0) {
				directly_communication = true;
				strcpy((char*)packet.flag, "3\0");
			}

		} while (strcmp((const char*)communication_type, "da") != 0 && strcmp((const char*)communication_type, "ne") != 0);

		if (WaitForSingleObject(FinishSignal, 60) != WAIT_TIMEOUT || (strcmp((const char*)communication_type, "da") != 0 && strcmp((const char*)communication_type, "ne") != 0)) {
			break;
		}

		if (directly_communication == false) {

			EnterCriticalSection(&critical_section_std);
			printf("Unesite naziv klijenta kome zelite da posaljete poruku:\n");
			scanf("%s", &receiver);
			getchar();
			LeaveCriticalSection(&critical_section_std);
			
			strcpy((char*)packet.receiver, (char*)receiver);

			if (WaitForSingleObject(FinishSignal, 60) != WAIT_TIMEOUT) {
				break;
			}

			EnterCriticalSection(&critical_section_std);
			printf("Unesite poruku:\n");
			fgets(message, MAX_MESSAGE, stdin);
			LeaveCriticalSection(&critical_section_std);
			message[strlen(message) - 1] = message[strlen(message)];
			strcpy((char*)packet.message, (char*)message);
			EnterCriticalSection(&critical_section_server);
			iResult = send(connectSocket, (char*)&packet, sizeof(packet), 0);
			LeaveCriticalSection(&critical_section_server);
			if (iResult == SOCKET_ERROR)
			{
				//printf("send failed with error: %d\n", WSAGetLastError());
				break;
			}

		}
		else {  // directly communication

			break;
		}

	}

	if (directly_communication != true) {
		
		EnterCriticalSection(&critical_section_std);
		printf("Server vise nije dostupan!");
		LeaveCriticalSection(&critical_section_std);

		ReleaseSemaphore(FinishThreadSignal, 1, NULL);
		ReleaseSemaphore(FinishSignal, 1, NULL);
		if (thread != NULL) {
			WaitForSingleObject(thread, INFINITE);
		}
		if (thread_for_accept_clients != NULL) {
			WaitForSingleObject(thread_for_accept_clients, INFINITE);
		}
		
		SAFE_DELETE_HANDLE(thread);
		SAFE_DELETE_HANDLE(thread_directly_recv);
		SAFE_DELETE_HANDLE(thread_send_message_directly);
		SAFE_DELETE_HANDLE(thread_for_accept_clients);
		SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
		SAFE_DELETE_HANDLE(FinishThreadSignal);
		EndOfProgram();
		free(message);
		free(client_information_for_thread);

		return 0;
		
	}

	/*-----------directly communication------------*/
	free(message);

	strcpy((char*)packet.flag, "4\0");
	EnterCriticalSection(&critical_section_server);
	iResult = send(connectSocket, (char*)&packet, sizeof(packet), 0);
	LeaveCriticalSection(&critical_section_server);
	if (iResult == SOCKET_ERROR)
	{
		//printf("send failed with error: %d\n", WSAGetLastError());
		EnterCriticalSection(&critical_section_std);
		printf("Server vise nije dostupan!");
		LeaveCriticalSection(&critical_section_std);

		ReleaseSemaphore(FinishThreadSignal, 1, NULL);
		ReleaseSemaphore(FinishSignal, 1, NULL);
		if (thread != NULL) {
			WaitForSingleObject(thread, INFINITE);
		}
		if (thread_for_accept_clients != NULL) {
			WaitForSingleObject(thread_for_accept_clients, INFINITE);
		}
		
		SAFE_DELETE_HANDLE(thread);
		SAFE_DELETE_HANDLE(thread_directly_recv);
		SAFE_DELETE_HANDLE(thread_send_message_directly);
		SAFE_DELETE_HANDLE(thread_for_accept_clients);
		SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);
		SAFE_DELETE_HANDLE(FinishThreadSignal);
		EndOfProgram();
		free(message);
		free(client_information_for_thread);

		return 0;

	}

	ReleaseSemaphore(FinishThreadSignal, 1, NULL);
	if (thread != NULL) {

		WaitForSingleObject(thread, INFINITE);
	}
	SAFE_DELETE_HANDLE(FinishThreadSignal);
	SAFE_DELETE_HANDLE(thread);
	/*------------------------------------------*/
	ResumeThread(thread_directly_recv);
	ResumeThread(thread_send_message_directly);
	ResumeThread(thread_recv_on_connectSockets);

	strcpy((char*)packet.flag, "3\0");
	EnterCriticalSection(&critical_section_std);
	printf("Presli ste na direktan nacin komunikacije sa klijentima!\n");
	LeaveCriticalSection(&critical_section_std);

	char *directly_message = (char*)malloc(MAX_MESSAGE_DIRECTLY);
	Directly_Message directly_message_packet;

	HANDLE semaphores[2] = { FinishSignal_Directly, StartMainSignal };
	while (WaitForMultipleObjects((DWORD)2, semaphores, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {

		Sleep(110);
		bool break_all = false;

		EnterCriticalSection(&critical_section_std);
		printf("Unesite naziv klijenta sa kojim zelite da komunicirate:\n");
		scanf("%s", &receiver);
		LeaveCriticalSection(&critical_section_std);

		if (WaitForSingleObject(FinishSignal_Directly, 1) != WAIT_TIMEOUT) {
			break;
		}
		 
		bool postoji = false;

		EnterCriticalSection(&critical_section_hash_map);
		postoji = ClientExistsInHashMap(HashMap, receiver);
		LeaveCriticalSection(&critical_section_hash_map);

		if (!postoji) {

			strcpy((char*)packet.receiver, (char*)receiver);
			EnterCriticalSection(&critical_section_server);
			iResult = send(connectSocket, (char*)&packet, sizeof(packet), 0);
			LeaveCriticalSection(&critical_section_server);
			if (iResult == SOCKET_ERROR)
			{
				EnterCriticalSection(&critical_section_std);
				printf("Server vise nije dostupan!\n");
				LeaveCriticalSection(&critical_section_std);
				break;
			}

		}
		else {

			EnterCriticalSection(&critical_section_std);
			printf("Vec ste povezani sa zeljenim klijentom.\n");
			LeaveCriticalSection(&critical_section_std);

			EnterCriticalSection(&critical_section_hash_map);
			ClientData *client_from_HashMap = FindValueInHashMap(HashMap, receiver);
			LeaveCriticalSection(&critical_section_hash_map);

			if (strcmp((char*)client_from_HashMap->socket_type, "1\0") == 0) {  // search acceptedSockets

				bool nasao_a = false;

				struct sockaddr_in socketAddress;  // struktura za smestanje adrese i porta iz socket-a iz connectSocket_directly niza
				int socketAddress_len = sizeof(socketAddress);

				for (int k = 0; k < counter_accepted_clients; k++)
				{
					// Ask getsockname to fill in this socket's local adress
					EnterCriticalSection(&critical_section_accept_directly);
					if (getpeername(acceptedSockets[k], (sockaddr *)&socketAddress, &socketAddress_len) == -1)
					{
						LeaveCriticalSection(&critical_section_accept_directly);
						break;
					}
					LeaveCriticalSection(&critical_section_accept_directly);

					char client_address[MAX_ADDRESS];
					inet_ntop(AF_INET, &socketAddress.sin_addr, client_address, INET_ADDRSTRLEN);

					if ((strcmp(client_address, (char*)client_from_HashMap->address) == 0) && ((unsigned int)ntohs(socketAddress.sin_port) == client_from_HashMap->port))
					{
						nasao_a = true;

						
						EnterCriticalSection(&critical_section_std);
						getchar();
						printf("Unesite poruku:\n");
						fgets(directly_message, MAX_MESSAGE_DIRECTLY, stdin);
						LeaveCriticalSection(&critical_section_std);

						if (WaitForSingleObject(FinishSignal_Directly, 1) != WAIT_TIMEOUT) {
							break_all = true;
							break;
						}

						directly_message[strlen(directly_message) - 1] = directly_message[strlen(directly_message)];
						strcpy((char*)directly_message_packet.flag, "1\0");
						sprintf((char*)directly_message_packet.message, "[%s]:%s", (char*)sender, directly_message);
						EnterCriticalSection(&critical_section_accept_directly);
						iResult = send(acceptedSockets[k], (char*)&directly_message_packet, sizeof(directly_message_packet), 0);
						LeaveCriticalSection(&critical_section_accept_directly);
						if (iResult == SOCKET_ERROR)
						{
							//printf("send failed with error: %d\n", WSAGetLastError());
							EnterCriticalSection(&critical_section_std);
							printf("Poruka nije poslata, jer klijent vise nije dostupan!\n");
							LeaveCriticalSection(&critical_section_std);
							EnterCriticalSection(&critical_section_accept_directly);
							closesocket(acceptedSockets[k]);
							for (int j = k; j < counter_accepted_clients - 1; j++)
							{
								acceptedSockets[j] = acceptedSockets[j + 1];
							}
							acceptedSockets[counter_accepted_clients - 1] = INVALID_SOCKET;
							counter_accepted_clients--;
							LeaveCriticalSection(&critical_section_accept_directly);

							EnterCriticalSection(&critical_section_hash_map);
							RemoveValueFromHashMap(HashMap, client_from_HashMap->name);
							//ShowHashMap(HashMap);
							LeaveCriticalSection(&critical_section_hash_map);
						}
						else {

							EnterCriticalSection(&critical_section_std);
							printf("Poruka je uspesno poslata zeljenom klijentu!\n");
							LeaveCriticalSection(&critical_section_std);
						}

						break;
					}

				}

				if (break_all == true) {
					break;
				}

				if (nasao_a == false) {  // error in getsockname()
					
					EnterCriticalSection(&critical_section_std);
					printf("Pokusajte ponovo!\n");
					LeaveCriticalSection(&critical_section_std);
					
				}

			}
			else {  // search connectSockets_directly

				bool nasao = false;

				struct sockaddr_in socketAddress;
				int socketAddress_len = sizeof(socketAddress);

				for (int k = 0; k < count_connect_sockets; k++)
				{
					// Ask getsockname to fill in this socket's local adress
					EnterCriticalSection(&critical_section_connect_directly);
					if (getpeername(connectSockets_directly[k], (sockaddr *)&socketAddress, &socketAddress_len) == -1)
					{
						LeaveCriticalSection(&critical_section_connect_directly);
						break;
					}
					LeaveCriticalSection(&critical_section_connect_directly);

					char client_address[MAX_ADDRESS];
					inet_ntop(AF_INET, &socketAddress.sin_addr, client_address, INET_ADDRSTRLEN);

					if ((strcmp(client_address, (char*)client_from_HashMap->address) == 0) && ((unsigned int)ntohs(socketAddress.sin_port) == client_from_HashMap->port))
					{
						nasao = true;

						
						EnterCriticalSection(&critical_section_std);
						getchar();
						printf("Unesite poruku:\n");
						fgets(directly_message, MAX_MESSAGE_DIRECTLY, stdin);
						LeaveCriticalSection(&critical_section_std);

						if (WaitForSingleObject(FinishSignal_Directly, 1) != WAIT_TIMEOUT) {
							break_all = true;
							break;
						}

						directly_message[strlen(directly_message) - 1] = directly_message[strlen(directly_message)];
						strcpy((char*)directly_message_packet.flag, "1\0");
						sprintf((char*)directly_message_packet.message, "[%s]:%s", (char*)sender, directly_message);
						EnterCriticalSection(&critical_section_connect_directly);
						iResult = send(connectSockets_directly[k], (char*)&directly_message_packet, sizeof(directly_message_packet), 0);
						LeaveCriticalSection(&critical_section_connect_directly);
						if (iResult == SOCKET_ERROR)
						{
							//printf("send failed with error: %d\n", WSAGetLastError());
							EnterCriticalSection(&critical_section_std);
							printf("Poruka nije poslata, jer klijent vise nije dostupan!\n");
							LeaveCriticalSection(&critical_section_std);
							EnterCriticalSection(&critical_section_connect_directly);
							closesocket(connectSockets_directly[k]);
							for (int j = k; j < count_connect_sockets - 1; j++)
							{
								connectSockets_directly[j] = connectSockets_directly[j + 1];
							}
							connectSockets_directly[count_connect_sockets - 1] = INVALID_SOCKET;
							count_connect_sockets--;
							LeaveCriticalSection(&critical_section_connect_directly);

							EnterCriticalSection(&critical_section_hash_map);
							RemoveValueFromHashMap(HashMap, client_from_HashMap->name);
							//ShowHashMap(HashMap);
							LeaveCriticalSection(&critical_section_hash_map);
						}
						else {
							
							EnterCriticalSection(&critical_section_std);
							printf("Poruka je uspesno poslata zeljenom klijentu!\n");
							LeaveCriticalSection(&critical_section_std);
						}
						
						break;
					}

				}

				if (break_all == true) {
					break;
				}

				if (nasao == false) {  // error in getsockname()

					EnterCriticalSection(&critical_section_std);
					printf("Pokusajte ponovo!\n");
					LeaveCriticalSection(&critical_section_std);
				}

			}

			ReleaseSemaphore(StartMainSignal, 1, NULL);

		}



	}

	if (thread_directly_recv != NULL) {

		WaitForSingleObject(thread_directly_recv, INFINITE);
	}
	if (thread_send_message_directly != NULL) {

		WaitForSingleObject(thread_send_message_directly, INFINITE);
	}
	if (thread_recv_on_connectSockets != NULL) {

		WaitForSingleObject(thread_recv_on_connectSockets, INFINITE);
	}
	if (thread_for_accept_clients != NULL) {

		WaitForSingleObject(thread_for_accept_clients, INFINITE);
	}

	SAFE_DELETE_HANDLE(thread_directly_recv);
	SAFE_DELETE_HANDLE(thread_send_message_directly);
	SAFE_DELETE_HANDLE(thread_for_accept_clients);
	SAFE_DELETE_HANDLE(thread_recv_on_connectSockets);

	EndOfProgram();

	free(directly_message);
	free(client_information_for_thread);

	return 0;

}

bool InitializeWindowsSockets()
{
	WSADATA wsaData;
	// Initialize windows sockets library for this process
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("WSAStartup failed with error: %d\n", WSAGetLastError());
		return false;
	}
	return true;
}

void EndOfProgram() {

	int iResult = 0;

	SAFE_DELETE_HANDLE(FinishSignal_Directly);
	SAFE_DELETE_HANDLE(StartMainSignal);
	SAFE_DELETE_HANDLE(StartSendMessageSignal);
	SAFE_DELETE_HANDLE(FinishSignal);
	
	DeleteCriticalSection(&critical_section_hash_map);
	DeleteCriticalSection(&critical_section_std);
	DeleteCriticalSection(&critical_section_server);
	DeleteCriticalSection(&critical_section_connect_directly);
	DeleteCriticalSection(&critical_section_accept_directly);

	printf("\nPress any key to exit: ");
	_getch();

	iResult = shutdown(connectSocket, SD_BOTH);
	if (iResult == SOCKET_ERROR)
	{
		//printf("Shutdown failed with error: %d\n", WSAGetLastError());
	}
	closesocket(connectSocket);

	for (int i = 0; i < counter_accepted_clients; i++) {
		iResult = shutdown(acceptedSockets[i], SD_BOTH);
		if (iResult == SOCKET_ERROR)
		{
			//printf("Shutdown failed with error: %d\n", WSAGetLastError());
		}
		closesocket(acceptedSockets[i]);
	}

	for (int i = 0; i < count_connect_sockets; i++) {
		iResult = shutdown(connectSockets_directly[i], SD_BOTH);
		if (iResult == SOCKET_ERROR)
		{
			//printf("Shutdown failed with error: %d\n", WSAGetLastError());
		}
		closesocket(connectSockets_directly[i]);
	}

	closesocket(listenSocket);

	WSACleanup();

	return;
}
