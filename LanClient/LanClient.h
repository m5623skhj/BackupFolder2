#pragma once
#include <unordered_map>
#include "Ringbuffer.h"
//#include "Stack.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"

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

#define dfSERVER_IP							L"127.0.0.1"
#define dfSERVER_PORT						6000

//////////////////////////////////////////////////////////////////////////////////////
// LANSERVER_ERR
// CLanServer 내에서 발생하는 에러 메시지들의 집합
// 새로운 에러가 필요할 경우 추가할 것
//////////////////////////////////////////////////////////////////////////////////////
enum LANCLIENT_ERR
{
	NO_ERR = 0,
	WSASTARTUP_ERR,
	LISTEN_SOCKET_ERR,
	LISTEN_BIND_ERR,
	LISTEN_LISTEN_ERR,
	BEGINTHREAD_ERR,
	SETSOCKOPT_ERR,
	WORKERIOCP_NULL_ERR,
	PARSING_ERR,
	SESSION_NULL_ERR,
	ACCEPT_ERR,
	WSARECV_ERR,
	WSASEND_ERR,
	OVERLAPPED_NULL_ERR,
	SERIALIZEBUF_NULL_ERR,
	RINGBUFFER_MAX_SIZE_ERR,
	RINGBUFFER_MIN_SIZE_ERR,
	INCORRECT_SESSION_ERR,
	HEADER_CODE_ERR,
	CHECKSUM_ERR,
	END_OF_ERR
};

//////////////////////////////////////////////////////////////////////////////////////
// st_Error
// Error 관리 구조체
// GetLanServerError() 를 호출할 경우 
// LanServer 클래스가 지니고 해당 형식의 에러 구조체를 넘겨줌
//////////////////////////////////////////////////////////////////////////////////////
struct st_Error
{
	int GetLastErr = 0;
	int LanClientErr = 0;
	int Line = 0;
};

class CNetServerSerializationBuf;
class CLanClient
{
private:
	BYTE		m_byNumOfWorkerThread;
	SOCKET		m_Connet;
	UINT		m_uiMaxClient;

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
		CLockFreeQueue<CNetServerSerializationBuf*>	SendQ;
	};

	struct Session
	{
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


	Session					m_pSessionArray;

	bool ReleaseSession(Session *pSession);

	char RecvPost(Session *pSession);
	char SendPost(Session *pSession);

	UINT Worker();
	static UINT __stdcall WorkerThread(LPVOID pLanClient);

	bool LanClientOptionParsing();

public:
	CLanClient();
	virtual ~CLanClient();

	bool Start(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle, UINT MaxClient);
	void Stop();

	bool DisConnect(UINT64 SessionID);
	bool SendPacket(UINT64 SessionID, CNetServerSerializationBuf *pSerializeBuf);

	// Accept 후 접속처리 완료 후 호출
	virtual void OnClientJoin(UINT64 OutClientID/* Client 정보 / ClientID / 기타등등 */) = 0;
	// Disconnect 후 호출
	virtual void OnClientLeave(UINT64 ClientID) = 0;
	// Accept 직후 IP 차단등을 위한 용도
	virtual bool OnConnectionRequest(const WCHAR *IP) = 0;

	// 패킷 수신 완료 후
	virtual void OnRecv(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf) = 0;
	// 패킷 송신 완료 후
	virtual void OnSend() = 0;

	// 워커스레드 GQCS 바로 하단에서 호출
	virtual void OnWorkerThreadBegin() = 0;
	// 워커스레드 1루프 종료 후
	virtual void OnWorkerThreadEnd() = 0;
	// 사용자 에러 처리 함수
	virtual void OnError(st_Error *OutError) = 0;
};
