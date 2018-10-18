#pragma once
#include "Session.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"

// Post 함수의 에러로 인하여 세션이 이미 종료 처리됨
#define POST_RETVAL_ERR_SESSION_DELETED		0
// Post 함수에서 에러가 발생하였고 세션은 아직 살아있는 상태임
#define POST_RETVAL_ERR						1
// Post 함수가 에러 없이 처리되었음
#define POST_RETVAL_COMPLETE				2

#define dfNUM_OF_THREAD						5

struct st_Error;

class CNetServerSerializationBuf;

class CMMOServer
{
public :
	CMMOServer();
	virtual ~CMMOServer();

	UINT GetAcceptTPSAndReset();
	UINT GetRecvTPSAndReset();
	UINT GetSendTPSAndReset();
	UINT GetNumOfUserInfoAlloc()
	{
		return m_pUserInfoMemoryPool->GetAllocChunkCount();
	}
	UINT GetUserInfoUse()
	{
		return m_pUserInfoMemoryPool->GetUseNodeCount();
	}

	UINT GetSendThreadFPSAndReset();
	UINT GetAuthThreadFPSAndReset();
	UINT GetGameThreadFPSAndReset();

	UINT *GetAuthFPSPtr() { return &AuthThreadLoop; }
	UINT *GetGameFPSPtr() { return &GameThreadLoop; }
	UINT *GetNumOfAllSession() { return (UINT*)&NumOfTotalPlayer; }
	UINT *GetNumOfAuthSession() { return &NumOfAuthPlayer; }
	UINT *GetNumOfGameSession() { return &NumOfGamePlayer; }
	UINT *GetNumOfWaitRoom() { return NULL; }
	UINT *GetNumOfPlayRoom() { return NULL; }

	int GetRecvCompleteQueueTotalNodeCount();

public :
	UINT						NumOfAuthPlayer;
	UINT						NumOfGamePlayer;
	UINT64						NumOfTotalPlayer;

	UINT64						NumOfTotalAccept;

	UINT						RecvTPS;
	UINT						SendTPS;
	UINT						AcceptTPS;

	UINT						SendThreadLoop;
	UINT						AuthThreadLoop;
	UINT						GameThreadLoop;
	
protected :
	bool Start(const WCHAR *szMMOServerOptionFileName);
	void Stop();

	/////////////////////////////////////////////
	// MMOServer Start 함수 호출 이전에 반드시 호출 해 줘야함
	// 세션과의 연동을 위해서 다음 수순으로 함수를 호출해야 함
	// InitMMOServer -> 각 플레이어들 LinkSession -> CMMOServer::Start
	/////////////////////////////////////////////
	bool InitMMOServer(UINT NumOfMaxClient);

	// 인증 스레드의 한 프레임마다 호출됨
	virtual void OnAuth_Update() = 0;
	// 업데이트 스레드의 한 프레임마다 호출됨
	virtual void OnGame_Update() = 0;
	// 에러 상황 발생시 호출 됨
	virtual void OnError(st_Error *pError) = 0;

	////////////////////////////////////////////////////////////////////
	// 바깥에서 미리 Session 를 상속받은 객체가 만들어져 있는 상태에서
	// MMOServer 와 연결시키기 위하여 모든 객체들에 대해
	// 낱개로 호출되어야 함
	// 초기화시에만 호출 해 주고 이후로는 호출 하지 않음
	// 이 함수는 MMOServer 를 상속받은 객체가 생성된 후
	// Start 함수를 호출하기 이전에 반드시 실행 시켜줘야 함
	// 반복문으로 호출하길 권장하며 0 ~ NumOfMaxClient - 1 까지를 호출 해 줘야 함
	////////////////////////////////////////////////////////////////////
	void LinkSession(CSession *PlayerPointer, UINT PlayerIndex);

private :
	/////////////////////////////////////////////
	// Accept Thread
	// accept 함수만을 호출하여 사용자와 연결한 이후
	// Accept 완료 큐에 넣어줌
	/////////////////////////////////////////////
	static UINT __stdcall AcceptThreadStartAddress(LPVOID CMMOServerPointer);
	UINT AcceptThread();

	/////////////////////////////////////////////
	// Worker Thread
	// Recv 완료통지 시 검증을 끝낸 후 
	// CompleteRecvPacket 에 담아줌
	// Send 완료통지 시 Send 가 완료된 패킷들을 지워줌
	// IOCount 가 0 이 되어도 바로 삭제하지 않고 플래그를 변경함으로써
	// 외부에서 지우게 유도함
	/////////////////////////////////////////////
	static UINT __stdcall WorkerThreadStartAddress(LPVOID CMMOServerPointer);
	UINT WorkerThread();

	/////////////////////////////////////////////
	// Send Thread
	// 일정 주기마다 깨어나서 
	// 모든 세션에 대하여 보낼것이 존재한다면
	// WSASend 를 요청함
	/////////////////////////////////////////////
	static UINT __stdcall SendThreadStartAddress(LPVOID CMMOServerPointer);
	UINT SendThread();
	
	/////////////////////////////////////////////
	// Auth Thread
	// 일정 주기마다 깨어나서
	// 인증 과정을 돌림
	/////////////////////////////////////////////
	static UINT __stdcall AuthThreadStartAddress(LPVOID CMMOServerPointer);
	UINT AuthThread();

	/////////////////////////////////////////////
	// Game Thread
	// 일정 주기마다 깨어나서
	// 게임 로직을 돌림
	/////////////////////////////////////////////
	static UINT __stdcall GameThreadStartAddress(LPVOID CMMOServerPointer);
	UINT GameThread();

	/////////////////////////////////////////////
	// Release Thread
	// 일정 주기마다 깨어나서
	// SessionMode 가 로그아웃 대기중이면 그 세션을 초기화 함
	/////////////////////////////////////////////
	static UINT __stdcall ReleaseThreadStartAddress(LPVOID CMMOServerPointer);
	UINT ReleaseThread();

	char RecvPost(CSession *pSession);

	bool OptionParsing(const WCHAR *szMMOServerOptionFileName);

private :
	// 스레드 종료 이벤트 핸들 번호
	enum en_EVENT_NUMBER
	{
		EVENT_SEND = 0,
		EVENT_AUTH,
		EVENT_GAME,
		EVENT_RELEASE,
		NUM_OF_EVENT
	};

	BYTE												m_byNumOfWorkerThread;
	BYTE												m_byNumOfUsingWorkerThread;
	BOOL												m_bIsNagleOn;
	WORD												m_wSendThreadSleepTime;
	WORD												m_wAuthThreadSleepTime;
	WORD												m_wGameThreadSleepTime;
	WORD												m_wReleaseThreadSleepTime;
	WORD												m_wLoopAuthHandlingPacket;
	WORD												m_wLoopGameHandlingPacket;

	WORD												m_wPort;

	// 한 루프에 Send를 시도하는 최대 횟수
	WORD												m_wNumOfSendProgress;
	// 한 루프에 최대로 연결하는 최대 개수
	WORD												m_wNumOfAuthProgress;
	// 한 루프에 게임 스레드로 넘어오는 세션의 최대 개수
	WORD												m_wNumOfGameProgress;

	UINT												m_uiSendThreadLoopStartValue;

	UINT												m_uiNumOfMaxClient;
	UINT												m_uiNumOfUser;
	SOCKET												m_ListenSock;
	WCHAR												m_IP[16];

	HANDLE												*m_pWorkerThreadHandle;
	HANDLE												m_hAcceptThread;
	HANDLE												m_hSendThread[2];
	HANDLE												m_hAuthThread;
	HANDLE												m_hGameThread;
	HANDLE												m_hReleaseThread;

	HANDLE												m_hWorkerIOCP;

	// 스레드 종료용 이벤트 배열
	// 각 번호는 en_EVENT_NUMBER 에 정의 되어있음
	HANDLE												m_hThreadExitEvent[en_EVENT_NUMBER::NUM_OF_EVENT];

	// 세션 정보 스택
	//CLockFreeStack<CSession::AcceptUserInfo*>			m_SessionInfoStack;
	CLockFreeStack<WORD>								m_SessionInfoStack;
	// Accept 된 유저들의 세션 정보를 집어넣을 큐
	CLockFreeQueue<CSession::AcceptUserInfo*>			m_AcceptUserInfoQueue;

	CTLSMemoryPool<CSession::AcceptUserInfo>			*m_pUserInfoMemoryPool;
	
	// 세션 포인터 배열
	// 외부 유저들의 포인터를 직접 배열에 넣어줘야함
	CSession											**m_ppSessionPointerArray;
};