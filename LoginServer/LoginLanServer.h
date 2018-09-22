#pragma once
#include "LanServer.h"

#define dfSESSIONKEY_SIZE			64

//#define dfACQUIRE_RESPONSE_SHIFT	60
#define dfACQUIRE_RESPONSE_SHIFT	60
#define dfIDENTIFIER_ERR_SHIFT		56
//#define dfAND_PARAMETER_PART		0x0fffffffffffffff
#define dfAND_PARAMETER_PART		0x00ffffffffffffff

#define dfNUM_OF_SERVER				1

class CLoginServer;

class CLoginLanServer : CLanServer
{
public:
	CLoginLanServer();
	virtual ~CLoginLanServer();

	bool LoginLanServerStart(CLoginServer* pLoginServer, const WCHAR *szOptionFileName);
	void Stop();

	void SendLoginPacketToLanClientAll(INT64 AccountNo, char *SessionKey);

	bool ChattingServerJoined() { return m_bChattingServerClientJoined; }

	int GetUsingSessionNodeCount() { return m_pSessionInfoTLS->GetUseNodeCount(); }
	int GetUsingLanSerializeBufCount();

	int NumOfSendLanClient;
	int NumOfRecvLanClient;

private :
	// Accept 후 접속처리 완료 후 호출
	virtual void OnClientJoin(UINT64 OutClientID/* Client 정보 / ClientID / 기타등등 */);
	// Disconnect 후 호출
	virtual void OnClientLeave(UINT64 ClientID);
	// Accept 직후 IP 차단등을 위한 용도
	virtual bool OnConnectionRequest();

	// 패킷 수신 완료 후
	virtual void OnRecv(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf);
	// 패킷 송신 완료 후
	virtual void OnSend(UINT64 ClientID, int sendsize);

	// 워커스레드 GQCS 바로 하단에서 호출
	virtual void OnWorkerThreadBegin();
	// 워커스레드 1루프 종료 후
	virtual void OnWorkerThreadEnd();
	// 사용자 에러 처리 함수
	virtual void OnError(st_Error *OutError);

private :
	struct st_ServerInfo
	{
		BYTE	ServerType;			// dfSERVER_TYPE_GAME / dfSERVER_TYPE_CHAT
		UINT64  ClientSessionID;
		WCHAR	ServerName[32];		// 해당 서버의 이름. 
	};

	struct st_SessionInfo
	{
		UINT64 SessionKeyIdentifier;
		BYTE   SessionKey[64];
	};
	
	BYTE										m_byServerIndex;
	bool										m_bChattingServerClientJoined = false;

	UINT64										m_iIdentificationNumber;
	CTLSMemoryPool<st_SessionInfo>				*m_pSessionInfoTLS;
	std::unordered_map<INT64, st_SessionInfo*>	m_AccountMap;
	CRITICAL_SECTION							m_AccountMapCriticalSection;

	CLoginServer								*m_pLoginServer;
	st_ServerInfo								m_ChattingServer;
};
