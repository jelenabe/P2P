#ifndef HASHMAP_H
#define HASHMAP_H

#define MAX_CLIENTS 20
#define MAX_USERNAME 25
#define MAX_ADDRESS 50
#define MAXLEN 256

typedef struct ClientData {
	unsigned char name[MAX_USERNAME];
	unsigned char address[MAX_ADDRESS];
	unsigned int port;
	unsigned char listen_address[MAX_ADDRESS];
	unsigned int listen_port;
	unsigned char directly[2];
} ClientData;

struct Element
{
	ClientData *clientData;
	struct Element *nextElement;
};

static Element *HashMap[MAX_CLIENTS];

unsigned long GenerateHashValue(unsigned char *str);
void InitializeHashMap();
void ShowHashMap();
bool AddValueToHashMap(ClientData *clientData);
ClientData* FindValueInHashMap(unsigned char *clientName);
bool RemoveValueFromHashMap(unsigned char *clientName);
bool ClientExistsInHashMap(unsigned char *name);
bool ChangeClientsDirectlyValue(unsigned char *name, char newValue[2]);

#endif