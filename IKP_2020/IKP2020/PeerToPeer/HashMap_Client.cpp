#include "pch.h"
#include "HashMap_Client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long GenerateHashMapKey(unsigned char *str)
{
	unsigned long hash = 5381;
	int c;

	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	return hash;
}

void InitializeHashMap(Element** HashMap)
{
	for (int i = 0; i < MAX_DIRECTLY_CONNECTIONS; i++)
	{
		HashMap[i] = NULL;
	}
}

bool AddValueToHashMap(Element** HashMap, ClientData* clientData)
{
	struct Element *newElement = (struct Element*)malloc(sizeof(struct Element));
	newElement->clientData = clientData;
	newElement->nextElement = NULL;

	unsigned int key = GenerateHashMapKey(clientData->name) % MAX_DIRECTLY_CONNECTIONS;

	if (HashMap[key] == NULL)
	{
		HashMap[key] = newElement;
		return true;
	}
	else
	{
		struct Element *tempElement = HashMap[key];
		while (tempElement->nextElement)
		{
			tempElement = tempElement->nextElement;
		}
		tempElement->nextElement = newElement;
		return true;
	}
	return false;
}

void ShowHashMap(Element** HashMap)
{
	printf("\n---- START ----\n");
	for (int i = 0; i < MAX_DIRECTLY_CONNECTIONS; i++)
	{
		struct Element *tempElement = HashMap[i];
		printf("[%d] --->", i);
		while (tempElement)
		{
			printf(" %s, %s, %d %s|", tempElement->clientData->name, tempElement->clientData->address, tempElement->clientData->port, tempElement->clientData->socket_type);
			tempElement = tempElement->nextElement;
		}
		printf(" NULL\n");
	}
	printf("---- END ----\n");
}

ClientData* FindValueInHashMap(Element** HashMap, unsigned char *clientName)
{
	unsigned int key = GenerateHashMapKey(clientName) % MAX_DIRECTLY_CONNECTIONS;
	if (HashMap[key] != NULL)
	{
		struct Element *tempElement = HashMap[key];
		while (tempElement)
		{
			if (strcmp((const char*)clientName, (const char*)tempElement->clientData->name) == 0)
			{
				return tempElement->clientData;
			}
			tempElement = tempElement->nextElement;
		}
	}

	/*
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		struct Element *tempElement = HashMap[i];
		while (tempElement)
		{
			if (strcmp((const char*)clientName, (const char*)tempElement->clientData->name) == 0)
			{
				return tempElement->clientData;
			}
			tempElement = tempElement->nextElement;
		}
	}
	*/
}

bool RemoveValueFromHashMap(Element** HashMap, unsigned char *clientName)
{
	unsigned int key = GenerateHashMapKey(clientName) % MAX_DIRECTLY_CONNECTIONS;
	struct Element *tempElement = HashMap[key];
	if (tempElement != NULL)
	{
		if (strcmp((const char*)clientName, (const char*)tempElement->clientData->name) == 0)
		{
			HashMap[key] = NULL;
			return true;
		}
		else
		{
			while (tempElement->nextElement)
			{
				if (strcmp((const char*)clientName, (const char*)tempElement->nextElement->clientData->name) == 0)
				{
					HashMap[key]->nextElement = tempElement->nextElement->nextElement;
					return true;
				}
				tempElement = tempElement->nextElement;
			}
		}
	}
	return false;
}

bool ClientExistsInHashMap(Element** HashMap, unsigned char *clientName)
{
	unsigned int key = GenerateHashMapKey(clientName) % MAX_DIRECTLY_CONNECTIONS;
	if (HashMap[key] != NULL)
	{
		struct Element *tempElement = HashMap[key];
		while (tempElement)
		{
			if (strcmp((const char*)clientName, (const char*)tempElement->clientData->name) == 0)
			{
				return true;
			}
			tempElement = tempElement->nextElement;
		}
	}
	return false;
	
	/*
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		struct Element *tempElement = HashMap[i];
		while (tempElement)
		{
			if (strcmp((const char*)clientName, (const char*)tempElement->clientData->name) == 0)
			{
				return true;
			}
			tempElement = tempElement->nextElement;
		}
	}
	return false;
	*/
}
