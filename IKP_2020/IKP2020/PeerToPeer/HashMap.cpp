#include "pch.h"
#include "HashMap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long GenerateHashValue(unsigned char *str)
{
	unsigned long hash = 5381;
	int c;

	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	return hash;
}

void InitializeHashMap()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		HashMap[i] = NULL;
	}
}

bool AddValueToHashMap(ClientData* clientData)
{
	struct Element *newElement = (struct Element*)malloc(sizeof(struct Element));
	strcpy_s((char*)clientData->directly, sizeof(clientData->directly), "0\0");
	newElement->clientData = clientData;
	newElement->nextElement = NULL;

	unsigned int key = GenerateHashValue(clientData->name) % MAX_CLIENTS;

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

void ShowHashMap()
{
	printf("\n---- START ----\n");
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		struct Element *tempElement = HashMap[i];
		printf("[%d] --->", i);
		while (tempElement)
		{
			printf(" %s, %s, %d, %s, %d, %s |", tempElement->clientData->name, tempElement->clientData->address, tempElement->clientData->port, tempElement->clientData->listen_address, tempElement->clientData->listen_port, tempElement->clientData->directly);
			tempElement = tempElement->nextElement;
		}
		printf(" NULL\n");
	}
	printf("---- END ----\n");
}

ClientData* FindValueInHashMap(unsigned char *clientName)
{
	unsigned int key = GenerateHashValue(clientName) % MAX_CLIENTS;
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

bool RemoveValueFromHashMap(unsigned char *clientName)
{
	unsigned int key = GenerateHashValue(clientName) % MAX_CLIENTS;
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

bool ClientExistsInHashMap(unsigned char *clientName)
{
	unsigned int key = GenerateHashValue(clientName) % MAX_CLIENTS;
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

bool ChangeClientsDirectlyValue(unsigned char* clientName, char newValue[2])
{
	unsigned int key = GenerateHashValue(clientName) % MAX_CLIENTS;
	if (HashMap[key] != NULL)
	{
		struct Element *tempElement = HashMap[key];
		while (tempElement)
		{
			if (strcmp((const char*)clientName, (const char*)tempElement->clientData->name) == 0)
			{
				strcpy_s((char*)tempElement->clientData->directly, sizeof(tempElement->clientData->directly), newValue);
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
				strcpy_s((char*)tempElement->clientData->flag, sizeof(tempElement->clientData->flag), "1\0");
				return true;
			}
			tempElement = tempElement->nextElement;
		}
	}
	return false;
	*/
}
