#pragma once
#include "LockFreeMemoryPool.h"

#define dfDEFAULTSIZE		1024
#define BUFFFER_MAX			10000

// Header 의 크기를 결정함
// 프로그램마다 유동적임
// 헤더 크기에 따라 수정할 것
// 또한 내부에서 반드시 
// WritePtrSetHeader() 와 WritePtrSetLast() 를 사용함으로써
// 헤더 시작 부분과 페이로드 마지막 부분으로 포인터를 옮겨줘야 함
// 혹은 GetLastWrite() 를 사용하여 사용자가 쓴 마지막 값을 사이즈로 넘겨줌
#define df_HEADER_SIZE		5

#define NUM_OF_CHUNK		10

//#define df_HEADER_CODE		(BYTE)0x11
// 헤더에 들어갈 암호화용 XOR 고정 코드
// 고정 코드 2개에 대하여 XOR 한 코드임
//#define df_XOR_CODE			(BYTE)0x41

#define df_RAND_CODE_LOCATION		 3
#define df_CHECKSUM_CODE_LOCATION	 4

class CNetServer;
class CLanClient;

class CNetServerSerializationBuf
{
private:
	BYTE		m_byError;
	BOOL		m_bIsEncoded;
	int			m_iWrite;
	int			m_iRead;
	int			m_iSize;
	// WritePtrSetHeader 함수로 부터 마지막에 쓴 공간을 기억함
	int			m_iWriteLast;
	int			m_iUserWriteBefore;
	UINT		m_iRefCount;
	char		*m_pSerializeBuffer;
	//static CLockFreeMemoryPool<CSerializationBuf>	*pMemoryPool;

	static BYTE												m_byHeaderCode;
	static BYTE												m_byXORCode;

	static CTLSMemoryPool<CNetServerSerializationBuf>		*pMemoryPool;

	friend CNetServer;
	friend CLanClient;
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
	
	// 원하는 길이만큼 읽기 위치에서 삭제
	void RemoveData(int Size);

	void Encode();
	bool Decode();

	// 서버 종료 할 때만 사용 할 것
	static void ChunkFreeForcibly();

	int GetAllUseSize();
public:
	CNetServerSerializationBuf();
	~CNetServerSerializationBuf();

	void Init();

	void WriteBuffer(char *pData, int Size);
	void ReadBuffer(char *pDest, int Size);
	void PeekBuffer(char *pDest, int Size);

	char *GetReadBufferPtr();
	char *GetWriteBufferPtr();

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


	// --------------- 반환값 ---------------
	// 0 : 정상처리 되었음
	// 1 : 버퍼를 읽으려고 하였으나, 읽는 공간이 버퍼 크기보다 크거나 아직 쓰여있지 않은 공간임
	// 2 : 버퍼를 쓰려고 하였으나, 쓰는 공간이 버퍼 크기보다 큼
	// --------------- 반환값 ---------------
	BYTE GetBufferError();

	int GetUseSize();
	int GetFreeSize();

	static CNetServerSerializationBuf*	Alloc();
	static void							AddRefCount(CNetServerSerializationBuf* AddRefBuf);
	static void							Free(CNetServerSerializationBuf* DeleteBuf);


	CNetServerSerializationBuf& operator<<(int Input);
	CNetServerSerializationBuf& operator<<(WORD Input);
	CNetServerSerializationBuf& operator<<(DWORD Input);
	CNetServerSerializationBuf& operator<<(char Input);
	CNetServerSerializationBuf& operator<<(BYTE Input);
	CNetServerSerializationBuf& operator<<(float Input);
	CNetServerSerializationBuf& operator<<(UINT Input);
	CNetServerSerializationBuf& operator<<(UINT64 Input);
	CNetServerSerializationBuf& operator<<(__int64 Input);

	CNetServerSerializationBuf& operator>>(int &Input);
	CNetServerSerializationBuf& operator>>(WORD &Input);
	CNetServerSerializationBuf& operator>>(DWORD &Input);
	CNetServerSerializationBuf& operator>>(char &Input);
	CNetServerSerializationBuf& operator>>(BYTE &Input);
	CNetServerSerializationBuf& operator>>(float &Input);
	CNetServerSerializationBuf& operator>>(UINT &Input);
	CNetServerSerializationBuf& operator>>(UINT64 &Input);
	CNetServerSerializationBuf& operator>>(__int64 &Input);
};

