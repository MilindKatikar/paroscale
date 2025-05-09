#include "library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASH_TABLE_BUCKETS 256

//pHLRUCache should be initialized
//pCachedData should have valid data
//Still checks are added against NULL pointer
RetCodeE AddToHashTable(LRUCacheP pLRUCache, CachedDataP pCachedData)
{
	RetCodeE eRetCode = E_FAILED;

	if(NULL != pLRUCache &&
	   NULL != pLRUCache->pHashTable &&
	   NULL != pLRUCache->pHashTable->pCacheNodeBuckets &&
	   NULL != pCachedData &&
	   NULL != pCachedData->pszName &&
	   0 < strlen(pCachedData->pszName))
	{
		HashTableP pHashTable = pLRUCache->pHashTable;

		//Find bucket
		int nBucketIndex = 0;
		for (int i = 0; i < strlen(pCachedData->pszName); i++)
		{
			nBucketIndex += pCachedData->pszName[i];
		}
		nBucketIndex %= HASH_TABLE_BUCKETS; //hash key

		if(NULL == pHashTable->pCacheNodeBuckets[nBucketIndex])
		{
			pHashTable->pCacheNodeBuckets[nBucketIndex] =
			                                  (CacheNodeP)malloc(sizeof(CacheNodeS));
			pHashTable->pCacheNodeBuckets[nBucketIndex]->pCachedData = pCachedData;
			pHashTable->pCacheNodeBuckets[nBucketIndex]->pCacheNodeLRUListNext = NULL;
			pHashTable->pCacheNodeBuckets[nBucketIndex]->pCacheNodeCollisionListNext = NULL;
			++pLRUCache->nKeyCount;
			eRetCode = E_SUCCESS;
		}
		else
		{
			//hash key collision
			CacheNodeP pCacheNode = NULL;
			for(pCacheNode = pHashTable->pCacheNodeBuckets[nBucketIndex];
			    NULL != pCacheNode;
			    pCacheNode = pCacheNode->pCacheNodeCollisionListNext)
			{
				if(0 == strcmp(pCacheNode->pCachedData->pszName, pCachedData->pszName))
				{
					//key already exists
					eRetCode = E_ENTRYEXIST;
					break;
				}
			}
			if(NULL != pCacheNode)
			{
				//name does not exist add to list
				CacheNodeP pNewCacheNode = (CacheNodeP)malloc(sizeof(CacheNodeS));
				pNewCacheNode->pCachedData                 = pCachedData;
				pNewCacheNode->pCacheNodeLRUListNext       = NULL;
				pNewCacheNode->pCacheNodeCollisionListNext = NULL;
				pNewCacheNode->pCacheNodeLRUListPrev       = NULL;

				pNewCacheNode->pCacheNodeCollisionListNext =
					pHashTable->pCacheNodeBuckets[nBucketIndex]->pCacheNodeCollisionListNext;
				pHashTable->pCacheNodeBuckets[nBucketIndex]->pCacheNodeCollisionListNext =
					pNewCacheNode;
				++pLRUCache->nKeyCount;
				eRetCode = E_SUCCESS;
			}
		}

		{
			//Update the LRU list
			CacheNodeP pCacheNodeRecent = pHashTable->pCacheNodeBuckets[nBucketIndex];
			if(NULL == pCacheNodeRecent->pCacheNodeLRUListNext &&
			   NULL == pCacheNodeRecent->pCacheNodeLRUListPrev)
			{
				//Newly added node to hash table and not present in the list
				CacheNodeP pCacheNodeLRUHeadCurrent =
				                    pLRUCache->pListCacheNodeLRUOrder->pCacheNodeHead;
				pCacheNodeRecent->pCacheNodeLRUListNext = pCacheNodeLRUHeadCurrent;
				pCacheNodeRecent->pCacheNodeLRUListPrev = NULL;
				pCacheNodeLRUHeadCurrent->pCacheNodeLRUListPrev = pCacheNodeRecent;
				pLRUCache->pListCacheNodeLRUOrder->pCacheNodeHead = pCacheNodeRecent;
			}
			else
			{
				//Already in the hash table and list
				if(pCacheNodeRecent == pLRUCache->pListCacheNodeLRUOrder->pCacheNodeHead)
				{
					//no need to change the order
				}
				else
				{
					if(NULL != pCacheNodeRecent->pCacheNodeLRUListNext &&
					   NULL != pCacheNodeRecent->pCacheNodeLRUListPrev)
					{
						//node is in the middle
						//remove recent node from LRU list
						pCacheNodeRecent->pCacheNodeLRUListPrev->pCacheNodeLRUListNext =
																				 pCacheNodeRecent->pCacheNodeLRUListNext;
						pCacheNodeRecent->pCacheNodeLRUListNext->pCacheNodeLRUListPrev =
																				 pCacheNodeRecent->pCacheNodeLRUListPrev;
						//node is out of the LRU list
						//pCacheNodeRecent->pCacheNodeLRUListNext = NULL;
						//pCacheNodeRecent->pCacheNodeLRUListPrev = NULL;
					}
					else
					{
						//node should be last
						//assert(NULL == pCacheNodeRecent->pCacheNodeLRUListNext &&
						//       NULL != pCacheNodeRecent->pCacheNodeLRUListPrev);
						pCacheNodeRecent->pCacheNodeLRUListPrev->pCacheNodeCollisionListNext = NULL;
						//Update tail
						pLRUCache->pListCacheNodeLRUOrder->pCacheNodeTail =
						                           pCacheNodeRecent->pCacheNodeLRUListPrev;
						//node is out of the LRU list
						//pCacheNodeRecent->pCacheNodeLRUListPrev = NULL;
					}

					//make recent node a new head
					{
						CacheNodeP pCacheNodeLRUHeadCurrent =
						                 pLRUCache->pListCacheNodeLRUOrder->pCacheNodeHead;
						pCacheNodeRecent->pCacheNodeLRUListNext = pCacheNodeLRUHeadCurrent;
						pCacheNodeRecent->pCacheNodeLRUListPrev = NULL;
						pCacheNodeLRUHeadCurrent->pCacheNodeLRUListPrev = pCacheNodeRecent;
						pLRUCache->pListCacheNodeLRUOrder->pCacheNodeHead = pCacheNodeRecent;
					}
				}
			}
			if(1 == pLRUCache->nKeyCount)
			{
				//Set tail
				pLRUCache->pListCacheNodeLRUOrder->pCacheNodeTail =
				                     pLRUCache->pListCacheNodeLRUOrder->pCacheNodeHead;
			}
		}
	}

	return eRetCode;
}

extern RetCodeE CreateAndInitLRUCache(LRUCacheP *ppLRUCache)
{

	RetCodeE eRetCode = E_FAILED;

	if(NULL != ppLRUCache && NULL == *ppLRUCache)
	{
		LRUCacheP pLRUCache = (LRUCacheP)malloc(sizeof(LRUCacheS));

		pLRUCache->pHashTable = malloc(sizeof(HashTableS));
		pLRUCache->pHashTable->nNoBuckets = HASH_TABLE_BUCKETS;
		pLRUCache->pHashTable->pCacheNodeBuckets =
		               (CacheNodeP)malloc(HASH_TABLE_BUCKETS * sizeof(CacheNodeP));
		memset(pLRUCache->pHashTable->pCacheNodeBuckets, 0,
		                  HASH_TABLE_BUCKETS * sizeof(CacheNodeP));
		pLRUCache->nKeyCount = 0;
		pLRUCache->pListCacheNodeLRUOrder = NULL;

		*ppLRUCache = pLRUCache;
	}

	return eRetCode;
}

extern RetCodeE AddToLRUCache(LRUCacheP pLRUCache, char *pszName)
{
	CachedDataP pCachedData = (CachedDataP)malloc(sizeof(CachedDataS));
	pCachedData->pszName = (char *)malloc(strlen(pszName) + 1);
	strcpy(pCachedData->pszName, pszName);
	RetCodeE eRetCode = AddToHashTable(pLRUCache, pCachedData);
	if(E_ENTRYEXIST == eRetCode)
	{
		free(pCachedData->pszName);
		free(pCachedData);
	}
	return eRetCode;
}

extern RetCodeE FindInLRUCache(LRUCacheP pLRUCache, char *pszName)
{
	RetCodeE eRetCode = E_FAILED;

	if(NULL != pLRUCache &&
		 NULL != pLRUCache->pHashTable &&
		 NULL != pLRUCache->pHashTable->pCacheNodeBuckets &&
		 NULL != pszName &&
		 0 < strlen(pszName))
	{
		HashTableP pHashTable = pLRUCache->pHashTable;

		//Find bucket
		int nBucketIndex = 0;
		for(int i = 0; i < strlen(pszName); i++)
		{
			nBucketIndex += pszName[i];
		}
		nBucketIndex %= HASH_TABLE_BUCKETS; //hash key

		if(NULL == pHashTable->pCacheNodeBuckets[nBucketIndex])
		{
			eRetCode = E_ENTRYDOESNOTEXIST;
		}
		else
		{
			//hash key collision
			CacheNodeP pCacheNode = NULL;
			for(pCacheNode = pHashTable->pCacheNodeBuckets[nBucketIndex];
					 NULL != pCacheNode;
					 pCacheNode = pCacheNode->pCacheNodeCollisionListNext)
			{
				if(0 == strcmp(pCacheNode->pCachedData->pszName, pszName))
				{
					//key already exists
					eRetCode = E_ENTRYEXIST;
					break;
				}
			}
			if(NULL == pCacheNode)
			{
				eRetCode = E_ENTRYDOESNOTEXIST;
			}
		}
	}

	return eRetCode;
}

extern RetCodeE RemoveFromLRUCache(LRUCacheP pLRUCache)
{
	RetCodeE eRetCode = E_FAILED;

	if(NULL != pLRUCache &&
		 0    < pLRUCache->nKeyCount)
	{
		HashTableP pHashTable = pLRUCache->pHashTable;
		CacheNodeP pCacheNodeToRemove = NULL;
		if(1 == pLRUCache->nKeyCount)
		{
			pCacheNodeToRemove = pLRUCache->pListCacheNodeLRUOrder->pCacheNodeHead;
		}
		else
		{
			pCacheNodeToRemove = pLRUCache->pListCacheNodeLRUOrder->pCacheNodeTail;
			pLRUCache->pListCacheNodeLRUOrder->pCacheNodeTail =
			 pLRUCache->pListCacheNodeLRUOrder->pCacheNodeTail->pCacheNodeLRUListPrev;
		}

		//Remove from hash table
		//Find bucket
		int nBucketIndex = 0;
		for(int i = 0; i < strlen(pCacheNodeToRemove->pCachedData->pszName); i++)
		{
			nBucketIndex += pCacheNodeToRemove->pCachedData->pszName[i];
		}
		nBucketIndex %= HASH_TABLE_BUCKETS; //hash key

		if(NULL == pHashTable->pCacheNodeBuckets[nBucketIndex])
		{
			//This should not happen as entry was in the list
			eRetCode = E_ENTRYDOESNOTEXIST;
		}
		else
		{
			CacheNodeP pCacheNode = NULL;
			if(pCacheNodeToRemove == pHashTable->pCacheNodeBuckets[nBucketIndex])
			{
				pHashTable->pCacheNodeBuckets[nBucketIndex] = pCacheNodeToRemove->pCacheNodeLRUListNext;
				--pLRUCache->nKeyCount;
			}
			else
			{
				for(pCacheNode = pHashTable->pCacheNodeBuckets[nBucketIndex];
					  NULL != pCacheNode;
					  pCacheNode = pCacheNode->pCacheNodeCollisionListNext)
				{
					if(pCacheNodeToRemove == pCacheNode->pCacheNodeCollisionListNext)
					{
						pCacheNode->pCacheNodeCollisionListNext =
						  pCacheNodeToRemove->pCacheNodeCollisionListNext;
						--pLRUCache->nKeyCount;
						break;
					}
				}
			}

			free(pCacheNodeToRemove);
			eRetCode = E_SUCCESS;
		}
	}

	return eRetCode;
}
