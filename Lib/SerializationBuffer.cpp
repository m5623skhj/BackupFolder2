#include "PreComfile.h"
#include "SerializationBuffer.h"

struct st_Exception
{
	WCHAR ErrorCode[32];
	WCHAR szErrorComment[1024];
};

CSerializationBuffer::CSerializationBuffer() : m_byError(0), m_iWrite(HEADER_SIZE), m_iRead(0), m_iWriteLast(0)
{
	Initialize(dfDEFAULTSIZE);
}

CSerializationBuffer::~CSerializationBuffer()
{
	delete[] m_pSerializeBuffer;
}

void CSerializationBuffer::Initialize(int BufferSize)
{
	m_iSize = BufferSize;
	m_pSerializeBuffer = new char[BufferSize];
}

void CSerializationBuffer::Resize(int Size)
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

void CSerializationBuffer::WriteBuffer(char *pData, int Size)
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

void CSerializationBuffer::ReadBuffer(char *pDest, int Size)
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

void CSerializationBuffer::PeekBuffer(char *pDest, int Size)
{
	memcpy_s(pDest, Size, &m_pSerializeBuffer[m_iRead], Size);
}

void CSerializationBuffer::RemoveData(int Size)
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

void CSerializationBuffer::MoveWritePos(int Size)
{
	if (m_iSize < m_iWrite + Size)
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

void CSerializationBuffer::MoveWritePosThisPos(int ThisPos)
{
	if (ThisPos < ThisPos)
	{
		m_byError = 2;
		st_Exception e;
		wcscpy_s(e.ErrorCode, L"ErrorCode : 2");
		wsprintf(e.szErrorComment,
			L"%s Line %d\n\n버퍼에 쓰려고 하였으나, 버퍼 공간보다 더 큰 값이 들어왔습니다.\nWrite = %d Read = %d BufferSize = %d InputSize = %d\n\n프로그램을 종료합니다"
			, TEXT(__FILE__), __LINE__, m_iWrite, m_iRead, m_iSize, ThisPos);
		throw e;
	}
	m_iWrite = ThisPos;
}

void CSerializationBuffer::MoveWriteAndReadPos(int Pos)
{
	m_iRead = Pos;
	m_iWrite = Pos;
}

BYTE CSerializationBuffer::GetBufferError()
{
	return m_byError;
}

char* CSerializationBuffer::GetBufferPtr()
{
	return m_pSerializeBuffer;
}

char* CSerializationBuffer::GetReadBufferPtr()
{
	return &m_pSerializeBuffer[m_iRead];
}

char* CSerializationBuffer::GetWriteBufferPtr()
{
	return &m_pSerializeBuffer[m_iWrite];
}

int CSerializationBuffer::GetUseSize()
{
	return m_iWrite - m_iRead;
}

int CSerializationBuffer::GetFreeSize()
{
	return m_iSize - m_iWrite;
}

int CSerializationBuffer::GetLastWrite()
{
	return m_iWriteLast;
}

void CSerializationBuffer::WritePtrSetPayload(int HeaderSize)
{
	m_iWrite = HeaderSize;
}

void CSerializationBuffer::WritePtrSetHeader()
{
	m_iWriteLast = m_iWrite;
	m_iWrite = 0;
}

void CSerializationBuffer::WritePtrSetLast()
{
	m_iWrite = m_iWriteLast;
}

//////////////////////////////////////////////////////////////////
// Operator <<
//////////////////////////////////////////////////////////////////

CSerializationBuffer &CSerializationBuffer::operator<<(int Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator<<(WORD Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator<<(DWORD Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator<<(UINT Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator<<(UINT64 Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator<<(char Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator<<(BYTE Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator<<(float Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator<<(__int64 Input)
{
	WriteBuffer((char*)&Input, sizeof(Input));
	return *this;
}

//////////////////////////////////////////////////////////////////
// Operator >>
//////////////////////////////////////////////////////////////////

CSerializationBuffer &CSerializationBuffer::operator>>(int &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator>>(WORD &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator>>(DWORD &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator>>(char &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator>>(BYTE &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator>>(float &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator>>(UINT &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator>>(UINT64 &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}

CSerializationBuffer &CSerializationBuffer::operator>>(__int64 &Input)
{
	ReadBuffer((char*)&Input, sizeof(Input));
	return *this;
}