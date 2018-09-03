#pragma once
#include <unordered_map>
#include "Ringbuffer.h"
//#include "Stack.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"

// �ش� ������ �۽��߿� �ִ��� �ƴ���
#define NONSENDING	0
#define SENDING		1
// ���� ID �� ���ؼ� ���� �ε����� ã�� ���� ����Ʈ ��
#define SESSION_INDEX_SHIFT 48
// Recv / Send Post ��ȯ ��
#define POST_RETVAL_ERR_SESSION_DELETED		0
#define POST_RETVAL_ERR						1
#define POST_RETVAL_COMPLETE				2

#define ONE_SEND_WSABUF_MAX					200

#define df_RELEASE_VALUE					0x100000000

#define dfSERVER_IP							L"127.0.0.1"
#define dfSERVER_PORT						6000

//////////////////////////////////////////////////////////////////////////////////////
// LANSERVER_ERR
// CLanServer ������ �߻��ϴ� ���� �޽������� ����
// ���ο� ������ �ʿ��� ��� �߰��� ��
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
// Error ���� ����ü
// GetLanServerError() �� ȣ���� ��� 
// LanServer Ŭ������ ���ϰ� �ش� ������ ���� ����ü�� �Ѱ���
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

	// Accept �� ����ó�� �Ϸ� �� ȣ��
	virtual void OnClientJoin(UINT64 OutClientID/* Client ���� / ClientID / ��Ÿ��� */) = 0;
	// Disconnect �� ȣ��
	virtual void OnClientLeave(UINT64 ClientID) = 0;
	// Accept ���� IP ���ܵ��� ���� �뵵
	virtual bool OnConnectionRequest(const WCHAR *IP) = 0;

	// ��Ŷ ���� �Ϸ� ��
	virtual void OnRecv(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf) = 0;
	// ��Ŷ �۽� �Ϸ� ��
	virtual void OnSend() = 0;

	// ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
	virtual void OnWorkerThreadBegin() = 0;
	// ��Ŀ������ 1���� ���� ��
	virtual void OnWorkerThreadEnd() = 0;
	// ����� ���� ó�� �Լ�
	virtual void OnError(st_Error *OutError) = 0;
};
