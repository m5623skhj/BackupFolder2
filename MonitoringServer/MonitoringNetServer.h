#pragma once
#include "NetServer.h"
#include "MonitoringServerCommon.h"

#define dfACCOUNT_MAX			30

class CMonitoringNetServer : public CNetServer
{
public :
	CMonitoringNetServer();
	virtual ~CMonitoringNetServer();

	bool MonitoringNetServerStart(const WCHAR *szNetServerOptionFileName, const WCHAR *szMonitoringServerOption);
	void MonitoringNetServerStop();

	bool MonitoringServerOptionParsing(const WCHAR *szOptionFileName);

	// Accept 후 접속처리 완료 후 호출
	virtual void OnClientJoin(UINT64 OutClientID);
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

	void SendMonitoringInfoToNetClient(st_MonitorToolUpdateData &SendToolData);
private :
	//WORD									m_wNumOfMaxMonitoringClient;
	//WORD									m_wNowMonitoringClientIndex;
	//st_ServerLoginInfo						*m_pNetServerLoginInfo;
	char									m_LoginSessionKey[32];

	int										m_NumOfLoginUser;
	UINT64									m_uiAccountNoArray[dfACCOUNT_MAX];
	CRITICAL_SECTION						m_AccountNoArrayCS;
	//std::map<UINT64, st_ServerLoginInfo>	m_LoginInfoMap;
	//st_ServerLoginInfo						m_NetServerLoginInfo;
};