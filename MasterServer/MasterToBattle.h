#pragma once

#include "LanServer.h"

#include <list>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

using namespace std;

#define dfSIZE_BATTLE_ON				132
#define dfSIZE_BATTLE_REISSUE_TOKEN		36
#define dfSIZE_CREATE_ROOM				48
#define dfSIZE_CLOSE_ROOM				8
#define dfSIZE_LEFT_USER				16

#define dfSIZE_CONNECT_TOKEN			32
#define dfPRIORITY_QUEUE_DEFAULT_SIZE	5000

struct st_CLIENT_INFO;

class CSerializationBuf;
class CMasterServer;

class CMasterToBattleServer : CLanServer
{
	friend CMasterServer;

public:
	CMasterToBattleServer();
	~CMasterToBattleServer();

	bool MasterToBattleServerStart(const WCHAR *szMasterNetServerFileName, CMasterServer *pCMasterServer);
	void MasterToBattleServerStop();

	int GetRoomInfoMemoryPoolChunkAllocSize();
	int GetRoomInfoMemoryPoolChunkUseSize();
	int GetRoomInfoMemoryPoolBufUseSize();
	int GetBattleStanbyRoomSize();

public:
	UINT								NumOfBattleServerSessionAll;
	UINT								NumOfBattleServerLoginSession;

private:
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

	// Master NetServer 옵션 파싱 함수
	bool MasterToBattleServerOptionParsing(const WCHAR *szOptionFileName);

	// ----------------------------------------------------------------------------------------
	// MastertToBattle 프로시저 함수
	void MasterToBattleServerOnProc(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf);
	void MasterToBattleConnectTokenProc(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf);
	void MasterToBattleLeftUserProc(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf);
	void MasterToBattleCreatedRoomProc(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf);
	void MasterToBattleClosedRoomProc(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf);
	// ----------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------
	// MatchToMaster 프로시저 함수
	// 아래 함수들을 호출할 경우 마스터 서버에서 반드시 인증을 거치고 올 것
	void MatchToMasterRequireRoom(CSerializationBuf *OutSendBuf);
	void MatchToMasterEnterRoomSuccess(UINT64 ClientKey, int BattleServerNo, int BattleRoomNo);
	void MatchToMasterEnterRoomFail(UINT64 ClientKey, UINT64 BattleRoomInfo);
	// ----------------------------------------------------------------------------------------

private:
	// 마스터에게 붙을 배틀서버들의 정보
	struct st_BATTLE_SERVER_INFO
	{
		st_BATTLE_SERVER_INFO()
		{
			InitializeSRWLock(&ConnectTokenSRWLock);
		}

		WCHAR			ServerIP[16];
		WORD			wPort;
		char			ConnectTokenNow[32];
		char			ConnectTokenBefore[32];
		WCHAR			ChatServerIP[16];
		WORD			wChatServerPort;
		int				iServerNo;

		SRWLOCK			ConnectTokenSRWLock;
	};

	// 방 정보
	struct st_ROOM_INFO
	{
		int						iBattleServerNo;
		int						iRoomNo;
		//int						iRemainingUser;
		int						iMaxUser;
		char					EnterToken[32];
		// UserClientKey 셋을 위한 락
		//CRITICAL_SECTION		UserSetLock;
		// 유저가 해당 방에 있다는 것을 확인하기 위한 셋
		set<UINT64>				UserClientKeySet;
	};

	// m_RoomInfoPriorityQueue 의 정렬 방식
	//struct st_ROOM_INFO_COMPARE
	//{
	//	bool operator() (st_ROOM_INFO *lhs, st_ROOM_INFO *rhs)
	//	{
	//		if (lhs->iRemainingUser > rhs->iRemainingUser)
	//			return true;
	//		else
	//			return false;
	//	}
	//};
	
	// 들어온 배틀 서버가 인증이 된 상태인지를 확인하는 맵
	// Key : ClientID, Value : st_MATCHMAKING_CLIENT
	map<UINT64, st_BATTLE_SERVER_INFO*>											m_BattleServerInfoMap;
	// m_BattleServerInfoMap 전용 SRWLock
	SRWLOCK																		m_BattleServerInfoMapSRWLock;
	// m_BattleServerInfoMap 전용 Critical Section
	//CRITICAL_SECTION															m_csBattleServerInfoLock;
	// BATTLE_CLIENT 전용 TLS 메모리 풀
	CTLSMemoryPool<st_BATTLE_SERVER_INFO*>										*m_pBattleServerInfoMemoryPool;
	
	// 관리하고 있는 st_ROOM_INFO 구조체들 중에서
	// 유저가 가장 빠르게 게임을 시작 할 수 있도록 하기 위하여
	// MaxUser 로 정렬 되어있는 Min Heap 을 사용함
	//priority_queue<st_ROOM_INFO*, vector<st_ROOM_INFO*>, st_ROOM_INFO_COMPARE>	m_RoomInfoPriorityQueue;
	//vector<st_ROOM_INFO*>														m_RoomInfoPriorityQueue;

	// 한 번도 방에 인원이 꽉 차지 않았던 방들에 대한 리스트
	// 이 리스트는 방의 생성 패킷을 받았을 때 해당 방을 push_back 하고
	// 방의 삭제에 대한 패킷을 받았을 때는 순회를 하며
	// 방에 인원이 꽉 찬다면 m_PlayStandbyRoom 에게 관리하던 포인터를 넘겨준다
	list<st_ROOM_INFO*>															m_RoomInfoList;
	// m_RoomInfoList 전용 Critical Section
	CRITICAL_SECTION															m_csRoomInfoLock;
	// ROOM_INFO 전용 TLS 메모리 풀
	CTLSMemoryPool<st_ROOM_INFO>												*m_pRoomInfoMemoryPool;
	// 마스터 서버가 판단했을 때 방에 더 들어갈 수 있는 인원이 없다면
	// m_PlayStandbyRoom 에서 빼고 m_PlayStandbyRoom 에 집어넣어
	// 더이상 방에 인원이 추가되지 못하도록 함
	// 이후 실제로 배틀서버에서 해당 방에 대한 게임 시작 패킷이 오면
	// 이 m_PlayStandbyRoomMap 내부의 실객체를 반환한다
	// 이러한 과정을 거치는 이유는 패킷을 보내고 바로 방을 파괴하고
	// 이후 배틀에서 패킷이 도착하기 이전에 인원이 빠져나간다면
	// 마스터에서는 파괴되었지만 배틀에서는 1 명이 빈 채로 영원히 방이 시작을 못하게 됨
	// 이 자료구조에 포인터가 넘어왔다면 인원이 빠져나가도 m_PlayStandbyRoom 로 돌아가지 않으며
	// 때문에 인원이 들어올 때 이 맵 부터 순회하여 우선적으로 인원을 채운다
	// 이후 속도가 느리다고 판단된다면 인원이 빠져있는 맵 노드들의 RoomNo 를 관리하는
	// list 를 두어서 맨 앞에 있는 것만 뽑아내서 맵을 꽉 채우게 유도 해 보는것도 고려할 것
	// Key : RoomNo, Value : st_ROOM_INFO*
	map<UINT64, st_ROOM_INFO*>													m_PlayStandbyRoomMap;
	// m_PlayStandbyRoomMap 전용 Critical Section
	CRITICAL_SECTION															m_csPlayStandbyRoomLock;

	// 배틀 서버 혹은 매치메이킹 서버에 있는 클라이언트들을 관리하는 맵
	// Key : AccountNo, Value : st_CLIENT_INFO
	//unordered_map<UINT64, st_CLIENT_INFO*>										m_ClientInfoMapFindAccountNo;
	// m_ClientInfoMapFindClientKey 전용 Critical Section
	//CRITICAL_SECTION															m_csClientInfoMapFindAccountNo;

	// 마스터 서버에서 정해주는 배틀 서버의 번호
	UINT																		m_iBattleServerNo;

	// 마스터 서버와 통신하기 위한 포인터
	CMasterServer																*m_pMasterServer;

};