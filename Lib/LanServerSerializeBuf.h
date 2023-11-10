#pragma once
//#include "LanServer.h"
#include "LockFreeMemoryPool.h"
#include <Windows.h>
#include <list>

#define dfLAN_DEFAULTSIZE	512
#define BUFFFER_MAX			10000

// Header �� ũ�⸦ ������
// ���ο��� �ݵ�� 
// WritePtrSetHeader() �� WritePtrSetLast() �� ��������ν�
// ��� ���� �κа� ���̷ε� ������ �κ����� �����͸� �Ű���� ��
// Ȥ�� GetLastWrite() �� ����Ͽ� ����ڰ� �� ������ ���� ������� �Ѱ���
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
	// WritePtrSetHeader �Լ��� ���� �������� �� ������ �����
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
	// ���� �������� ������ ������ ������ ����ϰ�
	// ���� �����͸� ����ȭ ���� ���� ó������ �ű�
	void WritePtrSetHeader();
	// ���� �����͸� ������ ����ߴ� ������ ������ �κ����� �̵���Ŵ
	void WritePtrSetLast();

	// ������ ó������ ����ڰ�
	// ���������� �� ���������� ���� ����
	int GetLastWrite();

	char *GetBufferPtr();
	char *GetWriteBufferPtr();

	void PeekBuffer(char *pDest, int Size);

	// ���ϴ� ���̸�ŭ �б� ��ġ���� ����
	void RemoveData(int Size); 

	// ���� ���� �� ���� ��� �� ��
	static void ChunkFreeForcibly();

	int GetAllUseSize();
	FORCEINLINE void CheckReadBufferSize(int needSize);
	FORCEINLINE void CheckWriteBufferSize(int needSize);
public:
	CSerializationBuf();
	~CSerializationBuf();

	void Init();

	// ���� ����ũ�⸦ �Ҵ��Ͽ� ���� �����͸� �ű�
	void Resize(int Size);

	// ���ϴ� ���̸�ŭ �б� ���� ��ġ �̵�
	void MoveWritePos(int Size);
	// ThisPos�� ���� �����͸� �̵���Ŵ
	void MoveWritePosThisPos(int ThisPos);
	// MoveWritePosThisPos �� �̵��ϱ� ���� ���� �����ͷ� �̵���Ŵ
	// �� �Լ��� ������� �ʰ� �� �Լ��� ȣ���� ���
	// ���� �����͸� ���� ó������ �̵���Ŵ
	void MoveWritePosBeforeCallThisPos();

	char *GetReadBufferPtr();
	void WriteBuffer(char *pData, int Size);
	void WriteBuffer(const std::string& dest);
	void WriteBuffer(const std::wstring& dest);
	void ReadBuffer(char* pDest, int Size);
	void ReadBuffer(OUT std::string& dest);
	void ReadBuffer(OUT std::wstring& dest);

	// --------------- ��ȯ�� ---------------
	// 0 : ����ó�� �Ǿ���
	// 1 : ���۸� �������� �Ͽ�����, �д� ������ ���� ũ�⺸�� ũ�ų� ���� �������� ���� ������
	// 2 : ���۸� ������ �Ͽ�����, ���� ������ ���� ũ�⺸�� ŭ
	// --------------- ��ȯ�� ---------------
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
	CSerializationBuf& operator<<(std::list<std::string>& input);
	CSerializationBuf& operator<<(std::list<std::wstring>& input);

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

