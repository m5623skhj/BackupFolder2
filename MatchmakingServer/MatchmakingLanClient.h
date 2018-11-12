#pragma once

#include "LanClient.h"

class CSerializationBuf;
class CMatchmakingServer;

class CMatchmakingLanClient : CLanClient
{
	friend CMatchmakingServer;

public :
	CMatchmakingLanClient();
	virtual ~CMatchmakingLanClient();

	bool MatchmakingLanClientStart(CMatchmakingServer *pMatchmakingNetServer,int ServerNo, const WCHAR *szMatchmakingLanServerOptionFile);
	void MatchmakingLanClientStop();

private :
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

	// 마스터 서버에게 방 정보를 요구하기 위해 호출하는 함수
	// MatchmakingNetServer 에서만 호출이 가능하도록 private 함수로 만듦
	void SendToMaster_GameRoom(UINT64 ClientKey, UINT64 AccountNo);
	// 마스터 서버에게 유저가 방에 들어갔음을 알려주기 위해 호출하는 함수
	// MatchmakingNetServer 에서만 호출이 가능하도록 private 함수로 만듦
	void SendToMaster_GameRoomEnterSuccess(WORD BattleServerNo, int RoomNo, UINT64 ClientKey);
	// 마스터 서버에게 유저가 방에 접속을 실패 하였다는 것을 알려주기 위해 호출하는 함수
	// MatchmakingNetServer 에서만 호출이 가능하도록 private 함수로 만듦
	void SendToMaster_GameRoomEnterFail(UINT64 ClientKey);

	// Matchmaking LanClient 옵션 파싱 함수
	bool MatchmakingLanClientOptionParsing(const WCHAR *szOptionFileName);

private :
	// 해당 서버가 마스터 서버와 연결 되었는지에 대한 값
	// MatchmakingNetServer 에서는 이 값을 통하여 사용자들을 어떻게 처리할지 판단한다
	bool											m_bConnectMasterServer;

	// 해당 서버의 서버 번호
	// 이 값은 Matchmaking NetServer 에서 넘겨주는 것을 사용함
	int												m_iServerNo;

	// 마스터 서버에 연결하기 위한 토큰
	// 상호 합의 하에 작성된 값이며 서버에 접속하기 위한 인증과정에서 사용됨
	char											m_MasterToken[32];

	// Matchmaking NetServer 포인터
	// 이 값은 MatchmakingLanClientStart 의 인자로 들어옴
	// private 멤버 함수로 되어있는 
	// SendToClient_GameRoom 와 SendToClient_GameRoomEnter 에 접근한다
	CMatchmakingServer								*m_pMatchmakingNetServer;
};
