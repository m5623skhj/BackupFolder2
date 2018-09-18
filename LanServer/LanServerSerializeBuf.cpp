#include "PreCompile.h"
#include "LanServerSerializeBuf.h"
#include "LockFreeMemoryPool.h"

CTLSMemoryPool<CSerializationBuf>* CSerializationBuf::pMemoryPool = new CTLSMemoryPool<CSerializationBuf>(NUM_OF_CHUNK, false);

struct st_Exception
{
	WCHAR ErrorCode[32];
	WCHAR szErrorComment[1024];
};

CSerializationBuf::CSerializationBuf() :
	m_byError(0), m_iWrite(HEADER_SIZE), m_iRead(HEADER_SIZE), m_iWriteLast(0),
	m_iUserWriteBefore(HEADER_SIZE), m_iRefCount(1), m_bHeaderInputted(FALSE)
{		
	Initialize(dfDEFAULTSIZE);
}

CSerializationBuf::~CSerializationBuf()
{
	delete[] m_pSerializeBuffer;
}

void CSerializationBuf::Initialize(int BufferSize)
{
	m_iSize = BufferSize;
	m_pSerializeBuffer = new char[BufferSize];
}

void CSerializationBuf::Init()
{
	m_byError = 0;
	m_iWrite = HEADER_SIZE;
	m_iRead = 0;
	m_iWriteLast = 0;
	m_iUserWriteBefore = HEADER_SIZE;
	m_iRefCount = 1;
}

void CSerializationBuf::Resize(int Size)
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

void CSerializationBuf::WriteBuffer(char *pData, int Size)
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

void CSerializationBuf::ReadBuffer(char *pDest, int Size)
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

void CSerializationBuf::PeekBuffer(char *pDest, int Size)
{
	memcpy_s(pDest, Size, &m_pSerializeBuffer[m_iRead], Size);
}

void CSerializationBuf::RemoveData(int Size)
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

void CSerializationBuf::MoveWritePos(int Size)
{
	if (m_iSize < m_iWrite + Size + HEADER_SIZE)
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

void CSerializationBuf::MoveWritePosThisPos(int ThisPos)
{
	m_iUserWriteBefore = m_iWrite;
	m_iWrite = HEADER_SIZE + ThisPos;
}

void CSerializationBuf::MoveWritePosBeforeCallThisPos()
{
	m_iWrite = m_iUserWriteBefore;
}

BYTE CSerializationBuf::GetBufferError()
{
	return m_byError;
}

char* CSerializationBuf::GetBufferPtr()
{
	return m_pSerializeBuffer;
}

char* CSerializationBuf::GetReadBufferPtr()
{
	return &m_pSerializeBuffer[m_iRead];
}

char* CSerializationBuf::GetWriteBufferPtr()
{
	return &m_pSerializeBuffer[m_iWrite];
}

int CSerializationBuf::GetUseSize()
{
	return m_iWrite - m_iRead/* - HEADER_SIZE*/;
}

int CSerializationBuf::GetFreeSize()
{
	return m_iSize - m_iWrite;
}

int CSerializationBuf::GetAllUseSize()
{
	return m_iWriteLast - m_iRead;
}

void CSerializationBuf::SetWriteZero()
{
	m_iWrite = 0;
}

int CSerializationBuf::GetLastWrite()
{
	return m_iWriteLast;
}

void CSerializationBuf::WritePtrSetHeader()
{
	m_iWriteLast = m_iWrite;
	m_iWrite = 0;
}

void CSerializationBuf::WritePtrSetLast()
{
	m_iWrite = m_iWriteLast;
}

//////////////////////////////////////////////////////////////////
// static
//////////////////////////////////////////////////////////////////

CSerializationBuf* CSerializationBuf::Alloc()
{
	return CSerializationBuf::pMemoryPool->Alloc();
}

void CSerializationBuf::AddRefCount(CSerializationBuf* AddRefBuf)
{
	InterlockedIncrement(&AddRefBuf->m_iRefCount);
}

void CSerializationBuf::Free(CSerializationBuf* DeleteBuf)
{
	UINT RefCnt = InterlockedDecrement(&DeleteBuf->m_iRefCount);
	if (RefCnt == 0)
	{
		DeleteBuf->m_byDelCnt++;

		//DeleteBuf->Init();
		DeleteBuf->m_byError = 0;
		DeleteBuf->m_bHeaderInputted = FALSE;
		DeleteBuf->m_iWrite = HEADER_SIZE;
		DeleteBuf->m_iRead = HEADER_SIZE;
		DeleteBuf->m_iWriteLast = 0;
		DeleteBuf->m_iUserWriteBefore = HEADER_SIZE;
		DeleteBuf->m_iRefCount = 1;
		CSerializationBuf::pMemoryPool->Free(DeleteBuf);
	}
}

void CSerializationBuf::ChunkFreeForcibly()
{
	CSerializationBuf::pMemoryPool->ChunkFreeForcibly();
}

//////////////////////////////////////////////////////////////////
// Operator <<
//////////////////////////////////////////////////////////////////

CSerializationBuf &CSerializationBuf::operator<<(int Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator<<(WORD Input)
{
	*((short*)(&m_pSerializeBuffer[m_iWrite])) = *(short*)&Input;
	m_iWrite += sizeof(WORD);
	return *this;
}

CSerializationBuf &CSerializationBuf::operator<<(DWORD Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator<<(UINT Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator<<(UINT64 Input)
{
	*((long long*)(&m_pSerializeBuffer[m_iWrite])) = *(long long*)&Input;
	m_iWrite += sizeof(UINT64);
	return *this;
}

CSerializationBuf &CSerializationBuf::operator<<(char Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator<<(BYTE Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator<<(float Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator<<(__int64 Input)
{
	*((long long*)(&m_pSerializeBuffer[m_iWrite])) = *(long long*)&Input;
	m_iWrite += sizeof(__int64);
	return *this;
}

//////////////////////////////////////////////////////////////////
// Operator >>
//////////////////////////////////////////////////////////////////

CSerializationBuf &CSerializationBuf::operator>>(int &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator>>(WORD &Input)
{
	*((short*)(&Input)) = *(short*)&m_pSerializeBuffer[m_iRead];
	m_iRead += sizeof(WORD);
	return *this;
}

CSerializationBuf &CSerializationBuf::operator>>(DWORD &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator>>(char &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator>>(BYTE &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator>>(float &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator>>(UINT &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuf &CSerializationBuf::operator>>(UINT64 &Input)
{
	*((long long*)(&Input)) = *(long long*)&m_pSerializeBuffer[m_iRead];
	m_iRead += sizeof(UINT64);
	return *this;
}

CSerializationBuf &CSerializationBuf::operator>>(__int64 &Input)
{
	*((long long*)(&Input)) = *(long long*)&m_pSerializeBuffer[m_iRead];
	m_iRead += sizeof(__int64);
	return *this;
}