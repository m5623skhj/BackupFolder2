#pragma once
#include "LanClient.h"

#define dfSESSIONKEY_SIZE			64

class CChatServer;

struct st_SessionKey;

class CChattingLanClient : CLanClient
{
public:
	CChattingLanClient();
	virtual ~CChattingLanClient();

	bool ChattingLanClientStart(CChatServer* pChattingServer, const WCHAR *szOptionFileName, CTLSMemoryPool<st_SessionKey> *pSessionKeyMemoryPool);
	void Stop();

	int NumOfRecvPacket = 0;

private:
	// 서버에 Connect 가 완료 된 후
	virtual void OnConnectionComplete();

	// 패킷 수신 완료 후
	virtual void OnRecv(CSerializationBuf *OutReadBuf);
	// 패킷 송신 완료 후
	virtual void OnSend();

	// 워커스레드 GQCS 바로 하단에서 호출
	virtual void OnWorkerThreadBegin();
	// 워커스레드 1루프 종료 후
	virtual void OnWorkerThreadEnd();
	// 사용자 에러 처리 함수
	virtual void OnError(st_Error *OutError);

private:

	struct st_ServerInfo
	{
		BYTE	ServerType;			// dfSERVER_TYPE_GAME / dfSERVER_TYPE_CHAT
		UINT64  ClientSessionID;
		WCHAR	ServerName[32];		// 해당 서버의 이름. 
	};

	BYTE										m_byServerIndex;

	UINT64										m_iIdentificationNumber;
	LPCRITICAL_SECTION							m_pAccountMapCriticalSection;
	CTLSMemoryPool<st_SessionKey>				*m_pSessionKeyMemoryPool;

	CChatServer									*m_pChattingServer;
};