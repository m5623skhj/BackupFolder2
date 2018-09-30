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

//using namespace Olbbemi;

class CNetServer
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

	SOCKET		m_ListenSock;
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
		LONG										lBufferCount;
		UINT										IOMode;
		OVERLAPPED									Overlapped;
		
		CLockFreeQueue<CNetServerSerializationBuf*>		SendQ;
	};

	enum en_CALL_WHERER
	{
		Call_Accept = 1,
		Call_Accept_Last,
		Call_Transffered_Zero,
		Call_Header_Error,
		Call_Payload_Error,
		Call_Checksum_Error,
		Call_GQCS_END,
		Call_RecvPost1,
		Call_RecvPost2,
		Call_RecvPost3,
		Call_SendPost1,
		Call_SendPost2,
		Call_SendPost3,
		Call_AcquireLock,
		Call_AcquireUnLock,
	};

	struct Session
	{
		BOOL						bSendDisconnect;
		UINT						IOCount;
		UINT						IsUseSession;
		SOCKET						sock;
		UINT64						SessionID;
		OVERLAPPEDIODATA			RecvIOData;
		OVERLAPPED_SEND_IO_DATA		SendIOData;

		CNetServerSerializationBuf*	pSeirializeBufStore[ONE_SEND_WSABUF_MAX];

		///////////////////////////////////////////
		//LONG				New;
		//LONG				Del;
		///////////////////////////////////////////
	};


	Session					*m_pSessionArray;
	CLockFreeStack<WORD>	*m_pSessionIndexStack;

	bool ReleaseSession(Session *pSession);

	char RecvPost(Session *pSession);
	char SendPost(Session *pSession);

	UINT Accepter();
	UINT Worker();
	static UINT __stdcall AcceptThread(LPVOID pNetServer);
	static UINT __stdcall WorkerThread(LPVOID pNetServer);

	bool NetServerOptionParsing(const WCHAR *szOptionFileName);

	bool SessionAcquireLock(UINT64 SessionID);
	void SessionAcquireUnLock(WORD SessionIndex);

protected :
	bool Start(const WCHAR *szOptionFileName);
	void Stop();

public:
	CNetServer();
	virtual ~CNetServer();

	bool DisConnect(UINT64 SessionID);
	bool SendPacket(UINT64 SessionID, CNetServerSerializationBuf *pSerializeBuf);
	bool SendPacketAndDisConnect(UINT64 SessionID, CNetServerSerializationBuf *pSendBuf);

	// Accept 후 접속처리 완료 후 호출
	virtual void OnClientJoin(UINT64 OutClientID/* Client 정보 / ClientID / 기타등등 */) = 0;
	// Disconnect 후 호출
	virtual void OnClientLeave(UINT64 ClientID) = 0;
	// Accept 직후 IP 차단등을 위한 용도
	virtual bool OnConnectionRequest(const WCHAR *IP) = 0;

	// 패킷 수신 완료 후
	virtual void OnRecv(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf) = 0;
	// 패킷 송신 완료 후
	virtual void OnSend(UINT64 ClientID, int sendsize) = 0;

	// 워커스레드 GQCS 바로 하단에서 호출
	virtual void OnWorkerThreadBegin() = 0;
	// 워커스레드 1루프 종료 후
	virtual void OnWorkerThreadEnd() = 0;
	// 사용자 에러 처리 함수
	virtual void OnError(st_Error *OutError) = 0;

	UINT GetNumOfUser();
	UINT GetStackRestSize();
	UINT GetAcceptTotal();
	UINT GetAcceptTPSAndReset();

	int checksum = 0;
	int payloadOver = 0;
	int HeaderCode = 0;
};
