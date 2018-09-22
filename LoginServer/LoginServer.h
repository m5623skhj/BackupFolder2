#pragma once

#include "NetServer.h"
#include <unordered_map>

#define dfUSER_INFO_SIZE						80
#define dfWCHAR_IP_SIZE							32


class CNetServerSerializationBuf;
class CDBConnector;
class CLoginLanServer;

class CLoginServer : CNetServer
{
public :
	CLoginServer();
	virtual ~CLoginServer();

	bool LoginServerStart(const WCHAR *szLoginServerFileName, const WCHAR *szLoginLanServerFileName, const WCHAR *szLogInServerOptionFile);
	void LoginServerStop();
	void LoginComplete(UINT64 AccountNo, BYTE Status);

	bool ChattingServerJoined();

	int NumOfLoginWaitingUser;
	int NumOfLoginCompleteUser;

	int GetLanServerSend();
	int GetLanServerRecv();
	int GetNetServerAcceptTotal() { return GetAcceptTotal(); }

	int GetUsingNetSerializeBufCount();
	int GetUsingNetUserInfoNodeCount();

	int GetUsingLanServerSessionNodeCount();
	int GetUsingLanSerializeBufCount();

private:
	// Accept 후 접속처리 완료 후 호출
	virtual void OnClientJoin(UINT64 OutClientID/* Client 정보 / ClientID / 기타등등 */);
	// Disconnect 후 호출
	virtual void OnClientLeave(UINT64 ClientID);
	// Accept 직후 IP 차단등을 위한 용도
	virtual bool OnConnectionRequest(const WCHAR *IP);

	// 패킷 수신 완료 후
	virtual void OnRecv(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf);
	// 패킷 송신 완료 후
	virtual void OnSend(UINT64 ClientID, int sendsize);

	// 워커스레드 GQCS 바로 하단에서 호출
	virtual void OnWorkerThreadBegin();
	// 워커스레드 1루프 종료 후
	virtual void OnWorkerThreadEnd();
	// 사용자 에러 처리 함수
	virtual void OnError(st_Error *OutError);

	bool LoginServerOptionParsing(const WCHAR *szLogInServerOptionFile);

	struct st_LoginUserInfo
	{
		UINT64		uiSessionID;
		WCHAR		szID[20];
		WCHAR		szNick[20];
	};

	CRITICAL_SECTION								m_UserSessionIDMapCriticalSection;
	std::unordered_map<INT64, st_LoginUserInfo*>	m_LoginUserInfoMap;
	CTLSMemoryPool<st_LoginUserInfo>				*m_pUserInfoMemeoryPool;

	//CDBConnector									*m_pDBConnector;
	// 채팅서버와의 내부 통신을 위한 객체
	CLoginLanServer									*m_pLoginLanServer;

	USHORT											m_usGameServerPort;
	USHORT											m_usChatServerPort;
	WCHAR											m_GameServerIP[16];
	WCHAR											m_ChatServerIP[16];
};