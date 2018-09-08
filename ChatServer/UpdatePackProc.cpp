#include "PreComfile.h"
#include "ChatServer.h"
#include "NetServerSerializeBuffer.h"
#include "CommonProtocol.h"


/////////////////////////////////////////////////////////////////////
// Update 스레드 Packet 함수 전용 cpp
// pMessage 안의 Packet이 원래 Packet 에서 Type 이 떨어져 나온 상태로 들어옴
/////////////////////////////////////////////////////////////////////

bool CChatServer::PacketProc_PlayerJoin(UINT64 SessionID)
{
	st_USER &NewClient = *m_pUserMemoryPool->Alloc();
	NewClient.uiSessionID = SessionID;
	NewClient.SectorX = dfINIT_SECTOR_VALUE;
	NewClient.SectorY = dfINIT_SECTOR_VALUE;
	NewClient.bIsLoginUser = FALSE;
	NewClient.uiAccountNO = dfACCOUNT_INIT_VALUE;

	m_UserSessionMap.insert({ SessionID, &NewClient });

	return true;
}

bool CChatServer::PacketProc_PlayerLeave(UINT64 SessionID)
{   
	st_USER &LeaveClient = *m_UserSessionMap.find(SessionID)->second;
	if (&LeaveClient == m_UserSessionMap.end()->second)
		return false;

	m_UserSessionMap.erase(SessionID);
	if (LeaveClient.SectorX >= dfMAX_SECTOR_X || LeaveClient.SectorY >= dfMAX_SECTOR_Y)
	{
		m_pUserMemoryPool->Free(&LeaveClient);
		return false;
	}

	if (m_UserSectorMap[LeaveClient.SectorY][LeaveClient.SectorX].erase(SessionID) == 0)
	{
		st_Error Err;
		Err.GetLastErr = 0;
		Err.NetServerErr = NOT_IN_USER_SECTORMAP_ERR;
		OnError(&Err);
		m_pUserMemoryPool->Free(&LeaveClient);
		return false;
	}

	m_pUserMemoryPool->Free(&LeaveClient);
	return true;
}

bool CChatServer::PacketProc_SectorMove(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID)
{
	INT64 AccountNo;
	WORD SectorX, SectorY;
	if (pRecvPacket->GetUseSize() != dfPACKETSIZE_SECTOR_MOVE)
	{
		st_Error err;
		err.GetLastErr = 0;
		err.Line = __LINE__;
		err.NetServerErr = SERIALIZEBUF_SIZE_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}
	
	*pRecvPacket >> AccountNo >> SectorX >> SectorY;

	if (SectorX >= dfMAX_SECTOR_X || SectorY >= dfMAX_SECTOR_Y)
	{
		st_Error err;
		err.GetLastErr = 0;
		err.Line = __LINE__;
		err.NetServerErr = SECTOR_RANGE_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	auto &User = *m_UserSessionMap.find(SessionID)->second;
	if (&User == m_UserSessionMap.end()->second)
	{
		st_Error err;
		err.Line = __LINE__;
		err.NetServerErr = NOT_IN_USER_MAP_ERR;
		OnError(&err);
		DisConnect(SessionID);
		// 처리 해 줄게 무엇이 있는가?
		return false;
	}

	if (User.SectorX < dfMAX_SECTOR_X && User.SectorY < dfMAX_SECTOR_Y)
	{
		if (m_UserSectorMap[User.SectorY][User.SectorX].erase(SessionID) == 0)
		{
			st_Error err;
			err.Line = __LINE__;
			err.NetServerErr = NOT_IN_USER_SECTORMAP_ERR;
			OnError(&err);
			DisConnect(SessionID);
			// 처리 해 줄게 무엇이 있는가?
			return false;
		}
	}

	if (User.bIsLoginUser == FALSE)
	{
		st_Error err;
		err.Line = __LINE__;
		err.NetServerErr = NOT_LOGIN_USER_ERR;
		OnError(&err);
		DisConnect(SessionID);
		// 처리 해 줄게 무엇이 있는가?
		return false;
	}

	if (User.uiAccountNO != AccountNo)
	{
		st_Error err;
		err.Line = __LINE__;
		err.NetServerErr = INCORRECT_ACCOUNTNO;
		OnError(&err);
		DisConnect(SessionID);
		// 처리 해 줄게 무엇이 있는가?
		return false;
	}

	User.SectorX = SectorX;
	User.SectorY = SectorY;
	m_UserSectorMap[SectorY][SectorX].insert({ SessionID, &User });

	WORD Type = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
	*pOutSendPacket << Type << AccountNo << SectorX << SectorY;

	SendPacket(SessionID, pOutSendPacket);
	return true;
}

bool CChatServer::PacketProc_ChatMessage(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID)
{
	INT64 AccountNo;
	WORD MessageLength;

	int RecvedSize = pRecvPacket->GetUseSize();
	if (RecvedSize < dfPACKETSIZE_CHAT_MESSAGE)
	{
		st_Error err;
		err.GetLastErr = 0;
		err.Line = __LINE__;
		err.NetServerErr = SERIALIZEBUF_SIZE_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}
	*pRecvPacket >> AccountNo >> MessageLength;

	if (RecvedSize != MessageLength + dfPACKETSIZE_CHAT_MESSAGE)
	{
		st_Error err;
		err.GetLastErr = 0;
		err.Line = __LINE__;
		err.NetServerErr = SERIALIZEBUF_SIZE_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	auto &User = *m_UserSessionMap.find(SessionID)->second;
	if (&User == m_UserSessionMap.end()->second)
	{
		st_Error err;
		err.Line = __LINE__;
		err.NetServerErr = NOT_IN_USER_MAP_ERR;
		OnError(&err);
		DisConnect(SessionID);
		// 처리 해 줄게 무엇이 있는가?
		return false;
	}

	if (User.bIsLoginUser == FALSE)
	{
		st_Error err;
		err.Line = __LINE__;
		err.NetServerErr = NOT_LOGIN_USER_ERR;
		OnError(&err);
		DisConnect(SessionID);
		// 처리 해 줄게 무엇이 있는가?
		return false;
	}

	if (User.uiAccountNO != AccountNo)
	{
		st_Error err;
		err.Line = __LINE__;
		err.NetServerErr = INCORRECT_ACCOUNTNO;
		OnError(&err);
		DisConnect(SessionID);
		// 처리 해 줄게 무엇이 있는가?
		return false;
	}

	if (User.SectorX > dfMAX_SECTOR_X || User.SectorY > dfMAX_SECTOR_Y)
	{
		st_Error err;
		err.Line = __LINE__;
		err.NetServerErr = SECTOR_RANGE_ERR;
		OnError(&err);
		DisConnect(SessionID);
		// 처리 해 줄게 무엇이 있는가?
		return false;
	}

	WORD Type = en_PACKET_CS_CHAT_RES_MESSAGE;

	CNetServerSerializationBuf &SendBuf = *pOutSendPacket;
	SendBuf << Type << AccountNo;
	SendBuf.WriteBuffer((char*)&User.szID, sizeof(WCHAR) * dfID_NICKNAME_LENGTH);
	SendBuf << MessageLength;

	SendBuf.WriteBuffer(pRecvPacket->GetReadBufferPtr(), MessageLength);
	BroadcastSectorAroundAll(User.SectorX, User.SectorY, &SendBuf);
	//SendPacket(SessionID, &SendBuf);
	return true;
}

void CChatServer::PacketProc_HeartBeat(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket)
{

}

bool CChatServer::PacketProc_Login(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID)
{
	INT64 AccountNo;
	CNetServerSerializationBuf &RecvPacket = *pRecvPacket;
	if (RecvPacket.GetUseSize() != dfPACKETSIZE_LOGIN)
	{
		st_Error err;
		err.GetLastErr = 0;
		err.Line = __LINE__;
		err.NetServerErr = SERIALIZEBUF_SIZE_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	RecvPacket >> AccountNo;

	BYTE Status;
	CNetServerSerializationBuf &SendBuf = *pOutSendPacket;
	WORD Type = en_PACKET_CS_CHAT_RES_LOGIN;
	
	auto &User = *m_UserSessionMap.find(SessionID)->second;
	if (&User != m_UserSessionMap.end()->second)
		Status = TRUE;
	else
	{
		Status = FALSE;
		SendBuf << Type << Status << AccountNo;
		st_Error err;
		err.Line = __LINE__;
		err.NetServerErr = NOT_IN_USER_MAP_ERR;
		OnError(&err);
		// 처리 해 줄게 무엇이 있는가?
		SendPacket(SessionID, &SendBuf);
		DisConnect(SessionID);
		return false;
	}

	User.uiAccountNO = AccountNo;
	RecvPacket.ReadBuffer((char*)&User.szID, sizeof(WCHAR) * dfID_NICKNAME_LENGTH);
	RecvPacket.ReadBuffer((char*)&User.SessionKey, sizeof(User.SessionKey));

	SendBuf << Type  << Status << AccountNo;
	
	User.bIsLoginUser = TRUE;

	SendPacket(SessionID, &SendBuf);
	return true;
}
