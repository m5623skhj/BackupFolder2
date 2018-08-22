#pragma once
#include <Windows.h>

#define dfDEFAULTSIZE		2000
#define BUFFFER_MAX			10000

// Header 의 크기를 결정함
// 프로그램마다 유동적임
// 헤더 크기에 따라 수정할 것
// 또한 내부에서 반드시 
// WritePtrSetHeader() 와 WritePtrSetLast() 를 사용함으로써
// 헤더 시작 부분과 페이로드 마지막 부분으로 포인터를 옮겨줘야 함
// 혹은 GetLastWrite() 를 사용하여 사용자가 쓴 마지막 값을 사이즈로 넘겨줌
#define HEADER_SIZE			2

class CSerializationBuffer
{
private:
	BYTE m_byError;
	int m_iWrite;
	int m_iRead;
	int m_iSize;
	// WritePtrSetHeader 함수로 부터 마지막에 쓴 공간을 기억함
	int m_iWriteLast;
	char *m_pSerializeBuffer;
private:
	void Initialize(int BufferSize);
public:
	CSerializationBuffer();
	~CSerializationBuffer();

	// 새로 버퍼크기를 할당하여 이전 데이터를 옮김
	void Resize(int Size);

	void WriteBuffer(char *pData, int Size);
	void ReadBuffer(char *pDest, int Size);
	void PeekBuffer(char *pDest, int Size);

	// 원하는 길이만큼 읽기 위치에서 삭제
	void RemoveData(int Size);
	// 원하는 길이만큼 읽기 쓰기 위치 이동
	void MoveWritePos(int Size);
	// 원하는 곳으로 읽기와 쓰기 포인터를 이동
	void MoveWriteAndReadPos(int Pos);
	// ThisPos로 쓰기 포인터를 이동시킴
	void MoveWritePosThisPos(int ThisPos);

	// 페이로드 부터 만들기 위해 헤더 사이즈만큼 쓰기 포인터를 이동시킴
	void WritePtrSetPayload(int HeaderSize);
	// 쓰기 포인터의 마지막 공간을 변수에 기억하고
	// 쓰기 포인터를 직렬화 버퍼 가장 처음으로 옮김
	void WritePtrSetHeader();
	// 쓰기 포인터를 이전에 사용했던 공간의 마지막 부분으로 이동시킴
	void WritePtrSetLast();


	// --------------- 반환값 ---------------
	// 0 : 정상처리 되었음
	// 1 : 버퍼를 읽으려고 하였으나, 읽는 공간이 버퍼 크기보다 크거나 아직 쓰여있지 않은 공간임
	// 2 : 버퍼를 쓰려고 하였으나, 쓰는 공간이 버퍼 크기보다 큼
	// --------------- 반환값 ---------------
	BYTE GetBufferError();

	char *GetBufferPtr();
	char *GetReadBufferPtr();
	char *GetWriteBufferPtr();
	int GetUseSize();
	// 버퍼의 처음부터 사용자가 마지막으로 쓴 공간까지의 
	// 차를 구함
	int GetLastWrite();
	int GetFreeSize();

	CSerializationBuffer& operator<<(int Input);
	CSerializationBuffer& operator<<(WORD Input);
	CSerializationBuffer& operator<<(DWORD Input);
	CSerializationBuffer& operator<<(char Input);
	CSerializationBuffer& operator<<(BYTE Input);
	CSerializationBuffer& operator<<(float Input);
	CSerializationBuffer& operator<<(UINT Input);
	CSerializationBuffer& operator<<(UINT64 Input);
	CSerializationBuffer& operator<<(__int64 Input);

	CSerializationBuffer& operator>>(int &Input);
	CSerializationBuffer& operator>>(WORD &Input);
	CSerializationBuffer& operator>>(DWORD &Input);
	CSerializationBuffer& operator>>(char &Input);
	CSerializationBuffer& operator>>(BYTE &Input);
	CSerializationBuffer& operator>>(float &Input);
	CSerializationBuffer& operator>>(UINT &Input);
	CSerializationBuffer& operator>>(UINT64 &Input);
	CSerializationBuffer& operator>>(__int64 &Input);
};