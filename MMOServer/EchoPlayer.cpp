#include "PreCompile.h"
#include "EchoPlayer.h"
#include "NetServerSerializeBuffer.h"
#include "CommonProtocol.h"

CEchoPlayer::CEchoPlayer() : m_bIsLoginPlayer(false)
{

}

CEchoPlayer::~CEchoPlayer()
{

}

void CEchoPlayer::OnAuth_ClientJoin()
{

}

void CEchoPlayer::OnAuth_ClientLeave(bool IsClientLeaveServer)
{

}

void CEchoPlayer::OnAuth_Packet(CNetServerSerializationBuf *pRecvPacket)
{
	CNetServerSerializationBuf &RecvBuf = *pRecvPacket;

	WORD Type;
	RecvBuf >> Type;

	if (Type == en_PACKET_TYPE::en_PACKET_CS_GAME_REQ_LOGIN)
	{
		char SessionKey[64];
		
		RecvBuf >> m_uiAccountNo;
		RecvBuf.ReadBuffer(SessionKey, sizeof(SessionKey));

		m_bIsLoginPlayer = true;

		CNetServerSerializationBuf &SendBuf = *CNetServerSerializationBuf::Alloc();

		Type = en_PACKET_TYPE::en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Status = 1;

		SendBuf << Type << Status << m_uiAccountNo;

		//CNetServerSerializationBuf::AddRefCount(&SendBuf);
		SendPacket(&SendBuf);

		//CNetServerSerializationBuf::Free(&SendBuf);

		ChangeMode_AuthToGame();
	}
	else
	{
		g_Dump.Crash();
	}
}

void CEchoPlayer::OnGame_ClientJoin()
{

}

void CEchoPlayer::OnGame_ClientLeave()
{

}

void CEchoPlayer::OnGame_Packet(CNetServerSerializationBuf *pRecvPacket)
{
	CNetServerSerializationBuf &RecvBuf = *pRecvPacket;

	WORD Type;
	RecvBuf >> Type;

	if (Type == en_PACKET_TYPE::en_PACKET_CS_GAME_REQ_ECHO)
	{
		INT64 AccountNo;
		LONGLONG SendTick;

		RecvBuf >> AccountNo >> SendTick;
		if (m_uiAccountNo != AccountNo)
		{
			DisConnect();
			return;
		}

		CNetServerSerializationBuf &SendBuf = *CNetServerSerializationBuf::Alloc();

		Type = en_PACKET_TYPE::en_PACKET_CS_GAME_RES_ECHO;

		SendBuf << Type << AccountNo << SendTick;

		//CNetServerSerializationBuf::AddRefCount(&SendBuf);
		SendPacket(&SendBuf);

		//CNetServerSerializationBuf::Free(&SendBuf);
	}
	else
	{
		g_Dump.Crash();
	}
}

void CEchoPlayer::OnGame_ClientRelease()
{
	m_bIsLoginPlayer = false;
}