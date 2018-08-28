#pragma once
#include <Windows.h>

#define DEFAULT_BUFFERMAX	8192

class CRingbuffer
{
private:
	int m_iFront;
	int m_iRear;
	int m_iSize;
	char *m_pBuffer;
	SRWLOCK m_SRWLock;
	//CRITICAL_SECTION m_CriticalSection;

private:
	void Initialize(int  BufferSize);

public:
	CRingbuffer();
	~CRingbuffer();

	// 바깥에서 rear 와 front 만 0으로 변경함
	void InitPointer();

	// 이전 데이터를 삭제하고 새로 버퍼크기를 할당함
	void Resize(int Size);
	// 현재 사용중인 용량
	int GetUseSize();
	// 현재 남아있는 용량
	int GetFreeSize();

	// 버퍼 포인터로 외부에서 한 번에 읽을 수 있는 길이
	int GetNotBrokenGetSize();
	// 버퍼 포인터로 외부에서 한 번에 쓸 수 있는 길이
	int GetNotBrokenPutSize();

	int Enqueue(char *pData, int Size);
	int Dequeue(char *pDest, int Size);
	int Peek(char *pDest, int Size);

	// 원하는 길이만큼 읽기 위치에서 삭제
	void RemoveData(int Size);
	// 원하는 길이만큼 읽기 쓰기 위치 이동
	void MoveWritePos(int Size);

	void ClearBuffer();

	char *GetBufferPtr();
	char *GetReadBufferPtr();
	char *GetWriteBufferPtr();

	bool IsEmpty();
	bool IsFull();

	//---------------- SRWLock 관련 함수 ----------------//

	// SRWLockExclusive 로 Enqueue 작업을 시행함
	// 작업이 끝나면 Lock을 해제함
	int LockEnqueue(char *pData, int Size);
	int LockDequeue(char *pDest, int Size);

	// SRWLockExclusive 로 MoveWritePos 작업을 시행함
	// 작업이 끝나면 Lock을 해제함
	void LockMoveWritePos(int Size);
	//void LockRemoveData(int Size);

	// 외부에서 해당 큐를 bool 값을 이용하여 Shared / Exclusive 모드로 Lock 함
	// Lock 관련 함수들에게만 영향이 있음
	// 그런데 이게 효용성이 있을지는 모르겠음
	void QueueLock(bool IsShared);
	// 외부에서 해당 큐를 bool 값을 이용하여 Shared / Exclusive 모드로 Lock을 해제함
	// Lock 관련 함수들에게만 영향이 있음
	// 그런데 이게 효용성이 있을지는 모르겠음
	void QueueUnlock(bool IsShared);

	//---------------- SRWLock 관련 함수 ----------------//
};