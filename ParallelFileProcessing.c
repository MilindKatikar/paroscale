//Assumption:
//File is a binary file
//Number of bytes required for each int is of sizeof(int)
//this will help counting number of integers in a given file

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

//Types

typedef struct ListIntS
{
	int nInt;
	struct ListIntS *pNext;
} *ListIntP;
typedef struct ListIntS ListIntS;

typedef struct ProcessingParamsS
{
	int      fd;
	int      nOffset;
	int      nIntsToProcess;
	int      nThreadNumber;
	ListIntP pListUniqueHead; //Output
} *ProcessingParamsP;
typedef struct ProcessingParamsS ProcessingParamsS;

typedef struct ListProcessingParamsS
{
	ProcessingParamsP pProcessingParams;
	pthread_t         pthreadID;
	struct ListProcessingParamsS *pNext;
} *ListProcessingParamsP;
typedef struct ListProcessingParamsS ListProcessingParamsS;

//Globals

pthread_mutex_t readBufferMutex = PTHREAD_MUTEX_INITIALIZER;
ListIntP gpListUniqueHead = NULL;


//Functions

//Returns number of ints read
int ReadInts(int fd, int nOffset, int nNoInts, char *pBuff)
{
	pthread_mutex_lock(&readBufferMutex); // Lock the mutex
	lseek(fd, nOffset, SEEK_SET);
	int nBytesRead = read(fd, pBuff, nNoInts * sizeof(int));
	pthread_mutex_unlock(&readBufferMutex); // Unlock the mutex
	return nBytesRead / sizeof(int);
}

int CompareInts(const void *nLeft, const void *nRight)
{
	return (*(int*)nLeft - *(int*)nRight);
}

void AddToList(ListIntP *ppListUniqueHead, int nInt)
{
	//Add nInt to list of it is not already present
	ListIntP pListUnique = *ppListUniqueHead;
	if(NULL == pListUnique)
	{
		pListUnique = (ListIntP)malloc(sizeof(ListIntS));
		pListUnique->nInt = nInt;
		pListUnique->pNext = NULL;
		*ppListUniqueHead = pListUnique;
	}
	else
	{
		while(NULL != pListUnique)
		{
			if(pListUnique->nInt == nInt)
			{
				return; //Already present
			}
			pListUnique = pListUnique->pNext;
		}
		ListIntP pNewNode = (ListIntP)malloc(sizeof(ListIntS));
		pNewNode->nInt = nInt;
		pNewNode->pNext = *ppListUniqueHead;
		*ppListUniqueHead = pNewNode;
	}
}

void MergeToGlobalList(ListIntP pListUniqueHead)
{
	//Merge sorted list pListUniqueHead into gpListUniqueHead
	ListIntP pListUniqueThread = pListUniqueHead;
	ListIntP pListUniqueGlobal = gpListUniqueHead;
	if (NULL == pListUniqueGlobal)
	{
		gpListUniqueHead = pListUniqueHead;
	}
	else
	{
		ListIntP pListUniqueGlobalPrev = NULL;
		while(NULL != pListUniqueThread && NULL != pListUniqueGlobal)
		{
			if(pListUniqueThread->nInt < pListUniqueGlobal->nInt)
			{
				ListIntP pListUniqueTemp = malloc(sizeof(ListIntP));
				if(NULL == pListUniqueGlobalPrev)
				{
					pListUniqueTemp->nInt = pListUniqueThread->nInt;
					gpListUniqueHead = pListUniqueTemp;
					pListUniqueTemp->pNext = pListUniqueGlobal;
					pListUniqueGlobalPrev = pListUniqueTemp;
				}
				else
				{
					pListUniqueGlobalPrev->pNext = pListUniqueTemp;
					pListUniqueTemp->pNext = pListUniqueGlobal;
					pListUniqueGlobalPrev = pListUniqueTemp;
				}
				pListUniqueThread = pListUniqueThread->pNext;
			}
			else if(pListUniqueThread->nInt > pListUniqueGlobal->nInt)
			{
				pListUniqueGlobalPrev = pListUniqueGlobal;
				pListUniqueGlobal = pListUniqueGlobal->pNext;
			}
			else
			{
				pListUniqueGlobalPrev = pListUniqueGlobal;
				pListUniqueGlobal = pListUniqueGlobal->pNext;
				pListUniqueThread = pListUniqueThread->pNext;
			}
		}
	}
}

//thread function for finding unique numbers
void* FindUniqueNumbers(void *arg)
{
#define MAX_INTS_TO_READ 1024
	char chBuffer[sizeof(int) * 1024];
	int nOffset    = 0;
	ListIntP pListUniqueHead = NULL;
	ProcessingParamsP pProcessingParams = (ProcessingParamsP)arg;

	nOffset = pProcessingParams->nOffset;
	for(int nIntsRead = 0; nIntsRead < pProcessingParams->nIntsToProcess;)
	{
		int nIntsToRead  = 0;

		if (pProcessingParams->nIntsToProcess - nIntsRead < MAX_INTS_TO_READ)
		{
			nIntsToRead = pProcessingParams->nIntsToProcess - nIntsRead;
		}
		else
		{
			nIntsToRead = MAX_INTS_TO_READ;
		}
		int nRead = ReadInts(pProcessingParams->fd,
													 nOffset,
													 nIntsToRead,
													 chBuffer);
		nIntsRead += nRead;
		nOffset += nRead * sizeof(int);

		//Find unique int from (int*)chBuffer
		int* pInts = (int*)chBuffer;
		qsort(pInts, nRead, sizeof(int), CompareInts);
		for(int i = 0, j = 0; i < nRead; i++)
		{
			AddToList(&pListUniqueHead, pInts[i]);
			for (int j = i + 1; j < nRead; j++)
			{
				if (pInts[i] != pInts[j])
				{
					i = j;
					break;
				}
			}
		}
	}

	pProcessingParams->pListUniqueHead = pListUniqueHead;
	return pListUniqueHead;
}

int ProcessFile(char* pszFileName, int nThreadCount)
{
	// Open the file
	int fd = open(pszFileName, O_RDONLY);
	if(fd == -1)
	{
		perror("Error opening file");
		return EXIT_FAILURE;
	}

	// Get the size of the file
	struct stat statFile;
	if(fstat(fd, &statFile) == -1)
	{
		perror("Error getting file size");
		close(fd);
		return EXIT_FAILURE;
	}

	//if file is not in the multiple of sizeof(int)
	//skip last incomplete integer
	int nNoIntegers = statFile.st_size / sizeof(int);
	ListProcessingParamsP  pListProcessingParamsHead = NULL;
	int nOffset = 0;
	for(int i = 0; i < nThreadCount; i++)
	{
		pthread_t threadID;
		ProcessingParamsP pProcessingParams = (ProcessingParamsP)malloc(sizeof(ProcessingParamsS));
		pProcessingParams->fd             = fd;
		pProcessingParams->nOffset        = nOffset;
		pProcessingParams->nIntsToProcess = (nNoIntegers / nThreadCount);
		pProcessingParams->nThreadNumber  = i;
		pProcessingParams->pListUniqueHead = NULL;
		// Create the thread
		if(pthread_create(&threadID, NULL, FindUniqueNumbers, (void*)pProcessingParams) != 0)
		{
			free(pProcessingParams);
			perror("Failed to create thread");
			return 1;
		}
		else
		{
			if(NULL == pListProcessingParamsHead)
			{
				pListProcessingParamsHead = (ListProcessingParamsP)malloc(sizeof(ListProcessingParamsP));
				pListProcessingParamsHead->pProcessingParams = pProcessingParams;
				pListProcessingParamsHead->pthreadID = threadID;
				pListProcessingParamsHead->pNext = NULL;
			}
			else
			{
				ListProcessingParamsP pListProcessingParams =
									(ListProcessingParamsP)malloc(sizeof(ListProcessingParamsP));
				pListProcessingParams->pProcessingParams = pProcessingParams;
				pListProcessingParams->pthreadID = threadID;
				pListProcessingParams->pNext = pListProcessingParamsHead;
				pListProcessingParamsHead = pListProcessingParams;
			}
		}
		nOffset += pProcessingParams->nIntsToProcess * sizeof(int);
	}

	for(ListProcessingParamsP pListProcessingParams = pListProcessingParamsHead;
			NULL != pListProcessingParams;
			pListProcessingParams = pListProcessingParams->pNext)
	{
		pthread_join(pListProcessingParams->pthreadID, NULL);
		MergeToGlobalList(pListProcessingParams->pProcessingParams->pListUniqueHead);
	}

	// Close the file descriptor
	close(fd);
	return 0;
}

int main(int argc, char *argv[])
{
	if(argc != 3)
	{
		fprintf(stderr, "Usage: %s <filename> <thread count>\n", argv[0]);
		return EXIT_FAILURE;
	}
		// Get the number of threads
	int nThreadCount = atoi(argv[2]);
	if (0 == nThreadCount)
	{
		nThreadCount = 1;
	}

	return ProcessFile(argv[1], nThreadCount);
}
