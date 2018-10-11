#pragma once
#include "PreCompile.h"
#include <new.h>
#include <stdlib.h>

#define NODE_CODE	((int)0x3f08cc9d)

template <typename Data>
class CFreeList
{
private :
	struct st_BLOCK_NODE
	{
		st_BLOCK_NODE()
		{
			stpNextBlock = NULL;
			iCode = NODE_CODE;
		}

		st_BLOCK_NODE		*stpNextBlock;
		Data				NodeData;
		int					iCode;
	};

	bool				m_bIsPlacementNew;
	int					m_iUseCount;
	int					m_iAllocCount;
	st_BLOCK_NODE		*m_pTop;

public :
	CFreeList(unsigned short MakeInInit, bool bIsPlacementNew);
	~CFreeList();

	Data *Alloc();
	void Free(Data *pData);
	int GetUseCount() { return m_iUseCount; }
	int GetAllocCount() { return m_iAllocCount; }
};


template <typename Data>
CFreeList<Data>::CFreeList(unsigned short MakeInInit, bool bIsPlacementNew) 
	: m_bIsPlacementNew(bIsPlacementNew), m_iUseCount(0), m_iAllocCount(0), m_pTop(NULL)
{
	for (unsigned short i = 0; i < MakeInInit; i++)
	{
		st_BLOCK_NODE *NewNode = new st_BLOCK_NODE();

		NewNode->stpNextBlock = m_pTop;
		m_pTop = NewNode;
	}
	m_iAllocCount = MakeInInit;
}

template <typename Data>
CFreeList<Data>::~CFreeList()
{
	st_BLOCK_NODE *next;
	if (!m_bIsPlacementNew)
	{
		while (m_pTop)
		{
			next = m_pTop->stpNextBlock;
			delete m_pTop;
			m_pTop = next;
		}
	}
	else
	{
		while (m_pTop)
		{
			next = m_pTop->stpNextBlock;
			free(m_pTop);
			m_pTop = next;
		}
	}
}

template <typename Data>
Data *CFreeList<Data>::Alloc()
{
	st_BLOCK_NODE *retptr;
	if (!m_bIsPlacementNew)
	{
		if (m_pTop != NULL)
		{
			retptr = m_pTop;
			m_pTop = m_pTop->stpNextBlock;
			retptr->stpNextBlock = NULL;
		}
		else
		{
			m_iAllocCount++;
			retptr = new st_BLOCK_NODE();
		}
	}
	else
	{
		if (m_pTop != NULL)
		{
			retptr = m_pTop;
			m_pTop = m_pTop->stpNextBlock;
		}
		else
		{
			m_iAllocCount++;
			retptr = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));
		}
		new (retptr) st_BLOCK_NODE();
	}

	m_iUseCount++;
	return &(retptr->NodeData);
}

template <typename Data>
void CFreeList<Data>::Free(Data *pData)
{
	st_BLOCK_NODE *temp = (st_BLOCK_NODE*)((char*)pData - 8);
	if (temp->iCode != NODE_CODE)
		g_Dump.Crash();
		//return;

	temp->stpNextBlock = m_pTop;
	m_pTop = temp;
	m_iUseCount--;

	if(m_bIsPlacementNew)
		temp->NodeData.~Data();
}