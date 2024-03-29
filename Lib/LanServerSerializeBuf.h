#pragma once
//#include "LanServer.h"
#include "LockFreeMemoryPool.h"
#include <Windows.h>
#include <list>

#define dfLAN_DEFAULTSIZE	512
#define BUFFFER_MAX			10000

// Header 의 크기를 결정함
// 내부에서 반드시 
// WritePtrSetHeader() 와 WritePtrSetLast() 를 사용함으로써
// 헤더 시작 부분과 페이로드 마지막 부분으로 포인터를 옮겨줘야 함
// 혹은 GetLastWrite() 를 사용하여 사용자가 쓴 마지막 값을 사이즈로 넘겨줌
#define HEADER_SIZE			2

#define NUM_OF_LANBUF_CHUNK		5

class CLanServer;
class CLanClient;
class CMultiLanClient;

class CSerializationBuf
{
private:
	BYTE		m_byDelCnt = 0;

	BYTE		m_byError;
	bool		m_bHeaderInputted;
	WORD		m_iWrite;
	WORD		m_iRead;
	WORD		m_iSize;
	// WritePtrSetHeader 함수로 부터 마지막에 쓴 공간을 기억함
	WORD		m_iWriteLast;
	WORD		m_iUserWriteBefore;
	UINT		m_iRefCount;
	char		*m_pSerializeBuffer;
	//static CLockFreeMemoryPool<CSerializationBuf>	*pMemoryPool;
	static CTLSMemoryPool<CSerializationBuf>		*pMemoryPool;

	friend CLanServer;
	friend CLanClient;
	friend CMultiLanClient;
private:
	void Initialize(int BufferSize);

	void SetWriteZero();
	// 쓰기 포인터의 마지막 공간을 변수에 기억하고
	// 쓰기 포인터를 직렬화 버퍼 가장 처음으로 옮김
	void WritePtrSetHeader();
	// 쓰기 포인터를 이전에 사용했던 공간의 마지막 부분으로 이동시킴
	void WritePtrSetLast();

	// 버퍼의 처음부터 사용자가
	// 마지막으로 쓴 공간까지의 차를 구함
	int GetLastWrite();

	char *GetBufferPtr();

	void PeekBuffer(char *pDest, int Size);

	// 원하는 길이만큼 읽기 위치에서 삭제
	void RemoveData(int Size); 

	// 서버 종료 할 때만 사용 할 것
	static void ChunkFreeForcibly();

	int GetAllUseSize();
	FORCEINLINE void CheckReadBufferSize(int needSize);
	FORCEINLINE void CheckWriteBufferSize(int needSize);
public:
	CSerializationBuf();
	~CSerializationBuf();

	void Init();

	// 새로 버퍼크기를 할당하여 이전 데이터를 옮김
	void Resize(int Size);

	// 원하는 길이만큼 읽기 쓰기 위치 이동
	void MoveWritePos(int Size);
	// ThisPos로 쓰기 포인터를 이동시킴
	void MoveWritePosThisPos(int ThisPos);
	// MoveWritePosThisPos 로 이동하기 이전 쓰기 포인터로 이동시킴
	// 위 함수를 사용하지 않고 이 함수를 호출할 경우
	// 쓰기 포인터를 가장 처음으로 이동시킴
	void MoveWritePosBeforeCallThisPos();

	char *GetReadBufferPtr();
	char* GetWriteBufferPtr();

	void WriteBuffer(char *pData, int Size);
	void WriteBuffer(const std::string& dest);
	void WriteBuffer(const std::wstring& dest);
	void ReadBuffer(char* pDest, int Size);
	void ReadBuffer(OUT std::string& dest);
	void ReadBuffer(OUT std::wstring& dest);

	// --------------- 반환값 ---------------
	// 0 : 정상처리 되었음
	// 1 : 버퍼를 읽으려고 하였으나, 읽는 공간이 버퍼 크기보다 크거나 아직 쓰여있지 않은 공간임
	// 2 : 버퍼를 쓰려고 하였으나, 쓰는 공간이 버퍼 크기보다 큼
	// --------------- 반환값 ---------------
	BYTE GetBufferError();

	int GetUseSize();
	int GetFreeSize();

	static CSerializationBuf* Alloc();
	static void				  AddRefCount(CSerializationBuf* AddRefBuf);
	static void				  Free(CSerializationBuf* DeleteBuf);
	static int				  GetUsingSerializeBufNodeCount();
	static int				  GetUsingSerializeBufChunkCount();
	static int				  GetAllocSerializeBufChunkCount();

	CSerializationBuf& operator<<(int Input);
	CSerializationBuf& operator<<(WORD Input);
	CSerializationBuf& operator<<(DWORD Input);
	CSerializationBuf& operator<<(char Input);
	CSerializationBuf& operator<<(BYTE Input);
	CSerializationBuf& operator<<(float Input);
	CSerializationBuf& operator<<(UINT Input);
	CSerializationBuf& operator<<(UINT64 Input);
	CSerializationBuf& operator<<(__int64 Input);
	CSerializationBuf& operator<<(std::list<std::string>& input);
	CSerializationBuf& operator<<(std::list<std::wstring>& input);
	template<typename T>
	CSerializationBuf& operator<<(T& input)
	{
		WriteBuffer((char*)&input, sizeof(T));
		return *this;
	}
	template<typename T>
	CSerializationBuf& operator<<(std::list<T>& input)
	{
		size_t listSize = input.size();
		WriteBuffer((char*)&listSize, sizeof(listSize));

		for (auto& item : input)
		{
			WriteBuffer((char*)&item, sizeof(T));
		}

		return *this;
	}

	CSerializationBuf& operator>>(int &Input);
	CSerializationBuf& operator>>(WORD &Input);
	CSerializationBuf& operator>>(DWORD &Input);
	CSerializationBuf& operator>>(char &Input);
	CSerializationBuf& operator>>(BYTE &Input);
	CSerializationBuf& operator>>(float &Input);
	CSerializationBuf& operator>>(UINT &Input);
	CSerializationBuf& operator>>(UINT64 &Input);
	CSerializationBuf& operator>>(__int64 &Input);
	template<typename T>
	CSerializationBuf& operator>>(T& input)
	{
		ReadBuffer((char*)&input, sizeof(input));
		return *this;
	}
	template<typename T>
	CSerializationBuf& operator>>(std::list<T>& input)
	{
		size_t listSize = input.size();
		ReadBuffer((char*)&listSize, sizeof(listSize));

		for (size_t i = 0; i < listSize; ++i)
		{
			T item;
			ReadBuffer((char*)&item, sizeof(T));
			input.push_back(std::move(item));
		}

		return *this;
	}
	CSerializationBuf& operator>>(std::list<std::string>& input);
	CSerializationBuf& operator>>(std::list<std::wstring>& input);
};

