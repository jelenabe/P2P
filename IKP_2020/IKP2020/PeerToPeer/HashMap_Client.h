#pragma once
#ifndef HASHMAP_CLIENT_H
#define HASHMAP_CLIENT_H

#define MAX_DIRECTLY_CONNECTIONS 10
#define MAX_USERNAME 25
#define MAX_ADDRESS 50

typedef struct ClientData {
	unsigned char name[MAX_USERNAME];
	unsigned char address[MAX_ADDRESS];
	unsigned int port;
	unsigned char socket_type[2];
} ClientData;

struct Element
{
	ClientData *clientData;
	struct Element *nextElement;
};

unsigned long GenerateHashMapKey(unsigned char *str);
void InitializeHashMap(Element** HashMap);
void ShowHashMap(Element** HashMap);
bool AddValueToHashMap(Element** HashMap, ClientData *clientData);
ClientData* FindValueInHashMap(Element** HashMap, unsigned char *clientName);
bool RemoveValueFromHashMap(Element** HashMap, unsigned char *clientName);
bool ClientExistsInHashMap(Element** HashMap, unsigned char *name);

#endif