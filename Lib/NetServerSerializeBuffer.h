#pragma once
#include "LockFreeMemoryPool.h"
#include <list>

#define dfDEFAULTSIZE		1024
#define BUFFFER_MAX			10000

// Header 의 크기를 결정함
// 프로그램마다 유동적임
// ?E갋크기에 따턿E수정할 것
// 또한 내부에서 반드시 
// WritePtrSetHeader() 와 WritePtrSetLast() 를 사퓖E纛막館갋
// ?E갋시작 부분컖E페이로탛E마지막 부분으로 포인터를 옮겨줘야 함
// 혹은 GetLastWrite() 를 사퓖E臼?사퓖E微?쓴 마지막 값을 사이짊踏 넘겨줌
#define df_HEADER_SIZE		5

#define dfNUM_OF_NETBUF_CHUNK		10

#define df_RAND_CODE_LOCATION		 3
#define df_CHECKSUM_CODE_LOCATION	 4

class CNetServer;
class CNetClient;
class CMMOServer;
class CP2P;
class CSession;
class RIOTestServer;
class RIOTestSession;
class IOCPServer;

class CNetServerSerializationBuf
{
private:
	BYTE		m_byError;
	bool		m_bIsEncoded;
	WORD		m_iWrite;
	WORD		m_iRead;
	WORD		m_iSize;
	// WritePtrSetHeader 함수로 부터 마지막에 쓴 공간을 기푳E?
	WORD		m_iWriteLast;
	WORD		m_iUserWriteBefore;
	UINT		m_iRefCount;
	char		*m_pSerializeBuffer;
	//static CLockFreeMemoryPool<CSerializationBuf>	*pMemoryPool;

	static CTLSMemoryPool<CNetServerSerializationBuf>		*pMemoryPool;
	static BYTE												m_byHeaderCode;
	static BYTE												m_byXORCode;

	friend CNetServer;
	friend CNetClient;
	friend CMMOServer;
	friend CSession;
	friend RIOTestServer;
	friend RIOTestSession;
	friend IOCPServer;
private:
	void Initialize(int BufferSize);

	void SetWriteZero();
	// 쓰콅E포인터의 마지막 공간을 변수에 기푳E構갋
	// 쓰콅E포인터를 직렬화 버퍼 가픸E처음으로 옮콅E
	void WritePtrSetHeader();
	// 쓰콅E포인터를 이픸E?사퓖E杉갋공간의 마지막 부분으로 이동시킴
	void WritePtrSetLast();

	// 버퍼의 처음부터 사퓖E微?
	// 마지막으로 쓴 공간깩痺의 차를 구함
	int GetLastWrite();

	char *GetBufferPtr();
	
	// 원하는 길이만큼 읽콅E위치에서 삭제
	void RemoveData(int Size);


	// 서퉩E종톩E할 때만 사퓖E할 것
	static void ChunkFreeForcibly();

	int GetAllUseSize();
	FORCEINLINE void CheckReadBufferSize(int needSize);
	FORCEINLINE void CheckWriteBufferSize(int needSize);
public:
	void Encode();
	bool Decode();
	CNetServerSerializationBuf();
	~CNetServerSerializationBuf();

	void Init();

	void WriteBuffer(char *pData, int Size);
	void WriteBuffer(const std::string& dest);
	void WriteBuffer(const std::wstring& dest);
	void ReadBuffer(char *pDest, int Size);
	void ReadBuffer(OUT std::string& dest);
	void ReadBuffer(OUT std::wstring& dest);
	void PeekBuffer(char *pDest, int Size);

	char *GetReadBufferPtr();
	char *GetWriteBufferPtr();

	// 새로 버퍼크기를 할당하여 이픸E데이터를 옮콅E
	void Resize(int Size);

	// 원하는 길이만큼 읽콅E쓰콅E위치 이동
	void MoveWritePos(int Size);
	// ThisPos로 쓰콅E포인터를 이동시킴
	void MoveWritePosThisPos(int ThisPos);
	// MoveWritePosThisPos 로 이동하콅E이픸E쓰콅E포인터로 이동시킴
	// 위 함수를 사퓖E舊갋않컖E이 함수를 호출할 경퓖E
	// 쓰콅E포인터를 가픸E처음으로 이동시킴
	void MoveWritePosBeforeCallThisPos();


	// --------------- 반환값 ---------------
	// 0 : 정상처리 되었음
	// 1 : 버퍼를 읽으려컖E하였으나, 읽는 공간이 버퍼 크기보다 크거나 아햨E쓰여있햨E않은 공간임
	// 2 : 버퍼를 쓰려컖E하였으나, 쓰는 공간이 버퍼 크기보다 큼
	// --------------- 반환값 ---------------
	BYTE GetBufferError();

	int GetUseSize();
	int GetFreeSize();

	static CNetServerSerializationBuf*	Alloc();
	static void							AddRefCount(CNetServerSerializationBuf* AddRefBuf);
	//static void							AddRefCount(CNetServerSerializationBuf* AddRefBuf, int AddNumber);
	static void							Free(CNetServerSerializationBuf* DeleteBuf);
	static int							GetUsingSerializeBufNodeCount();
	static int							GetUsingSerializeBufChunkCount();
	static int							GetAllocSerializeBufChunkCount();

	CNetServerSerializationBuf& operator<<(int Input);
	CNetServerSerializationBuf& operator<<(WORD Input);
	CNetServerSerializationBuf& operator<<(DWORD Input);
	CNetServerSerializationBuf& operator<<(char Input);
	CNetServerSerializationBuf& operator<<(BYTE Input);
	CNetServerSerializationBuf& operator<<(float Input);
	CNetServerSerializationBuf& operator<<(UINT Input);
	CNetServerSerializationBuf& operator<<(UINT64 Input);
	CNetServerSerializationBuf& operator<<(__int64 Input);
	CNetServerSerializationBuf& operator<<(std::string& input);
	CNetServerSerializationBuf& operator<<(std::wstring& input);
	template<typename T>
	CNetServerSerializationBuf& operator<<(T& input)
	{
		WriteBuffer((char*)&input, sizeof(T));
		return *this;
	}
	template<typename T>
	CNetServerSerializationBuf& operator<<(std::list<T>& input)
	{
		size_t listSize = input.size();
		WriteBuffer((char*)&listSize, sizeof(listSize));

		for (auto& item : input)
		{
			WriteBuffer((char*)&item, sizeof(T));
		}

		return *this;
	}
	CNetServerSerializationBuf& operator<<(std::list<std::string>& input);
	CNetServerSerializationBuf& operator<<(std::list<std::wstring>& input);

	CNetServerSerializationBuf& operator>>(int &Input);
	CNetServerSerializationBuf& operator>>(WORD &Input);
	CNetServerSerializationBuf& operator>>(DWORD &Input);
	CNetServerSerializationBuf& operator>>(char &Input);
	CNetServerSerializationBuf& operator>>(BYTE &Input);
	CNetServerSerializationBuf& operator>>(float &Input);
	CNetServerSerializationBuf& operator>>(UINT &Input);
	CNetServerSerializationBuf& operator>>(UINT64 &Input);
	CNetServerSerializationBuf& operator>>(__int64 &Input);
	CNetServerSerializationBuf& operator>>(std::string& input);
	CNetServerSerializationBuf& operator>>(std::wstring& input);
	template<typename T>
	CNetServerSerializationBuf& operator>>(T& input)
	{
		ReadBuffer((char*)&input, sizeof(input));
		return *this;
	}
	template<typename T>
	CNetServerSerializationBuf& operator>>(std::list<T>& input)
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
	CNetServerSerializationBuf& operator>>(std::list<std::string>& input);
	CNetServerSerializationBuf& operator>>(std::list<std::wstring>& input);
};

using NetBuffer = CNetServerSerializationBuf;
