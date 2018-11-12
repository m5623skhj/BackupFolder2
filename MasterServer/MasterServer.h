#pragma once

#define dfSIZE_SERVER_ON					36
#define dfSIZE_GAME_ROOM					16
#define dfSIZE_ENTER_SUCCESS				14
#define dfSIZE_ENTER_FAIL					10

#define dfSHIFT_RIGHT_SERVERNO				32

#define dfRESERVER_CLIENTKEYMAP_SIZE		8192

#include "LanServer.h"

#include <unordered_map>
#include <map>

using namespace std;

class CNetServerSerializationBuf;
class CSerializationBuf;
class CMasterToBattleServer;
class CMasterMonitoringClient;

class CMasterServer : CLanServer
{
	friend CMasterToBattleServer;

public :
	CMasterServer();
	~CMasterServer();

	bool MasterServerStart(const WCHAR *szMasterLanServerFileName, const WCHAR *szMasterToBattleServerFileName,
		const WCHAR *szMasterMonitoringClientFileName);
	void MasterServerStop();

	int GetNumOfClientInfo();
	int GetUsingLanServerSessionBufCount();
	int GetUsingLanSerializeChunkCount();
	int GetAllocLanSerializeBufCount();

	// MasterToBattle 에 있는 함수 콜 용
	int CallGetRoomInfoMemoryPoolChunkAllocSize();
	int CallGetRoomInfoMemoryPoolChunkUseSize();
	int CallGetRoomInfoMemoryPoolBufUseSize();
	int CallGetBattleStanbyRoomSize();

public :
	UINT								NumOfMatchmakingSessionAll;
	UINT								NumOfMatchmakingLoginSession;
	UINT								*pNumOfBattleServerSessionAll;
	UINT								*pNumOfBattleServerLoginSession;

private :
	// Accept 후 접속처리 완료 후 호출
	virtual void OnClientJoin(UINT64 ClientID/* Client 정보 / ClientID / 기타등등 */);
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

	// Master LanServer 옵션 파싱 함수
	bool MasterLanServerOptionParsing(const WCHAR *szOptionFileName);

private :
	struct st_MATCHMAKING_CLIENT
	{
		st_MATCHMAKING_CLIENT()
		{
			bIsAuthorized = false;
			iMatchmakingServerNo = -1;
		}

		bool				bIsAuthorized;
		int					iMatchmakingServerNo;
	};

	//struct st_CLIENT_INFO
	//{
	//	int					iBattleServerNo;
	//	int					iRoomNo;
	//	UINT64				uiClientKey;
	//	INT64				iAccountNo;
	//};

	// 들어온 매치메이킹 서버가 인증이 된 상태인지를 확인하는 맵
	// 이 안에 최초 접속시 매치메이킹 서버가 주는 ServerNo 도 포함되어있음
	// Key : ClientID, Value : st_MATCHMAKING_CLIENT
	map<UINT64, st_MATCHMAKING_CLIENT>				m_AuthorizedSessionMap;
	// m_AuthorizedSessionMap 전용 SRWLock
	SRWLOCK											m_AuthorizedSessionMapSRWLock;
	
	// 배틀 서버 혹은 매치메이킹 서버에 있는 클라이언트들을 관리하는 맵
	// Key : ClientKey, Value : st_CLIENT_INFO
	//unordered_map<UINT64, st_CLIENT_INFO*>			m_ClientInfoMapFindClientKey;
	// key : ClientKey, Valued : 상위 4byte ServerNo / 하위 4byte RoomNo
	unordered_map<UINT64, UINT64>					m_ClientInfoMapFindClientKey;
	// st_CLIENT_INFO 전용 TLS 메모리 풀
	//CTLSMemoryPool<st_CLIENT_INFO>					*m_pClientInfoMemoryPool;
	// m_ClientInfoMapFindClientKey 전용 Critical Section
	CRITICAL_SECTION								m_csClientInfoMapFindClientKey;

	// 마스터 서버와 배틀 서버간의 통신을 위한 포인터
	CMasterToBattleServer							*m_pMasterToBattleServer;
	// 모니터링 정보를 전달하기 위한 포인터
	CMasterMonitoringClient							*m_pMasterMonitoringClient;

	// 배틀 서버나 매치메이킹 서버가 마스터 서버로 들어올 때
	// 인증을 위해 비교해야 할 토큰
	char											m_MasterToken[32];

};