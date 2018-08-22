#pragma once

template <typename T>
class Stack
{
private :
	T	*m_pData;
	int m_iUseSize;
	int m_iStackMaxSize;

public :
	Stack(int StackSize)
	{
		m_pData = new T[StackSize];
		int m_iUseSize = -1;
		m_iStackMaxSize = StackSize;
	}
	~Stack()
	{
		delete[] m_pData;
	}

	int Push(T Data)
	{
		if (m_iUseSize >= m_iStackMaxSize)
			return -1;

		++m_iUseSize;
		m_pData[m_iUseSize - 1] = Data;
		return m_iUseSize;	
	}
	T Pop()
	{
		if (m_iUseSize <= 0)
			return NULL;

		--m_iUseSize;
		return m_pData[m_iUseSize];
	}
	T Top()
	{
		if (m_iUseSize < 0)
			return NULL;

		return m_pData[m_iUseSize];
	}
	int GetUseSize()
	{
		return m_iUseSize;
	}

};
