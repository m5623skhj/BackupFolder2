#pragma once
#include <new.h>
#include <stdlib.h>

#define NODE_CODE	((int)0x3f08cc9d)

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
	m_Top.pBlockPtr = nullptr;
	m_Top.ullBlockID = 0;
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
				CurTop.ullBlockID = m_Top.ullBlockID;

				NewTop.pBlockPtr = CurTop.pBlockPtr->stpNextBlock;
				NewTop.ullBlockID = CurTop.ullBlockID + 1;
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
				CurTop.ullBlockID = m_Top.ullBlockID;

				NewTop.pBlockPtr = CurTop.pBlockPtr->stpNextBlock;
				NewTop.ullBlockID = CurTop.ullBlockID + 1;
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

	st_BLOCK_INFO CurTop, NewTop;
	NewTop.pBlockPtr = PushPtr;
	//st_BLOCK_NODE *pCurTop;
	do {
		CurTop.pBlockPtr = m_Top.pBlockPtr;
		CurTop.ullBlockID = m_Top.ullBlockID;
		
		NewTop.pBlockPtr->stpNextBlock = CurTop.pBlockPtr;
		NewTop.ullBlockID = CurTop.ullBlockID + 1;

		//pCurTop = m_Top.pBlockPtr;
		//PushPtr->stpNextBlock = pCurTop;

		//PushPtr->stpNextBlock = m_Top.pBlockPtr;
	} while (!InterlockedCompareExchange128(
		(LONG64*)&m_Top, (LONG64)NewTop.ullBlockID, (LONG64)NewTop.pBlockPtr, (LONG64*)&CurTop));
		//while (InterlockedCompareExchange64(
		//(volatile LONG64*)&m_Top.pBlockPtr, (LONG64)PushPtr, (LONG64)pCurTop) != (LONG64)pCurTop);
		//(volatile LONG64*)&m_Top.pBlockPtr, (LONG64)PushPtr, (LONG64)PushPtr->stpNextBlock) != (LONG64)PushPtr->stpNextBlock);

	InterlockedDecrement((UINT*)&m_iUseCount);

	if (m_bIsPlacementNew)
		PushPtr->NodeData.~Data();
}