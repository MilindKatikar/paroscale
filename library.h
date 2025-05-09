#ifndef TESTLIB_LIBRARY_H
#define TESTLIB_LIBRARY_H

typedef enum
{
	E_FAILED = 0,
	E_SUCCESS,
	E_ENTRYEXIST,
	E_ENTRYDOESNOTEXIST,
	//More elaborate return codes
} RetCodeE;

typedef struct CachedDataS
{
	char *pszName;
	//other required members/info
} *CachedDataP;
typedef struct CachedDataS CachedDataS;

typedef struct CacheNodeS
{
	CachedDataP pCachedData;
	//other required members/info

	struct CacheNodeS *pCacheNodeLRUListNext;
	struct CacheNodeS *pCacheNodeLRUListPrev;
	struct CacheNodeS *pCacheNodeCollisionListNext;
	//This should also have been doubly linked list
} *CacheNodeP;
typedef struct CacheNodeS CacheNodeS;

typedef struct HashTableS
{
	int   nNoBuckets;
	CacheNodeP *pCacheNodeBuckets;

	//other required info/members
} *HashTableP;
typedef struct HashTableS HashTableS;

typedef struct ListCacheNodeS
{
	CacheNodeP pCacheNodeHead;
	CacheNodeP pCacheNodeTail;
} *ListCacheNodeP;
typedef struct ListCacheNodeS ListCacheNodeS;

typedef struct LRUCacheS
{
	HashTableP     pHashTable;
	int            nKeyCount;
	ListCacheNodeP pListCacheNodeLRUOrder;
} *LRUCacheP;
typedef struct LRUCacheS LRUCacheS;

extern RetCodeE CreateAndInitLRUCache(LRUCacheP *ppLRUCache);
extern RetCodeE AddToLRUCache(LRUCacheP pLRUCache, char *pszName);
extern RetCodeE FindInLRUCache(LRUCacheP pLRUCache, char *pszName)
extern RetCodeE RemoveFromLRUCache(LRUCacheP pLRUCache);

#endif //TESTLIB_LIBRARY_H
