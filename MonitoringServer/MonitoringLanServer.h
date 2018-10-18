#pragma once
#include "LanServer.h"
#include "MonitoringServerCommon.h"

#define dfLOGIN_SERVER								0
#define dfGAME_SERVER								1
#define dfCHAT_SERVER								2

#define dfNUM_OF_SERVER								5
#define dfNUM_OF_DATATYPE							42

class CMonitoringNetServer;

class CMonitoringLanServer : public CLanServer
{
public :
	CMonitoringLanServer();
	virtual ~CMonitoringLanServer();

	bool MonitoringLanServerStart(const WCHAR *szMonitoringLanServerOptionFileName, const WCHAR *szNetServerOptionFileName, const WCHAR *szMonitoringServerOption);
	void MonitoringLanServerStop();

	// Accept 후 접속처리 완료 후 호출
	virtual void OnClientJoin(UINT64 OutClientID);
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
	static UINT __stdcall DBSendThread(LPVOID pMonitoringServer);
	UINT DBSender();

private :
	struct st_UnitItemInfo
	{
		int iLast = 0;
		int iMax = 0;
		int iMin = 99999;
		int iTotal = 0;
		int iNumOfRecv = 0;
	};

	CMonitoringNetServer			*m_pMonitoringNetServer;
	
	HANDLE							m_hDBSendThreadHandle;
	HANDLE							m_hSendEventHandle;

	CRITICAL_SECTION				m_csUnitData[dfNUM_OF_DATATYPE];
	st_UnitItemInfo					m_UnitValue[dfNUM_OF_DATATYPE];

	st_ServerLoginInfo				m_ServerLoginInfo[dfNUM_OF_SERVER];
};