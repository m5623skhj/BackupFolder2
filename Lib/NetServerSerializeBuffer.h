#pragma once
#include "LockFreeMemoryPool.h"
#include <list>

#define dfDEFAULTSIZE		1024
#define BUFFFER_MAX			10000

// Header ﾀﾇ ﾅｩｱ篋ｦ ｰ眞､ﾇﾔ
// ﾇﾁｷﾎｱﾗｷ･ｸｶｴﾙ ﾀｯｵｿﾀ釥ﾓ
// ?E・ﾅｩｱ篩｡ ｵ郞・ｼ､ﾇﾒ ｰﾍ
// ｶﾇﾇﾑ ｳｻｺﾎｿ｡ｼｭ ｹﾝｵ蠖ﾃ 
// WritePtrSetHeader() ｿﾍ WritePtrSetLast() ｸｦ ｻ鄙・ﾔﾀｸｷﾎｽ・
// ?E・ｽﾃﾀﾛ ｺﾎｺﾐｰ・ﾆ菎ﾌｷﾎｵ・ｸｶﾁｷ ｺﾎｺﾐﾀｸｷﾎ ﾆﾎﾅﾍｸｦ ｿﾅｰﾜﾁ狎ﾟ ﾇﾔ
// ﾈ､ﾀｺ GetLastWrite() ｸｦ ｻ鄙・ﾏｿ?ｻ鄙・ﾚｰ?ｾｴ ｸｶﾁｷ ｰｪﾀｻ ｻ鄲ﾌﾁ﨧ﾎ ｳﾑｰﾜﾁﾜ
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
	// WritePtrSetHeader ﾇﾔｼﾎ ｺﾎﾅﾍ ｸｶﾁｷｿ｡ ｾｴ ｰ｣ﾀｻ ｱ篝・?
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
	// ｾｲｱ・ﾆﾎﾅﾍﾀﾇ ｸｶﾁｷ ｰ｣ﾀｻ ｺｯｼ｡ ｱ篝・ﾏｰ・
	// ｾｲｱ・ﾆﾎﾅﾍｸｦ ﾁﾄﾈｭ ｹﾛ ｰ｡ﾀ・ﾃｳﾀｽﾀｸｷﾎ ｿﾅｱ・
	void WritePtrSetHeader();
	// ｾｲｱ・ﾆﾎﾅﾍｸｦ ﾀﾌﾀ・?ｻ鄙・ﾟｴ・ｰ｣ﾀﾇ ｸｶﾁｷ ｺﾎｺﾐﾀｸｷﾎ ﾀﾌｵｿｽﾃﾅｴ
	void WritePtrSetLast();

	// ｹﾛﾀﾇ ﾃｳﾀｽｺﾎﾅﾍ ｻ鄙・ﾚｰ?
	// ｸｶﾁｷﾀｸｷﾎ ｾｴ ｰ｣ｱ錝ﾇ ﾂｦ ｱｸﾇﾔ
	int GetLastWrite();

	char *GetBufferPtr();
	
	// ｿﾏｴﾂ ｱ貘ﾌｸｸﾅｭ ﾀﾐｱ・ﾀｧﾄ｡ｿ｡ｼｭ ｻ霖ｦ
	void RemoveData(int Size);


	// ｼｭｹ・ﾁｾｷ・ﾇﾒ ｶｧｸｸ ｻ鄙・ﾇﾒ ｰﾍ
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

	// ｻﾎ ｹﾛﾅｩｱ篋ｦ ﾇﾒｴ酩ﾏｿｩ ﾀﾌﾀ・ｵ･ﾀﾌﾅﾍｸｦ ｿﾅｱ・
	void Resize(int Size);

	// ｿﾏｴﾂ ｱ貘ﾌｸｸﾅｭ ﾀﾐｱ・ｾｲｱ・ﾀｧﾄ｡ ﾀﾌｵｿ
	void MoveWritePos(int Size);
	// ThisPosｷﾎ ｾｲｱ・ﾆﾎﾅﾍｸｦ ﾀﾌｵｿｽﾃﾅｴ
	void MoveWritePosThisPos(int ThisPos);
	// MoveWritePosThisPos ｷﾎ ﾀﾌｵｿﾇﾏｱ・ﾀﾌﾀ・ｾｲｱ・ﾆﾎﾅﾍｷﾎ ﾀﾌｵｿｽﾃﾅｴ
	// ﾀｧ ﾇﾔｼｦ ｻ鄙・ﾏﾁ・ｾﾊｰ・ﾀﾌ ﾇﾔｼｦ ﾈ｣ﾃ簓ﾒ ｰ豼・
	// ｾｲｱ・ﾆﾎﾅﾍｸｦ ｰ｡ﾀ・ﾃｳﾀｽﾀｸｷﾎ ﾀﾌｵｿｽﾃﾅｴ
	void MoveWritePosBeforeCallThisPos();


	// --------------- ｹﾝﾈｯｰｪ ---------------
	// 0 : ﾁ､ｻｳｸｮ ｵﾇｾ惕ｽ
	// 1 : ｹﾛｸｦ ﾀﾐﾀｸｷﾁｰ・ﾇﾏｿｴﾀｸｳｪ, ﾀﾐｴﾂ ｰ｣ﾀﾌ ｹﾛ ﾅｩｱ篌ｸｴﾙ ﾅｩｰﾅｳｪ ｾﾆﾁ・ｾｲｿｩﾀﾖﾁ・ｾﾊﾀｺ ｰ｣ﾀﾓ
	// 2 : ｹﾛｸｦ ｾｲｷﾁｰ・ﾇﾏｿｴﾀｸｳｪ, ｾｲｴﾂ ｰ｣ﾀﾌ ｹﾛ ﾅｩｱ篌ｸｴﾙ ﾅｭ
	// --------------- ｹﾝﾈｯｰｪ ---------------
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
