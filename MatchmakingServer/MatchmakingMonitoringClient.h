#pragma once

#include "LanClient.h"
#include "CPUUsageObserver.h"
#include <pdh.h>
#include <pdhmsg.h>

#pragma comment(lib, "pdh.lib")

#define dfSESSIONKEY_SIZE			64

#define dfDIVIDE_KILOBYTE			1024
#define dfDIVIDE_MEGABYTE			1048576

class CSerializationBuf;

class CMatchmakingMonitoringClient : CLanClient
{
public:
	CMatchmakingMonitoringClient();
	virtual ~CMatchmakingMonitoringClient();

	bool MatchmakingMonitoringClientStart(const WCHAR *szMatchmakingMonitoringClientOptionFile, UINT *SessionAll, UINT *LoginPlayer);
	void MatchmakingMonitoringClientStop();

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

	// Matchmaking Monitoring LanClient 옵션 파싱 함수
	bool MatchmakingMonitoringClientOptionParsing(const WCHAR *szOptionFileName);

	// MonitoringInfoSender 를 호출시키기 위한 static 함수
	static UINT __stdcall	MonitoringInfoSendThread(LPVOID MonitoringClient);
	// 1 초 마다 일어나서 모니터링 정보를 송신하는 스레드
	UINT MonitoringInfoSender();

	// ---------------------------------------------------------------------------
	// 매치메이킹 서버 모니터링용 함수
	void SendMatchmakingServerOn(int TimeStamp);
	void SendMatchmakingCPU(int TimeStamp);
	void SendMatchmakingMemoryCommit(int TimeStamp);
	void SendMatchmakingPacketPool(int TimeStamp);
	void SendMatchmakingSessionAll(int TimeStamp);
	void SendMatchmakingLoginPlayer(int TimeStamp);
	void SendMatchmakingEnterRoomSuccessTPS(int TimeStamp);
	// ---------------------------------------------------------------------------

	// ---------------------------------------------------------------------------
	// 현재 서버의 모니터링용 함수
	void SendCPUTotal(int TimeStamp);
	void SendAvailableMemroy(int TimeStamp);
	void SendNetworkRecv(int TimeStamp);
	void SendNetworkSend(int TimeStamp);
	void SendNonpagedMemory(int TimeStamp);
	// ---------------------------------------------------------------------------

private :
	// 이 모니터링 서버의 고유 번호
	int								m_iServerNo;

	// 현재 이 매치메이킹 서버에 들어와있는 총 인원 수에 대한 포인터
	// Matchmaking 서버에서 포인터를 넘겨줌
	UINT							*m_pNumOfSessionAll;
	// 현재 이 매치메이킹 서버에서 로그인에 성공한 총 인원 수에 대한 포인터
	// Matchmaking 서버에서 포인터를 넘겨줌 
	UINT							*m_pNumOfLoginCompleteUser;

	// 모니터링 스레드를 중지시키기 위한 핸들
	HANDLE							m_hMonitoringThreadExitHandle;
	// 모니터링 스레드 핸들
	HANDLE							m_hMonitoringThreadHandle;
	// 매치메이킹 서버의 CPU 사용률 및 현재 서버의 총 CPU 사용률을 계산하기 위한 객체
	CCPUUsageObserver				m_CPUObserver;

	// ---------------------------------------------------------------------------
	// 이 매치메이킹 서버의 모니터링 정보를 넘겨주기 위하여 필요한 객체들
	PDH_HQUERY						m_PdhQuery;
	// 매치메이킹 전용
	PDH_HCOUNTER					m_PrivateBytes;
	// 이 서버 전체 계산 전용
	PDH_HCOUNTER					m_ProcessorUsage;
	PDH_HCOUNTER					m_AvailableMemory;
	PDH_HCOUNTER					m_NetworkRecv;
	PDH_HCOUNTER					m_NetworkSend;
	PDH_HCOUNTER					m_NonPagedMemory;
	// ---------------------------------------------------------------------------
};
