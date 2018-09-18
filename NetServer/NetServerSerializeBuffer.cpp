#include "PreCompile.h"
#include "NetServerSerializeBuffer.h"
#include "NetServer.h"
#include "LockFreeMemoryPool.h"

CTLSMemoryPool<CNetServerSerializationBuf>* CNetServerSerializationBuf::pMemoryPool = new CTLSMemoryPool<CNetServerSerializationBuf>(NUM_OF_CHUNK, false);
extern CParser g_Paser;

BYTE CNetServerSerializationBuf::m_byHeaderCode = 0;
BYTE CNetServerSerializationBuf::m_byXORCode = 0;

struct st_Exception
{
	WCHAR ErrorCode[32];
	WCHAR szErrorComment[1024];
};

CNetServerSerializationBuf::CNetServerSerializationBuf() :
	m_byError(0), m_bIsEncoded(FALSE), m_iWrite(df_HEADER_SIZE), m_iRead(df_HEADER_SIZE), m_iWriteLast(0),
	m_iUserWriteBefore(df_HEADER_SIZE), m_iRefCount(1)
{
	Initialize(dfDEFAULTSIZE);
}

CNetServerSerializationBuf::~CNetServerSerializationBuf()
{
	delete[] m_pSerializeBuffer;
}

void CNetServerSerializationBuf::Initialize(int BufferSize)
{
	m_iSize = BufferSize;
	m_pSerializeBuffer = new char[BufferSize];
}

void CNetServerSerializationBuf::Init()
{
	m_byError = 0;
	m_iWrite = df_HEADER_SIZE;
	m_iRead = df_HEADER_SIZE;
	m_iWriteLast = 0;
	m_iUserWriteBefore = df_HEADER_SIZE;
	m_iRefCount = 1;
}

void CNetServerSerializationBuf::Resize(int Size)
{
	if (BUFFFER_MAX < Size)
	{
		m_byError = 3;
		st_Exception e;
		wcscpy_s(e.ErrorCode, L"ErrorCode : 3");
		wsprintf(e.szErrorComment,
			L"%s Line %d\n\n버퍼의 크기를 재할당하려고 하였으나, 버퍼 최대 할당가능공간(10000byte) 보다 더 큰 값을 재할당하려고 하였습니다.\nWrite = %d Read = %d BufferSize = %d InputSize = %d\n\n프로그램을 종료합니다"
			, TEXT(__FILE__), __LINE__, m_iWrite, m_iRead, m_iSize, Size);
		throw e;
	}

	char *pBuffer = new char[Size];
	memcpy_s(pBuffer, m_iWrite, m_pSerializeBuffer, m_iWrite);
	delete[] m_pSerializeBuffer;

	m_pSerializeBuffer = pBuffer;
	m_iSize = Size;
}

void CNetServerSerializationBuf::WriteBuffer(char *pData, int Size)
{
	if (BUFFFER_MAX < m_iWrite + Size)
	{
		m_byError = 2;
		st_Exception e;
		wcscpy_s(e.ErrorCode, L"ErrorCode : 2");
		wsprintf(e.szErrorComment,
			L"%s Line %d\n\n버퍼에 쓰려고 하였으나, 버퍼 공간보다 더 큰 값이 들어왔습니다.\nWrite = %d Read = %d BufferSize = %d InputSize = %d\n\n프로그램을 종료합니다"
			, TEXT(__FILE__), __LINE__, m_iWrite, m_iRead, m_iSize, Size);
		throw e;
	}

	switch (Size)
	{
	case 1:
		*((char*)(&m_pSerializeBuffer[m_iWrite])) = *(char*)pData;
		break;
	case 2:
		*((short*)(&m_pSerializeBuffer[m_iWrite])) = *(short*)pData;
		break;
	case 4:
		*((int*)(&m_pSerializeBuffer[m_iWrite])) = *(int*)pData;
		break;
	case 8:
		*((long long*)(&m_pSerializeBuffer[m_iWrite])) = *(long long*)pData;
		break;
	default:
		memcpy_s(&m_pSerializeBuffer[m_iWrite], Size, pData, Size);
		break;
	}
	m_iWrite += Size;
}

void CNetServerSerializationBuf::ReadBuffer(char *pDest, int Size)
{
	if (m_iSize < m_iRead + Size || m_iWrite < m_iRead + Size)
	{
		m_byError = 1;
		st_Exception e;
		wcscpy_s(e.ErrorCode, L"ErrorCode : 1");
		wsprintf(e.szErrorComment,
			L"%s Line %d\n\n버퍼를 읽으려고 하였으나, 읽으려고 했던 공간이 버퍼 크기보다 크거나 아직 쓰여있지 않은 공간입니다.\nWrite = %d Read = %d BufferSize = %d InputSize = %d\n\n프로그램을 종료합니다"
			, TEXT(__FILE__), __LINE__, m_iWrite, m_iRead, m_iSize, Size);
		throw e;
	}

	switch (Size)
	{
	case 1:
		*((char*)(pDest)) = *(char*)&m_pSerializeBuffer[m_iRead];
		break;
	case 2:
		*((short*)(pDest)) = *(short*)&m_pSerializeBuffer[m_iRead];
		break;
	case 4:
		*((int*)(pDest)) = *(int*)&m_pSerializeBuffer[m_iRead];
		break;
	case 8:
		*((long long*)(pDest)) = *(long long*)&m_pSerializeBuffer[m_iRead];
		break;
	default:
		memcpy_s(pDest, Size, &m_pSerializeBuffer[m_iRead], Size);
		break;
	}
	m_iRead += Size;
}

void CNetServerSerializationBuf::PeekBuffer(char *pDest, int Size)
{
	memcpy_s(pDest, Size, &m_pSerializeBuffer[m_iRead], Size);
}

void CNetServerSerializationBuf::RemoveData(int Size)
{
	if (m_iSize < m_iRead + Size || m_iWrite < m_iRead + Size)
	{
		m_byError = 1;
		st_Exception e;
		wcscpy_s(e.ErrorCode, L"ErrorCode : 1");
		wsprintf(e.szErrorComment,
			L"%s Line %d\n\n버퍼를 읽으려고 하였으나, 읽으려고 했던 공간이 버퍼 크기보다 크거나 아직 쓰여있지 않은 공간입니다.\nWrite = %d Read = %d BufferSize = %d InputSize = %d\n\n프로그램을 종료합니다"
			, TEXT(__FILE__), __LINE__, m_iWrite, m_iRead, m_iSize, Size);
		throw e;
	}
	m_iRead += Size;
}

void CNetServerSerializationBuf::MoveWritePos(int Size)
{
	if (m_iSize < m_iWrite + Size + df_HEADER_SIZE)
	{
		m_byError = 2;
		st_Exception e;
		wcscpy_s(e.ErrorCode, L"ErrorCode : 2");
		wsprintf(e.szErrorComment,
			L"%s Line %d\n\n버퍼에 쓰려고 하였으나, 버퍼 공간보다 더 큰 값이 들어왔습니다.\nWrite = %d Read = %d BufferSize = %d InputSize = %d\n\n프로그램을 종료합니다"
			, TEXT(__FILE__), __LINE__, m_iWrite, m_iRead, m_iSize, Size);
		throw e;
	}
	m_iWrite += Size;
}

void CNetServerSerializationBuf::MoveWritePosThisPos(int ThisPos)
{
	m_iUserWriteBefore = m_iWrite;
	m_iWrite = df_HEADER_SIZE + ThisPos;
}

void CNetServerSerializationBuf::MoveWritePosBeforeCallThisPos()
{
	m_iWrite = m_iUserWriteBefore;
}

BYTE CNetServerSerializationBuf::GetBufferError()
{
	return m_byError;
}

char* CNetServerSerializationBuf::GetBufferPtr()
{
	return m_pSerializeBuffer;
}

char* CNetServerSerializationBuf::GetReadBufferPtr()
{
	return &m_pSerializeBuffer[m_iRead];
}

char* CNetServerSerializationBuf::GetWriteBufferPtr()
{
	return &m_pSerializeBuffer[m_iWrite];
}

int CNetServerSerializationBuf::GetUseSize()
{
	return m_iWrite - m_iRead;// - df_HEADER_SIZE;
}

int CNetServerSerializationBuf::GetFreeSize()
{
	return m_iSize - m_iWrite;
}

int CNetServerSerializationBuf::GetAllUseSize()
{
	return m_iWriteLast - m_iRead;
}

void CNetServerSerializationBuf::SetWriteZero()
{
	m_iWrite = 0;
}

int CNetServerSerializationBuf::GetLastWrite()
{
	return m_iWriteLast;
}

void CNetServerSerializationBuf::WritePtrSetHeader()
{
	m_iWriteLast = m_iWrite;
	m_iWrite = 0;
}

void CNetServerSerializationBuf::WritePtrSetLast()
{
	m_iWrite = m_iWriteLast;
}

// 헤더 순서
// Code(1) Length(2) Random XOR Code(1) CheckSum(1)
void CNetServerSerializationBuf::Encode()
{
	// 헤더 코드 삽입
	int NowWrite = 0;
	char *(&pThisBuffer) = m_pSerializeBuffer;
	pThisBuffer[NowWrite] = m_byHeaderCode;
	++NowWrite;

	// 페이로드 크기 삽입
	int WirteLast = m_iWriteLast;
	int Read = m_iRead;
	short PayloadLength = WirteLast - df_HEADER_SIZE;
	*((short*)(&pThisBuffer[NowWrite])) = *(short*)&PayloadLength;
	NowWrite += 2;

	// 난수 XOR 코드 생성
	BYTE RandCode = (BYTE)((rand() & 255) ^ m_byXORCode);

	// 체크섬 생성 및 페이로드와 체크섬 암호화
	WORD PayloadSum = 0;
	for (int BufIdx = df_HEADER_SIZE; BufIdx < WirteLast; ++BufIdx)
	{
		PayloadSum += pThisBuffer[BufIdx];
		pThisBuffer[BufIdx] ^= RandCode;
	}
	BYTE CheckSum = (BYTE)(PayloadSum & 255) ^ RandCode;

	// 암호화 된 랜덤코드와 체크섬 삽입
	pThisBuffer[df_RAND_CODE_LOCATION] = RandCode;
	pThisBuffer[df_CHECKSUM_CODE_LOCATION] = CheckSum;

	m_iWrite = df_HEADER_SIZE;
	m_bIsEncoded = TRUE;
}

// 헤더 순서
// Code(1) Length(2) Random XOR Code(1) CheckSum(1)
bool CNetServerSerializationBuf::Decode()
{
	char *(&pThisBuffer) = m_pSerializeBuffer;
	WORD PayloadSum = 0;
	int Write = m_iWrite;
	BYTE RandCode = pThisBuffer[df_RAND_CODE_LOCATION];

	for (int BufIdx = df_HEADER_SIZE; BufIdx < Write; ++BufIdx)
	{
		pThisBuffer[BufIdx] ^= RandCode;
		PayloadSum += pThisBuffer[BufIdx];
	}

	if (((BYTE)pThisBuffer[df_CHECKSUM_CODE_LOCATION] ^ RandCode) != (PayloadSum & 255))
		return false;

	m_iRead = df_HEADER_SIZE;
	return true;
}

//////////////////////////////////////////////////////////////////
// static
//////////////////////////////////////////////////////////////////

CNetServerSerializationBuf* CNetServerSerializationBuf::Alloc()
{
	CNetServerSerializationBuf *pBuf = pMemoryPool->Alloc();
	return pBuf;
}

void CNetServerSerializationBuf::AddRefCount(CNetServerSerializationBuf* AddRefBuf)
{
	InterlockedIncrement(&AddRefBuf->m_iRefCount);
}

void CNetServerSerializationBuf::Free(CNetServerSerializationBuf* DeleteBuf)
{
	if (InterlockedDecrement(&DeleteBuf->m_iRefCount) == 0)
	{
		DeleteBuf->m_byError = 0;
		DeleteBuf->m_bIsEncoded = FALSE;
		DeleteBuf->m_iWrite = df_HEADER_SIZE;
		DeleteBuf->m_iRead = df_HEADER_SIZE;
		DeleteBuf->m_iWriteLast = 0;
		DeleteBuf->m_iUserWriteBefore = df_HEADER_SIZE;
		DeleteBuf->m_iRefCount = 1;
		CNetServerSerializationBuf::pMemoryPool->Free(DeleteBuf);
	}
}

void CNetServerSerializationBuf::ChunkFreeForcibly()
{
	CNetServerSerializationBuf::pMemoryPool->ChunkFreeForcibly();
}

//////////////////////////////////////////////////////////////////
// Operator <<
//////////////////////////////////////////////////////////////////

CNetServerSerializationBuf &CNetServerSerializationBuf::operator<<(int Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator<<(WORD Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator<<(DWORD Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator<<(UINT Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator<<(UINT64 Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator<<(char Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator<<(BYTE Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator<<(float Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator<<(__int64 Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

//////////////////////////////////////////////////////////////////
// Operator >>
//////////////////////////////////////////////////////////////////

CNetServerSerializationBuf &CNetServerSerializationBuf::operator>>(int &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator>>(WORD &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator>>(DWORD &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator>>(char &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator>>(BYTE &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator>>(float &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator>>(UINT &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator>>(UINT64 &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CNetServerSerializationBuf &CNetServerSerializationBuf::operator>>(__int64 &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}