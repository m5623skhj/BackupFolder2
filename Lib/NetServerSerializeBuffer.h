#pragma once
#include "LockFreeMemoryPool.h"
#include <list>

#define dfDEFAULTSIZE		1024
#define BUFFFER_MAX			10000

// Header �� ũ�⸦ ������
// ���α׷����� ��������
// ?E�Eũ�⿡ ����E������ ��
// ���� ���ο��� �ݵ�� 
// WritePtrSetHeader() �� WritePtrSetLast() �� �翁E����ν�E
// ?E�E���� �κа�E���̷ε�E������ �κ����� �����͸� �Ű���� ��
// Ȥ�� GetLastWrite() �� �翁EϿ?�翁Eڰ?�� ������ ���� �������� �Ѱ���
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
	// WritePtrSetHeader �Լ��� ���� �������� �� ������ �⾁E?
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
	// ����E�������� ������ ������ ������ �⾁Eϰ�E
	// ����E�����͸� ����ȭ ���� ����Eó������ �ű�E
	void WritePtrSetHeader();
	// ����E�����͸� ����E?�翁Eߴ�E������ ������ �κ����� �̵���Ŵ
	void WritePtrSetLast();

	// ������ ó������ �翁Eڰ?
	// ���������� �� ���������� ���� ����
	int GetLastWrite();

	char *GetBufferPtr();
	
	// ���ϴ� ���̸�ŭ �б�E��ġ���� ����
	void RemoveData(int Size);


	// ����E����E�� ���� �翁E�� ��
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

	// ���� ����ũ�⸦ �Ҵ��Ͽ� ����E�����͸� �ű�E
	void Resize(int Size);

	// ���ϴ� ���̸�ŭ �б�E����E��ġ �̵�
	void MoveWritePos(int Size);
	// ThisPos�� ����E�����͸� �̵���Ŵ
	void MoveWritePosThisPos(int ThisPos);
	// MoveWritePosThisPos �� �̵��ϱ�E����E����E�����ͷ� �̵���Ŵ
	// �� �Լ��� �翁E���E�ʰ�E�� �Լ��� ȣ���� �濁E
	// ����E�����͸� ����Eó������ �̵���Ŵ
	void MoveWritePosBeforeCallThisPos();


	// --------------- ��ȯ�� ---------------
	// 0 : ����ó�� �Ǿ���
	// 1 : ���۸� ��������E�Ͽ�����, �д� ������ ���� ũ�⺸�� ũ�ų� ����E��������E���� ������
	// 2 : ���۸� ������E�Ͽ�����, ���� ������ ���� ũ�⺸�� ŭ
	// --------------- ��ȯ�� ---------------
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
