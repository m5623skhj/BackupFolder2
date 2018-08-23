#pragma once
#include <new.h>
#include <stdlib.h>

#define NODE_CODE	((int)0x3f08cc9d)

////////////////////////////////////////////////////////////////
// CLockFreeMemoryPool Class
////////////////////////////////////////////////////////////////

template <typename Data>
class CLockFreeMemoryPool
{
private:
	struct st_BLOCK_NODE
	{
		st_BLOCK_NODE()
		{
			stpNextBlock = NULL;
			iCode = NODE_CODE;
		}

		int									iCode;
		Data								NodeData;
		st_BLOCK_NODE						*stpNextBlock;
	};

	struct st_BLOCK_INFO
	{
		st_BLOCK_NODE						*pBlockPtr;
		ULONGLONG							ullBlockID;
	};

	bool									m_bIsPlacementNew;
	int										m_iUseCount;
	int										m_iAllocCount;
	__declspec(align(16)) st_BLOCK_INFO		m_Top;

public:
	CLockFreeMemoryPool(UINT MakeInInit, bool bIsPlacementNew);
	~CLockFreeMemoryPool();

	Data *Pop();
	void Push(Data *pData);
	int GetUseCount() { return m_iUseCount; }
	int GetAllocCount() { return m_iAllocCount; }
};


template <typename Data>
CLockFreeMemoryPool<Data>::CLockFreeMemoryPool(UINT MakeInInit, bool bIsPlacementNew)
	: m_bIsPlacementNew(bIsPlacementNew), m_iUseCount(0), m_iAllocCount(0)
{
	m_Top.ullBlockID = 0;
	m_Top.pBlockPtr = nullptr;
	for (UINT i = 0; i < MakeInInit; i++)
	{
		++m_Top.ullBlockID;
		st_BLOCK_NODE *NewNode = new st_BLOCK_NODE();

		NewNode->stpNextBlock = m_Top.pBlockPtr;
		m_Top.pBlockPtr = NewNode;
	}
	m_iAllocCount = MakeInInit;
}

template <typename Data>
CLockFreeMemoryPool<Data>::~CLockFreeMemoryPool()
{
	st_BLOCK_NODE *next;
	if (!m_bIsPlacementNew)
	{
		while (m_Top.pBlockPtr)
		{
			next = m_Top.pBlockPtr->stpNextBlock;
			delete m_Top.pBlockPtr;
			m_Top.pBlockPtr = next;
		}
	}
	else
	{
		while (m_Top.pBlockPtr)
		{
			next = m_Top.pBlockPtr->stpNextBlock;
			free(m_Top.pBlockPtr);
			m_Top.pBlockPtr = next;
		}
	}
}

template <typename Data>
Data *CLockFreeMemoryPool<Data>::Pop()
{
	__declspec(align(16)) st_BLOCK_INFO CurTop, NewTop;
	InterlockedIncrement((UINT*)&m_iUseCount);

	if (!m_bIsPlacementNew)
	{
		if (m_Top.pBlockPtr != nullptr)
		{
			do {
				CurTop.ullBlockID = m_Top.ullBlockID;
				CurTop.pBlockPtr = m_Top.pBlockPtr;

				//CurTop = m_Top;
				//CurTop.pBlockPtr = (st_BLOCK_NODE*)1;
				//CurTop.ullBlockID = -1;
				//InterlockedCompareExchange128((LONG64*)&m_Top, 1, -1, (LONG64*)&CurTop);
				if (CurTop.pBlockPtr == nullptr)
				{
					InterlockedIncrement((UINT*)&m_iAllocCount);
					CurTop.pBlockPtr = new st_BLOCK_NODE();
					break;
				}

				NewTop.ullBlockID = CurTop.ullBlockID + 1;
				NewTop.pBlockPtr = CurTop.pBlockPtr->stpNextBlock;
			} while (!InterlockedCompareExchange128(
				(LONG64*)&m_Top, (LONG64)NewTop.ullBlockID, (LONG64)NewTop.pBlockPtr, (LONG64*)&CurTop));

			CurTop.pBlockPtr->stpNextBlock = nullptr;
		}
		else
		{
			InterlockedIncrement((UINT*)&m_iAllocCount);
			CurTop.pBlockPtr = new st_BLOCK_NODE();
		}
	}
	else
	{
		if (m_Top.pBlockPtr != nullptr)
		{
			do {
				CurTop.ullBlockID = m_Top.ullBlockID;
				CurTop.pBlockPtr = m_Top.pBlockPtr;

				//CurTop = m_Top;
				//CurTop.pBlockPtr = (st_BLOCK_NODE*)1;
				//CurTop.ullBlockID = -1;
				//InterlockedCompareExchange128((LONG64*)&m_Top, 1, -1, (LONG64*)&CurTop);
				if (CurTop.pBlockPtr == nullptr)
				{
					InterlockedIncrement((UINT*)&m_iAllocCount);
					CurTop.pBlockPtr = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));
					break;
				}

				NewTop.ullBlockID = CurTop.ullBlockID + 1;
				NewTop.pBlockPtr = CurTop.pBlockPtr->stpNextBlock;
			} while (!InterlockedCompareExchange128(
				(LONG64*)&m_Top, (LONG64)NewTop.ullBlockID, (LONG64)NewTop.pBlockPtr, (LONG64*)&CurTop));
		}
		else
		{
			InterlockedIncrement((UINT*)&m_iAllocCount);
			CurTop.pBlockPtr = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));
		}
		new (CurTop.pBlockPtr) st_BLOCK_NODE();
	}

	return &(CurTop.pBlockPtr->NodeData);
}

template <typename Data>
void CLockFreeMemoryPool<Data>::Push(Data *pData)
{
	st_BLOCK_NODE *PushPtr = (st_BLOCK_NODE*)((char*)pData - 8);
	if (PushPtr->iCode != NODE_CODE)
		return;

	st_BLOCK_NODE *pCurTop = nullptr;
	//st_BLOCK_INFO CurTop, NewTop;
	//NewTop.pBlockPtr = PushPtr;
	//st_BLOCK_NODE *pCurTop;
	do {
		pCurTop = m_Top.pBlockPtr;
		//CurTop.pBlockPtr = m_Top.pBlockPtr;
		PushPtr->stpNextBlock = pCurTop;
		//NewTop.pBlockPtr->stpNextBlock = CurTop.pBlockPtr;

		//pCurTop = m_Top.pBlockPtr;
		//PushPtr->stpNextBlock = pCurTop;
		//PushPtr->stpNextBlock = m_Top.pBlockPtr;
	} /*while (!InterlockedCompareExchange128(
		(LONG64*)&m_Top, (LONG64)NewTop.ullBlockID, (LONG64)NewTop.pBlockPtr, (LONG64*)&CurTop));*/
	while (InterlockedCompareExchange64(
		//(volatile LONG64*)&m_Top.pBlockPtr, (LONG64)PushPtr, (LONG64)pCurTop) != (LONG64)pCurTop);
		(LONG64*)&m_Top.pBlockPtr, (LONG64)PushPtr, (LONG64)pCurTop) != (LONG64)pCurTop);

	InterlockedDecrement((UINT*)&m_iUseCount);

	if (m_bIsPlacementNew)
		PushPtr->NodeData.~Data();
}

#include "CrashDump.h"
CCrashDump dump;

///////////////////////////////
// CTLSMemoryPool Class
///////////////////////////////

#define df_CHUNK_ELEMENT_SIZE						3

template <typename Data>
class CTLSMemoryPool
{
private:
	///////////////////////////////
	// CChunk Class
	///////////////////////////////
	template <typename T>
	class CChunk
	{
	private:
		struct st_CHUNK_NODE
		{
			CChunk<T>								*pAssginedChunk;
			T										Data;
		};
		int											m_iChunkIndex;
		int											m_iNodeFreeCount;
		// 데이터만 줄 것임
		// 다만 청크를 알고 있어야 하기 때문에 각각의 노드들은 
		// 자신을 할당해준 청크를 기억하고 있어야 됨
		// 청크 구조체에 청크와 데이터를 넣어줌
		st_CHUNK_NODE								m_ChunkData[df_CHUNK_ELEMENT_SIZE];
		friend class CTLSMemoryPool;

	public:
		CChunk();
		~CChunk();

		// 마지막 노드가 할당 받았다면 false 를 반환하여 
		// Chunk 가 비었다는 것을 사용자에게 알림
		T* ChunkAlloc(bool *pOutNodeCanAllocMore);
		//bool ChunkAlloc(T *pOutData);
		bool ChunkFree();
	};
	bool									m_bIsPlacementNew;
	int										m_iUseChunkCount;
	int										m_iAllocChunkCount;
	int										m_iTLSIndex;
	CLockFreeMemoryPool<CChunk<Data>>		*m_pChunkMemoryPool;

public:
	CTLSMemoryPool(UINT MakeInInit, bool bIsPlacementNew);
	~CTLSMemoryPool();

	void Push(Data *pData);
	Data *Pop();
	int GetUseChunkCount()   { return m_iUseChunkCount; }
	int GetAllocChunkCount() { return m_iAllocChunkCount; }
};

///////////////////////////////
// CTLSMemoryPool Method
///////////////////////////////
template <typename Data>
CTLSMemoryPool<Data>::CTLSMemoryPool(UINT MakeInInit, bool bIsPlacementNew) :
	m_bIsPlacementNew(bIsPlacementNew), m_iUseChunkCount(0), m_iAllocChunkCount(MakeInInit)
{
	m_iTLSIndex = TlsAlloc();
	if (m_iTLSIndex == TLS_OUT_OF_INDEXES)
	{
		dump.Crash();
	}
	m_pChunkMemoryPool = new CLockFreeMemoryPool<CChunk<Data> >(MakeInInit, bIsPlacementNew);
}

template <typename Data>
CTLSMemoryPool<Data>::~CTLSMemoryPool()
{
	CChunk<Data> *TLSChunkPtr = (CChunk<Data>*)TlsGetValue(m_iTLSIndex);
	if (TLSChunkPtr != NULL)
		m_pChunkMemoryPool->Push(TLSChunkPtr);
	TlsFree(m_iTLSIndex);
	delete m_pChunkMemoryPool;
}

// 여기 이중 포인터를 다루는게 마음에 들지 않음
template <typename Data>
void CTLSMemoryPool<Data>::Push(Data *pData)
{
	CChunk<Data> **pChunk = ((CChunk<Data>**)((char*)pData - 8));
	++((*pChunk)->m_iNodeFreeCount);
	if ((*pChunk)->m_iNodeFreeCount >= df_CHUNK_ELEMENT_SIZE)
		m_pChunkMemoryPool->Push(*pChunk);
	//if (!pChunk->ChunkFree())
	//	m_pChunkMemoryPool->Push(pChunk);
}

template <typename Data>
Data* CTLSMemoryPool<Data>::Pop()
{
	CChunk<Data> *TLSChunkPtr = (CChunk<Data>*)TlsGetValue(m_iTLSIndex);
	if (GetLastError() != NO_ERROR)
		dump.Crash();

	if (TLSChunkPtr == NULL)
	{
		TLSChunkPtr = m_pChunkMemoryPool->Pop();
		TlsSetValue(m_iTLSIndex, TLSChunkPtr);
	}

	bool ChunkCanAllocMore;
	Data *pData = TLSChunkPtr->ChunkAlloc(&ChunkCanAllocMore);
	if (!ChunkCanAllocMore)
		TlsSetValue(m_iTLSIndex, NULL);

	return pData;
}

///////////////////////////////
// CChunk Method
///////////////////////////////
template <typename Data>
template <typename T>
CTLSMemoryPool<Data>::CChunk<T>::CChunk() :
	m_iChunkIndex(0), m_iNodeFreeCount(0)
{
	for (int i = 0; i < df_CHUNK_ELEMENT_SIZE; ++i)
		m_ChunkData[i].pAssginedChunk = this;
}

template <typename Data>
template <typename T>
CTLSMemoryPool<Data>::CChunk<T>::~CChunk()
{

}

template <typename Data>
template <typename T>
T* CTLSMemoryPool<Data>::CChunk<T>::ChunkAlloc(bool *pOutNodeCanAllocMore)
{
	++m_iChunkIndex;
	if (m_iChunkIndex >= df_CHUNK_ELEMENT_SIZE)
		*pOutNodeCanAllocMore = false;
	else
		*pOutNodeCanAllocMore = true;

	return &m_ChunkData[m_iChunkIndex - 1].Data;
}

//template <typename Data>
//template <typename T>
//bool CTLSMemoryPool<Data>::CChunk<T>::ChunkAlloc(T *pOutData)
//{
//	pOutData = &m_pChunkData[m_iChunkIndex].Data;
//
//	++m_iChunkIndex;
//	if (m_iChunkIndex >= df_CHUNK_ELEMENT_SIZE)
//		return false;
//	return true;
//}

template <typename Data>
template <typename T>
bool CTLSMemoryPool<Data>::CChunk<T>::ChunkFree()
{
	++m_iNodeFreeCount;
	if (m_iNodeFreeCount >= df_CHUNK_ELEMENT_SIZE)
		return false;
	return true;
}
