#pragma once

#include "NetServer.h"
#include <unordered_map>

#include "NetServerSerializeBuffer.h"
#include "LanServerSerializeBuf.h"

using namespace std;

//class CNetServerSerializationBuf;
//class CSerializationBuf;
class CMatchmakingLanClient;
class CMatchmakingMonitoringClient;

class CMatchmakingServer : CNetServer
{
	friend CMatchmakingLanClient;

public:
	CMatchmakingServer();
	virtual ~CMatchmakingServer();

	bool MatchmakingServerStart(const WCHAR *szMatchmakingNetServerOptionFile, const WCHAR *szMatchmakingLanClientOptionFile, const WCHAR *szMatchmakingMonitoringClientOptionFile);
	void MatchmakingServerStop();

	//----------------------------------------------------------------------------
	// main 문에서 출력할 정보들
	UINT GetLoginSuccessTPSAndReset();
	UINT GetAcceptTPSAndReset();
	UINT GetNumOfTotalAccept() { return m_uiUserNoCount; }

	int GetUserInfoAlloc() { return m_pUserMemoryPool->GetAllocChunkCount(); }
	int GetUserInfoUse() { return m_pUserMemoryPool->GetUseChunkCount(); }
	int GetNumOfAllocLanSerializeChunk() { return CSerializationBuf::GetAllocSerializeBufChunkCount(); }
	int GetNumOfUsingLanSerializeBuf() { return CSerializationBuf::GetUsingSerializeBufNodeCount(); }
	int GetNumOfUsingLanSerializeChunk() { return CSerializationBuf::GetUsingSerializeBufChunkCount(); }
	int GetNumOfAllocSerializeChunk() { return CSerializationBuf::GetAllocSerializeBufChunkCount(); }
	int GetNumOfUsingSerializeBuf() { return CSerializationBuf::GetUsingSerializeBufNodeCount(); }
	int GetNumOfUsingSerializeChunk() { return CSerializationBuf::GetUsingSerializeBufChunkCount(); }
	//----------------------------------------------------------------------------

public :
	// 해당 서버의 유저들 중 몇 명이 로그인에 성공하였는지에 대한 값
	UINT											m_uiNumOfLoginCompleteUser;

	// 해당 서버에 몇 명의 유저가 접속 되어있는지에 대한 값
	UINT											m_uiNumOfConnectUser;
	
private :
	// Accept 후 접속처리 완료 후 호출
	virtual void OnClientJoin(UINT64 ClientID/* Client 정보 / ClientID / 기타등등 */);
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
	
	// shDBSendThread 를 호출시키기 위한 static 함수
	static UINT __stdcall	shDBSendThreadAddress(LPVOID MatchmakingNetServer);
	// 매치메이킹 서버의 정보를 DB 에 보낼 스레드
	UINT shDBSendThread();

	// 마스터 서버에게 방 정보를 요구하기 위해 호출하는 함수
	// MatchmakingNetServer 에서만 호출이 가능하도록 private 함수로 만듦
	void SendToClient_GameRoom(CSerializationBuf *pRecvBuf);

	// Matchmaking NetServer 옵션 파싱 함수
	bool MatchmakingNetServerOptionParsing(const WCHAR *szOptionFileName);

	// ---------------------------------------------
	// 프로시저 함수
	// ---------------------------------------------
	void LoginProc(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf);

private :
	// Send Thread 가 지니고 있는 핸들들의 값
	enum en_SEND_THREAD_EVENT
	{
		// 설정 파일에서 지정해 놓은 값 이상으로 인원을 받게 되었다면 
		// 이 핸들을 시그널 상태로 바꿔주고 
		// shDBSendThread 가 한 루프를 실행하게 된다
		en_USER_CONNECT_MAX_IN_DB_SAVE_TIME = 0,

		// shDBSendThread 의 종료 이벤트 핸들
		en_SEND_THREAD_EXIT,
	};

	enum en_USER_STATUS
	{
		// 유저가 처음 들어왔을 때 상태
		en_STATUS_CREATE = 0,
		// 유저가 로그인 절차를 끝낸 상태
		en_STATUS_LOGIN_COMPLETE,
		// 유저가 방을 요청중인 상태
		en_STATUS_ACQUIRE_ROOM,
		// 유저가 배틀서버로 가는 상태
		en_STATUS_GO_TO_BATTLE,
		// 유저가 배틀서버로 들어가있는 상태
		en_STATUS_ENTER_BATTLE_COMPLETE,
	};

	struct st_USER
	{
		st_USER()
		{
			byStatus = en_STATUS_CREATE;
			uiClientKey = -1;
			iAccountNo = -1;
		}
		BYTE										byStatus;
		// 매치메이킹 서버에 의하여 만들어진 클라이언트 개인마다의 고유값
		UINT64										uiClientKey;
		// 클라이언트가 가지고 오는 유저 식별 번호
		// 단, 이 값으로 유저를 식별하게된다면
		// 유저가 재접속시 중복된 번호를 갖게 될 수 있음
		// => ex) 유저 1번이 요청 한 후 응답을 받기 전에 재접속함
		// => 새로 들어와도 그 유저는 1번이지만 요청하지 않은 응답을 받게 될 수 있음
		INT64										iAccountNo;
	};

	// st_USER 의 메모리 풀
	CTLSMemoryPool<st_USER>							*m_pUserMemoryPool;

	// 현재 해당 서버에 접속 되어있는 유저들을 관리하는 map
	// key 는 worker thread 에서 오는 ClientID 값임
	unordered_map<UINT64, st_USER*>					m_UserMap;

	// m_UserMap 전용 Critical Section
	CRITICAL_SECTION								m_csUserMapLock;

	// Lan 에서 Net 으로 정보들을 넘길 때 해당 패킷이 누구꺼인지를 알 필요가 있음
	// 따라서 해당 맵을 사용하여 누구의 패킷인지를 판단함
	// Key : ClientKey , Value : SessionID
	unordered_map<UINT64, UINT64>					m_SessionIDMap;

	// m_UserMap 전용 Critical Section
	CRITICAL_SECTION								m_csSessionMapLock;
		
	// DB 에 몇 ms 마다 저장할 것인가에 대한 값 (미리 세컨드 단위)
	WORD											m_wshDBSaveTime;

	// shDBSaveTime 동안 최대로 들어갈 수 있는 유저의 수
	// 시간내에 들어온 인원수가 이 값 보다 많아질 경우 
	// shDBSendThread 에서 추가로 한 번 더 DB 에 쿼리를 보냄
	WORD											m_wUserConnectMaxInshDBSaveTime;

	// 해당 서버의 DB No
	int 											m_wServerNo;

	// 각 매치메이킹 서버마다 지니는 고유한 값
	// 클라이언트가 이 값을 들고 해당 매치메이킹 서버로 오며
	// 그것을 비교하여 이 서버로 온 클라이언트가 맞는지 확인함
	// 인증 성공시 ClientKey 를 발행 할 때 사용될 값
	// ClientKey 8 바이트 중에서 상위 4 바이트에 추가하여
	// 모든 서버에 대하여 고유한 값을 생성해냄
	UINT											m_uiServerVersionCode;

	// 유저들이 들어올 때 하나씩 증가하는 값
	// 인증 성공시 ClientKey 를 발행 할 때 사용될 값
	// ClientKey 8 바이트 중에서 하위 4 바이트에 추가하여
	// 모든 서버에 대하여 고유한 값을 생성해냄
	UINT											m_uiUserNoCount;

	// 해당 서버에서 1 초 동안 몇 명의 유저가 로그인에 성공하였는지에 대한 값
	UINT											m_uiLoginSuccessTPS;

	// 해당 서버에서 1 초 동안 몇 명의 유저가 이 서버에 접속하였는지에 대한 값
	UINT											m_uiAcceptTPS;

	// 마스터 서버와의 통신을 위한 LanClient
	// 이 포인터를 통하여 private 멤버 함수인 
	// SendToMaster_GameRoom 와 SendToMaster_GameRoomEnter 에 접근한다
	CMatchmakingLanClient							*m_pMatchmakingLanclient;

	// 모니터링 서버와의 통신을 위한 MonitoringLanClient
	CMatchmakingMonitoringClient					*m_pMatchmakingMonitoringClient;
	
	// shDBSendThread 루프 내의 이벤트 핸들
	// en_SEND_THREAD_EVENT 에 각 핸들의 넘버가 정의 되어있음
	HANDLE											m_hSendEventHandle[2];

	// shDBSendThread 의 핸들
	HANDLE											m_hDBSendThreadHandle;

	// ---------------------------------------------------------------------------
	// 서버 정보
	WORD											m_wServerPort;
	WCHAR											m_ServerIP[16];
	// ---------------------------------------------------------------------------

	// ---------------------------------------------------------------------------
	// DB 컨넥트 정보
	WORD											m_wDBPort;
	WCHAR											m_DBIP[16];
	WCHAR											m_DBUserName[32];
	WCHAR											m_DBPassWord[32];
	WCHAR											m_DBName[32];
	// ---------------------------------------------------------------------------

};