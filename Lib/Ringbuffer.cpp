#include "PreCompile.h"
#include "Ringbuffer.h"

CRingbuffer::CRingbuffer() : m_iFront(0), m_iRear(0), m_iSize(DEFAULT_BUFFERMAX)
{
	Initialize(DEFAULT_BUFFERMAX);
	InitializeSRWLock(&m_SRWLock);
}

CRingbuffer::~CRingbuffer()
{
	delete[] m_pBuffer;
}

void CRingbuffer::Initialize(int BufferSize)
{
	m_pBuffer = new char[BufferSize];
}

void CRingbuffer::InitPointer()
{
	m_iFront = 0;
	m_iRear = 0;
}

void CRingbuffer::Resize(int Size)
{
	m_iFront = 0;
	m_iRear = 0;
	m_iSize = Size;

	delete[] m_pBuffer;
	Initialize(Size);
}

int CRingbuffer::GetUseSize()
{
	int f = m_iFront;
	int r = m_iRear;

	if (f < r)
	{
		return f + m_iSize - r;
	}
	else
	{
		return f - r;
	}
}

int CRingbuffer::GetFreeSize()
{
	int f = m_iFront;
	int r = m_iRear;

	if (f < r)
		return r - f - 1;
	else
		return r - 1 + m_iSize - f;
	//if (r == 0)
	//	return m_iSize - f - 1;
	//else
	//	return m_iSize - f;
}

int CRingbuffer::GetNotBrokenGetSize()
{
	// 스레드에서 직접 건드리는 값이 될 수 있으므로
	// 지역변수로 값을 넣어놓고 계산함
	int r = m_iRear;
	int f = m_iFront;

	if (f >= r)
	{
		return f - r;
	}
	else
	{
		return m_iSize - r;
	}
}

int CRingbuffer::GetNotBrokenPutSize()
{
	// 스레드에서 직접 건드리는 값이 될 수 있으므로
	// 지역변수로 값을 넣어놓고 계산함
	int r = m_iRear;
	int f = m_iFront;

	if (f < r)
	{
		return r - f - 1;
	}
	else
	{
		if (r == 0)
			return m_iSize - f - 1;
		else
			return m_iSize - f;
	}
}

int CRingbuffer::Enqueue(char *pData, int Size)
{
	//int RestSize = GetNotBrokenPutSize();
	// GetNotBrokenPutSize를 직접 여기에서 씀
	int RestSize;
	int curR = m_iRear;
	int curF = m_iFront;

	if (curF < curR)
	{
		RestSize = curR - curF - 1;
	}
	else
	{
		if (curR == 0)
			RestSize = m_iSize - curF - 1;
		else
			RestSize = m_iSize - curF;
	}

	char *Frontptr = &m_pBuffer[curF];
	if (RestSize > Size)
		RestSize = Size;

	if (curF + RestSize < m_iSize)
	{
		memcpy_s(Frontptr, RestSize, pData, RestSize);
		m_iFront += RestSize;
		return RestSize;
	}
	else
	{
		memcpy_s(Frontptr, RestSize, pData, RestSize);
		Frontptr = m_pBuffer;

		int rest = Size - RestSize;
		int r = curR - 1;

		if (r > rest)
		{
			memcpy_s(Frontptr, rest, pData + RestSize, rest);
			m_iFront = rest;
			return Size;
		}
		else
		{
			memcpy_s(Frontptr, r, pData + RestSize, r);
			m_iFront = r;
			return RestSize + r;
		}
	}
}

int CRingbuffer::Dequeue(char *pDest, int Size)
{
	//int UseSize = GetNotBrokenGetSize();
	// GetNotBrokenGetSize를 직접 여기에서 씀
	int UseSize;
	int curR = m_iRear;
	int curF = m_iFront;

	if (curF >= curR)
	{
		UseSize = curF - curR;
	}
	else
	{
		UseSize = m_iSize - curR;
	}

	char *Rearptr = &m_pBuffer[curR];
	if (UseSize > Size)
		UseSize = Size;

	if (curR + UseSize < m_iSize)
	{
		memcpy_s(pDest, UseSize, Rearptr, UseSize);
		m_iRear += UseSize;
		return UseSize;
	}
	else
	{
		memcpy_s(pDest, UseSize, Rearptr, UseSize);
		Rearptr = m_pBuffer;

		int rest = Size - UseSize;
		int f = curF;

		if (f > rest)
		{
			memcpy_s(pDest + UseSize, rest, Rearptr, rest);
			m_iRear = rest;
			return Size;
		}
		else
		{
			memcpy_s(pDest + UseSize, f, Rearptr, f);
			m_iRear = f;
			return UseSize + f;
		}
	}
}

int CRingbuffer::Peek(char *pDest, int Size)
{
	//int UseSize = GetNotBrokenGetSize();
	// GetNotBrokenGetSize를 직접 여기에서 씀
	int UseSize;
	int curR = m_iRear;
	int curF = m_iFront;

	if (curF >= curR)
	{
		UseSize = curF - curR;
	}
	else
	{
		UseSize = m_iSize - curR;
	}

	if (UseSize > Size)
		UseSize = Size;
	char *Rearptr = &m_pBuffer[curR];

	if (curR + UseSize < m_iSize)
	{
		memcpy_s(pDest, UseSize, Rearptr, UseSize);
		return UseSize;
	}
	else
	{
		memcpy_s(pDest, UseSize, Rearptr, UseSize);
		Rearptr = m_pBuffer;

		int rest = Size - UseSize;
		int f = curF;

		if (f > rest)
		{
			memcpy_s(pDest + UseSize, rest, Rearptr, rest);
			return Size;
		}
		else
		{
			memcpy_s(pDest + UseSize, f, Rearptr, f);
			return UseSize + f;
		}
	}
}

void CRingbuffer::RemoveData(int Size)
{
	int RemoveSize = Size + m_iRear;
	if (RemoveSize >= m_iSize)
	{
		m_iRear = RemoveSize - m_iSize;
	}
	else
	{
		m_iRear = RemoveSize;
	}
}

void CRingbuffer::MoveWritePos(int Size)
{
	int MovePos = Size + m_iFront;
	if (MovePos >= m_iSize)
	{
		m_iFront = MovePos - m_iSize;
	}
	else
	{
		m_iFront = MovePos;
	}
}

void CRingbuffer::ClearBuffer()
{
	m_iFront = 0;
	m_iRear = 0;
}

char *CRingbuffer::GetBufferPtr()
{
	return m_pBuffer;
}

char *CRingbuffer::GetReadBufferPtr()
{
	return &m_pBuffer[m_iRear];
}

char *CRingbuffer::GetWriteBufferPtr()
{
	return &m_pBuffer[m_iFront];
}

bool CRingbuffer::IsFull()
{
	if (m_iFront + 1 == m_iRear || (m_iFront + 1 == m_iSize && m_iRear == 0))
		return true;
	return false;
}

bool CRingbuffer::IsEmpty()
{
	if (m_iFront == m_iRear)
		return true;
	else
		return false;
}



int CRingbuffer::LockEnqueue(char *pData, int Size)
{
	//int RestSize = GetNotBrokenPutSize();
	// GetNotBrokenPutSize를 직접 여기에서 씀
	int RestSize;
	AcquireSRWLockExclusive(&m_SRWLock);
	int curR = m_iRear;
	int curF = m_iFront;

	if (curF < curR)
	{
		RestSize = curR - curF - 1;
	}
	else
	{
		if (curR == 0)
			RestSize = m_iSize - curF - 1;
		else
			RestSize = m_iSize - curF;
	}

	char *Frontptr = &m_pBuffer[curF];
	if (RestSize > Size)
		RestSize = Size;

	if (curF + RestSize < m_iSize)
	{
		memcpy_s(Frontptr, RestSize, pData, RestSize);
		m_iFront += RestSize;
		ReleaseSRWLockExclusive(&m_SRWLock);
		return RestSize;
	}
	else
	{
		memcpy_s(Frontptr, RestSize, pData, RestSize);
		Frontptr = m_pBuffer;

		int rest = Size - RestSize;
		int r = curR - 1;

		if (r > rest)
		{
			memcpy_s(Frontptr, rest, pData + RestSize, rest);
			m_iFront = rest;
			ReleaseSRWLockExclusive(&m_SRWLock);
			return Size;
		}
		else
		{
			memcpy_s(Frontptr, r, pData + RestSize, r);
			m_iFront = r;
			ReleaseSRWLockExclusive(&m_SRWLock);
			return RestSize + r;
		}
	}
}

int CRingbuffer::LockDequeue(char *pDest, int Size)
{
	//int UseSize = GetNotBrokenGetSize();
	// GetNotBrokenGetSize를 직접 여기에서 씀
	int UseSize;
	AcquireSRWLockExclusive(&m_SRWLock);
	int curR = m_iRear;
	int curF = m_iFront;

	if (curF >= curR)
	{
		UseSize = curF - curR;
	}
	else
	{
		UseSize = m_iSize - curR;
	}

	char *Rearptr = &m_pBuffer[curR];
	if (UseSize > Size)
		UseSize = Size;

	if (curR + UseSize < m_iSize)
	{
		memcpy_s(pDest, UseSize, Rearptr, UseSize);
		m_iRear += UseSize;
		ReleaseSRWLockExclusive(&m_SRWLock);
		return UseSize;
	}
	else
	{
		memcpy_s(pDest, UseSize, Rearptr, UseSize);
		Rearptr = m_pBuffer;

		int rest = Size - UseSize;
		int f = curF;

		if (f > rest)
		{
			memcpy_s(pDest + UseSize, rest, Rearptr, rest);
			m_iRear = rest;
			ReleaseSRWLockExclusive(&m_SRWLock);
			return Size;
		}
		else
		{
			memcpy_s(pDest + UseSize, f, Rearptr, f);
			m_iRear = f;
			ReleaseSRWLockExclusive(&m_SRWLock);
			return UseSize + f;
		}
	}
}

void CRingbuffer::LockMoveWritePos(int Size)
{
	AcquireSRWLockExclusive(&m_SRWLock);
	int MovePos = Size + m_iFront;
	if (MovePos >= m_iSize)
	{
		m_iFront = 0;
	}
	else
	{
		m_iFront = MovePos;
	}
	ReleaseSRWLockExclusive(&m_SRWLock);
}

void CRingbuffer::QueueLock(bool IsShared)
{
	if (IsShared)
		AcquireSRWLockShared(&m_SRWLock);
	else
		AcquireSRWLockExclusive(&m_SRWLock);
}

void CRingbuffer::QueueUnlock(bool IsShared)
{
	if (IsShared)
		ReleaseSRWLockShared(&m_SRWLock);
	else
		ReleaseSRWLockExclusive(&m_SRWLock);
}