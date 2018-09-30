#pragma once
#include <unordered_map>
#include "Ringbuffer.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"
#include "ServerCommon.h"

// 해당 소켓이 송신중에 있는지 아닌지
#define NONSENDING	0
#define SENDING		1
// 세션 ID 에 대해서 세션 인덱스를 찾기 위한 쉬프트 값
#define SESSION_INDEX_SHIFT 48
// Recv / Send Post 반환 값
#define POST_RETVAL_ERR_SESSION_DELETED		0
#define POST_RETVAL_ERR						1
#define POST_RETVAL_COMPLETE				2

#define ONE_SEND_WSABUF_MAX					200

#define df_RELEASE_VALUE					0x100000000

struct st_Error;

class CNetServerSerializationBuf;

class CNetClient
{
private:
	BYTE		m_byNumOfWorkerThread;
	BYTE		m_byNumOfUsingWorkerThread;
	BOOL		m_bIsNagleOn;
	WORD		m_wPort;
	UINT		m_uiNumOfUser;
	UINT		m_uiMaxClient;
	UINT		m_uiAcceptTotal;
	UINT		m_uiAcceptTPS;

	UINT64		m_iIDCount;
	WCHAR		m_IP[16];

	HANDLE		*m_pWorkerThreadHandle;
	HANDLE		m_hAcceptThread;
	HANDLE		m_hWorkerIOCP;

	struct OVERLAPPEDIODATA
	{
		WORD		wBufferCount;
		UINT		IOMode;
		OVERLAPPED  Overlapped;
		CRingbuffer RingBuffer;
	};

	struct OVERLAPPED_SEND_IO_DATA
	{
		LONG											lBufferCount;
		UINT											IOMode;
		OVERLAPPED										Overlapped;

		CLockFreeQueue<CNetServerSerializationBuf*>		SendQ;
	};

	WORD						m_wNumOfReconnect;
	UINT						m_IOCount;
	SOCKET						m_sock;
	UINT64						m_SessionID;
	OVERLAPPEDIODATA			m_RecvIOData;
	OVERLAPPED_SEND_IO_DATA		m_SendIOData;

	CNetServerSerializationBuf*	m_pSeirializeBufStore[ONE_SEND_WSABUF_MAX];

	bool ReleaseSession();

	char RecvPost();
	char SendPost();

	UINT Worker();
	static UINT __stdcall WorkerThread(LPVOID pNetServer);

	bool NetServerOptionParsing(const WCHAR *szOptionFileName);

	bool SessionAcquireLock();
	void SessionAcquireUnLock();

protected:
	bool Start(const WCHAR *szOptionFileName);
	void Stop();

public:
	CNetClient();
	virtual ~CNetClient();

	bool SendPacket(CNetServerSerializationBuf *pSerializeBuf);

	// 서버에 Connect 가 완료 된 후
	virtual void OnConnectionComplete() = 0;

	// 패킷 수신 완료 후
	virtual void OnRecv(CNetServerSerializationBuf *OutReadBuf) = 0;
	// 패킷 송신 완료 후
	virtual void OnSend(int sendsize) = 0;

	// 워커스레드 GQCS 바로 하단에서 호출
	virtual void OnWorkerThreadBegin() = 0;
	// 워커스레드 1루프 종료 후
	virtual void OnWorkerThreadEnd() = 0;
	// 사용자 에러 처리 함수
	virtual void OnError(st_Error *OutError) = 0;

	int checksum = 0;
	int payloadOver = 0;
	int HeaderCode = 0;
};
