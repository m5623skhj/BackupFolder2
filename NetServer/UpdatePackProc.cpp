#include "PreComfile.h"
#include "ChatServer.h"
#include "NetServerSerializeBuffer.h"
#include "CommonProtocol.h"

/////////////////////////////////////////////////////////////////////
// Update ������ Packet �Լ� ���� cpp
// pMessage ���� Packet�� ���� Packet ���� Type �� ������ ���� ���·� ����
/////////////////////////////////////////////////////////////////////

bool CChatServer::PacketProc_SectorMove(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID)
{
	INT64 AccountNo;
	WORD SectorX, SectorY;
	
	*pRecvPacket >> AccountNo >> SectorX >> SectorY;

	auto &User = *m_UserSessionMap.find(SessionID)->second;
	if (&User == m_UserSessionMap.end()->second)
	{
		// ó�� �� �ٰ� ������ �ִ°�?
		return false;
	}

	if (m_UserSectorMap[User.SectorY][User.SectorX].erase(SessionID) == 0)
	{
		// ó�� �� �ٰ� ������ �ִ°�?
		return false;
	}
	User.SectorX = SectorX;
	User.SectorY = SectorY;
	m_UserSectorMap[SectorY][SectorX].insert({ SessionID, &User });

	WORD Type = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
	*pOutSendPacket << Type << AccountNo << SectorX << SectorY;

	return true;
}

bool CChatServer::PacketProc_ChatMessage(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID)
{
	INT64 AccountNo;
	WORD MessageLength;

	*pRecvPacket >> AccountNo >> MessageLength;

	auto &User = *m_UserSessionMap.find(SessionID)->second;
	if (&User == m_UserSessionMap.end()->second)
	{
		// ó�� �� �ٰ� ������ �ִ°�?
		return false;
	}
	
	WORD Type = en_PACKET_CS_CHAT_RES_MESSAGE;

	CNetServerSerializationBuf &SendBuf = *pOutSendPacket;
	SendBuf << Type << AccountNo;
	SendBuf.WriteBuffer((char*)&User.szID, sizeof(WCHAR) * dfID_NICKNAME_LENGTH);
	SendBuf << MessageLength;

	SendBuf.ReadBuffer(SendBuf.GetWriteBufferPtr(), MessageLength);
	BroadcastSectorAroundAll(User.SectorX, User.SectorY, &SendBuf);

	return true;
}

void CChatServer::PacketProc_HeartBeat(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket)
{

}

//bool CChatServer::PacketProc_Login(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID)
//{
//	INT64 AccountNo;
//	*pRecvPacket >> AccountNo;
//	char SessionKey[64];
//
//	pRecvPacket->ReadBuffer(SessionKey, sizeof(SessionKey));
//
//	////////////////////////////////////////////////////////
//	auto &User = *m_UserSessionMap.find(SessionID)->second;
//	if (&User == m_UserSessionMap.end()->second)
//	{
//		// ó�� �� �ٰ� ������ �ִ°�?
//		return false;
//	}
//	 // ���� : 1 ���� : 0
//	BYTE Status = 1;
//	// ���� ó��
//	////////////////////////////////////////////////////////
//
//	CNetServerSerializationBuf &SendBuf = *pOutSendPacket;
//	WORD Type = en_PACKET_CS_LOGIN_RES_LOGIN;
//	SendBuf << Type << AccountNo << Status;
//	SendBuf.WriteBuffer((char*)User.szID, sizeof(WCHAR) * dfID_NICKNAME_LENGTH);
//
//
//}
