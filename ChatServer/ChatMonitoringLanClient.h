#pragma once
#include "LanClient.h"
#include "ChatServer.h"
#include "CPUUsageObserver.h"
#include <pdh.h>
#include <pdhmsg.h>
#include <strsafe.h>

#pragma comment(lib, "pdh.lib")

#define dfSESSIONKEY_SIZE			64

#define dfDIVIDE_KILOBYTE			1024
#define dfDIVIDE_MEGABYTE			1048576

class CChatMonitoringLanClient : CLanClient
{
public:
	CChatMonitoringLanClient();
	virtual ~CChatMonitoringLanClient();

	bool MonitoringLanClientStart(const WCHAR *szOptionFileName, UINT *pNumOfSessionAll, UINT *pNumOfLoginCompleteUser);
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
	void SendChatServerOn(int TimeStamp);
	void SendChatCPU(int TimeStamp);
	void SendChatMemoryCommit(int TimeStamp);
	void SendChatPacketPool(int TimeStamp);
	void SendChatSession(int TimeStamp);
	void SendChatPlayer(int TimeStamp);
	void SendChatRoom(int TimeStamp);

	// 매치 메이킹 서버로 넘어가야 모니터링 정보 송신용 함수
	// 매치 메이킹 제작 이후 매치 메이킹 서버로 해당 함수를 옮길것
	void SendServerCPUTotal(int TimeStamp);
	void SendServerAvailableMemroy(int TimeStamp);
	void SendServerNetworkRecv(int TimeStamp);
	void SendServerNetworkSend(int TimeStamp);
	void SendServerNonpagedMemory(int TimeStamp);


private:
	UINT							*m_pNumOfSessionAll;
	UINT							*m_pNumOfLoginCompleteUser;

	HANDLE							m_hMonitoringThreadHandle;
	CCPUUsageObserver				m_CPUObserver;
	
	PDH_HQUERY						m_PdhQuery;
	PDH_HCOUNTER					m_WorkingSet;
	
	// 매치 메이킹 이전용 임시 카운터 변수
	PDH_HCOUNTER					m_ProcessorUsage;
	PDH_HCOUNTER					m_AvailableMemory;
	PDH_HCOUNTER					m_NetworkRecv;
	PDH_HCOUNTER					m_NetworkSend;
	PDH_HCOUNTER					m_NonPagedMemory;
};