#pragma once
#include "Session.h"

class CEchoPlayer : public CSession
{
public :
	CEchoPlayer();
	~CEchoPlayer();

private :
	// 인증 스레드에 유저가 접속함
	virtual void OnAuth_ClientJoin();
	// 인증 스레드에서 유저가 나감
	virtual void OnAuth_ClientLeave(bool IsClientLeaveServer);
	// 인증 스레드에 있는 유저가 패킷을 받음
	virtual void OnAuth_Packet(CNetServerSerializationBuf *pRecvPacket);
	// 게임 스레드에 유저가 접속함
	virtual void OnGame_ClientJoin();
	// 게임 스레드에 유저가 나감
	virtual void OnGame_ClientLeave();
	// 게임 스레드에 있는 유저가 패킷을 받음
	virtual void OnGame_Packet(CNetServerSerializationBuf *pRecvPacket);
	// 최종적으로 유저가 릴리즈 됨
	virtual void OnGame_ClientRelease();

private :
	bool							m_bIsLoginPlayer;
	UINT64							m_uiAccountNo;

};