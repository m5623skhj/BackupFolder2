#pragma once
#include "LanClient.h"
#include "CPUUsageObserver.h"
#include <pdh.h>
#include <pdhmsg.h>
#include <strsafe.h>

#pragma comment(lib, "pdh.lib")

#define dfSESSIONKEY_SIZE			64

#define dfDIVIDE_KILOBYTE			1024
#define dfDIVIDE_MEGABYTE			1048576

class CMMOMonitoringLanClient : CLanClient
{
public:
	CMMOMonitoringLanClient();
	virtual ~CMMOMonitoringLanClient();

	bool MonitoringLanClientStart(const WCHAR *szOptionFileName, UINT *pNumOfSessionAll,
		UINT *NumOfSessionAuth, UINT *NumOfSessionGame, UINT *AuthFPS, UINT *GameFPS,
		UINT *NumOfRoomWait, UINT *NumOfRoomPlay);
	void MonitoringLanClientStop();

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

	static UINT __stdcall	MonitoringInfoSendThread(LPVOID MonitoringClient);
	UINT __stdcall			MonitoringInfoSender();

	// 모니터링 정보 송신용
	void SendGameServerOn(int TimeStamp);
	void SendGameCPU(int TimeStamp);
	void SendGameMemoryCommit(int TimeStamp);
	void SendGamePacketPool(int TimeStamp);
	void SendGameAuthFPS(int TimeStamp);
	void SendGameGameFPS(int TimeStamp);
	void SendGameSessionAll(int TimeStamp);
	void SendGameAuthSession(int TimeStamp);
	void SendGameGameSession(int TimeStamp);
	void SendGameRoomWait(int TimeStamp);
	void SendGameRoomPlay(int TimeStamp);

private:
	UINT							*m_pAuthFPS;
	UINT							*m_pGameFPS;
	UINT							*m_pNumOfSessionAll;
	UINT							*m_pNumOfSessionAuth;
	UINT							*m_pNumOfSessionGame;
	UINT							*m_pNumOfRoomWait;
	UINT							*m_pNumOfRoomGame;

	HANDLE							m_hMonitoringThreadHandle;
	CCPUUsageObserver				m_CPUObserver;

	PDH_HQUERY						m_PdhQuery;
	PDH_HCOUNTER					m_PrivateByte;
};