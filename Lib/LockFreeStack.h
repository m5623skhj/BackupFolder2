#pragma once
#include "LockFreeMemoryPool.h"

template <typename Data>
class CLockFreeStack
{
private:
	struct st_NODE
	{
		Data		ItemData;
		st_NODE		*pNext;
	};
	struct st_TOP_INFO
	{
		st_NODE										*pTopNodePtr;
		ULONGLONG									ullBlockID;
	};

	UINT											m_iRestStackSize;
	__declspec(align(16)) st_TOP_INFO				m_Top;
	CLockFreeMemoryPool<st_NODE>					*m_pLockFreeMemoryPool;

public:
	CLockFreeStack();
	~CLockFreeStack();

	void Push(Data InputData);
	bool Pop(Data *pOutData);
	//bool Peek(Data *pOutData);
	int GetRestStackSize();
	int GetMemoryPoolAllocCount();
	int GetMemoryPoolUseSize();
};

template <typename Data>
CLockFreeStack<Data>::CLockFreeStack()
{
	m_Top.pTopNodePtr = nullptr;
	m_Top.ullBlockID = 0;
	m_iRestStackSize = 0;
	m_pLockFreeMemoryPool = new CLockFreeMemoryPool<st_NODE>(0, false);
}

template <typename Data>
CLockFreeStack<Data>::~CLockFreeStack()
{
	while (m_Top.pTopNodePtr != nullptr)
	{
		st_NODE *pNode = m_Top.pTopNodePtr;
		m_pLockFreeMemoryPool->Push(pNode);
		m_Top.pTopNodePtr = m_Top.pTopNodePtr->pNext;
	}
	delete m_pLockFreeMemoryPool;
}

template <typename Data>
void CLockFreeStack<Data>::Push(Data InputData)
{
	st_NODE *pNewTop = m_pLockFreeMemoryPool->Pop();
	st_NODE *pCurTop;
	//st_TOP_INFO CurTop, NewTop;
	//NewTop.pTopNodePtr = m_pLockFreeMemoryPool->Pop();
	do {
		pCurTop = m_Top.pTopNodePtr;
		//CurTop.pTopNodePtr = m_Top.pTopNodePtr;
		//CurTop.ullBlockID = m_Top.ullBlockID;

		pNewTop->pNext = pCurTop;
		pNewTop->ItemData = InputData;
		//NewTop.pTopNodePtr->pNext = CurTop.pTopNodePtr;
		////NewTop.ullBlockID = CurTop.ullBlockID + 1;
		//NewTop.pTopNodePtr->ItemData = InputData;
		//pCurTop = m_Top.pTopNodePtr;
		//pNewTop->pNext = pCurTop;
		//pNewTop->ItemData = InputData;
	} 
	/*while (!InterlockedCompareExchange128(
		(LONG64*)&m_Top, (LONG64)NewTop.ullBlockID, (LONG64)NewTop.pTopNodePtr, (LONG64*)&CurTop));*/
	while (InterlockedCompareExchange64(
		(LONG64*)&m_Top.pTopNodePtr, (LONG64)pNewTop, (LONG64)pCurTop) != (LONG64)pCurTop);

	// 디버깅용 코드
	// 실제 사용시 제거할 것
	//InterlockedIncrement(&m_iRestStackSize);
}

template <typename Data>
bool CLockFreeStack<Data>::Pop(Data *pOutData)
{
	st_TOP_INFO NewTop, CurTop;
	// 디버깅용 코드
	// 실제 사용시 제거할 것
	//InterlockedDecrement(&m_iRestStackSize);

	do {
		//CurTop = m_Top;
		CurTop.ullBlockID = m_Top.ullBlockID;
		CurTop.pTopNodePtr = m_Top.pTopNodePtr;
		//CurTop.ullBlockID = -1;
		//CurTop.pTopNodePtr = (st_NODE*)1;
		//InterlockedCompareExchange128((LONG64*)&m_Top, 1, -1, (LONG64*)&CurTop);
		//if (CurTop.pTopNodePtr == nullptr)
		//{
		//	pOutData = nullptr;
		//	return false;
		//}
		NewTop.ullBlockID = CurTop.ullBlockID + 1;
		NewTop.pTopNodePtr = CurTop.pTopNodePtr->pNext;
	} while (!InterlockedCompareExchange128(
		(volatile LONG64*)&m_Top, (LONG64)NewTop.ullBlockID, (LONG64)NewTop.pTopNodePtr, (LONG64*)&CurTop));

	*pOutData = CurTop.pTopNodePtr->ItemData;
	m_pLockFreeMemoryPool->Push(CurTop.pTopNodePtr);
	return true;
}

//template <typename Data>
//bool CLockFreeStack<Data>::Peek(Data *pOutData)
//{
//	if (m_iRestStackSize <= 0)
//		return false;
//
//	pOutData = m_Top.pTopNodePtr;
//	return true;
//}

template <typename Data>
int CLockFreeStack<Data>::GetRestStackSize()
{
	return m_iRestStackSize;
}

template <typename Data>
int CLockFreeStack<Data>::GetMemoryPoolAllocCount()
{
	return m_pLockFreeMemoryPool->GetAllocCount();
}

template <typename Data>
int CLockFreeStack<Data>::GetMemoryPoolUseSize()
{
	return m_pLockFreeMemoryPool->GetUseCount();
}