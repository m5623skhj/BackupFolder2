#include "PreCompile.h"
#include "ChatServer.h"
#include "NetServerSerializeBuffer.h"
#include "CommonProtocol.h"
#include "Log.h"


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
	NewClient.uiBeforeRecvTime = m_uiHeartBeatTime;

	m_UserSessionMap.insert({ SessionID, &NewClient });

	return true;
}

bool CChatServer::PacketProc_PlayerLeave(UINT64 SessionID)
{
	st_USER &LeaveClient = *m_UserSessionMap.find(SessionID)->second;
	if (&LeaveClient == m_UserSessionMap.end()->second)
		return false;

	// SessionKeyMap 임계영역 시작
	EnterCriticalSection(&m_SessionKeyMapCS);

	// 이전에 해당 유저의 정보를 덮어 썼는지 확인함
	if (LeaveClient.byNumOfOverWriteSessionKey > 0)
	{
		// 이전 정보들은 이미 로그인 할 때 지웠으므로
		// 플래그만 깎고 반환함
		--LeaveClient.byNumOfOverWriteSessionKey;
		return false;
	}

	st_SessionKey *pSessionKey = m_UserSessionKeyMap.find(LeaveClient.uiAccountNO)->second;
	if (pSessionKey != NULL)
	{
		if (m_UserSessionKeyMap.erase(LeaveClient.uiAccountNO) != 0)
			m_pSessionKeyMemoryPool->Free(pSessionKey);
		else
			_LOG(LOG_LEVEL::LOG_DEBUG, L"ERROR", L"SessionKey Map Erase");
	}

	// SessionKeyMap 임계영역 끝
	LeaveCriticalSection(&m_SessionKeyMapCS);

	m_UserSessionMap.erase(SessionID);
	if (LeaveClient.SectorX > dfMAX_SECTOR_X || LeaveClient.SectorY > dfMAX_SECTOR_Y)
	{
		m_pUserMemoryPool->Free(&LeaveClient);
		return false;
	}

	if (m_UserSectorMap[LeaveClient.SectorY][LeaveClient.SectorX].erase(SessionID) == 0)
	{
		st_Error Err;
		Err.GetLastErr = 0;
		Err.ServerErr = NOT_IN_USER_SECTOR_MAP_ERR;
		OnError(&Err);
		m_pUserMemoryPool->Free(&LeaveClient);
		return false;
	}

	m_pUserMemoryPool->Free(&LeaveClient);
	return true;
}

bool CChatServer::PacketProc_PlayerLoginFromLoginServer(CNetServerSerializationBuf *pRecvPacket)
{   
	UINT64 AccountNo, SessionKeyAddress;
	st_SessionKey *pSessionKey;

	*pRecvPacket >> AccountNo >> SessionKeyAddress;
	pSessionKey = (st_SessionKey*)SessionKeyAddress;

	// SessionKeyMap 임계영역 시작
	EnterCriticalSection(&m_SessionKeyMapCS);
	
	// insert 하면서 이전 유저가 아직 나가지 않음을 확인함
	if ((m_UserSessionKeyMap.insert({ AccountNo, pSessionKey })).second == false)
	{
		// 세션키를 지우기 위하여 이전 세션키를 받음
		st_SessionKey *pNowSessionKeyAddr = m_UserSessionKeyMap.find(AccountNo)->second;
		// 현재 받아온 세션키를 이전 정보에 덮어 씀
		m_UserSessionKeyMap.find(AccountNo)->second = pSessionKey;
		// 이전 세션키를 지움
		st_USER &OverWirteUser = *m_UserSessionMap.find(AccountNo)->second;

		// 차후 Leave 에서 현재 정보를 삭제하지 못하도록 플래그를 올림
		++OverWirteUser.byNumOfOverWriteSessionKey;
		// 이전 정보 중 반환해야 될 정보들을 반환함
		m_pSessionKeyMemoryPool->Free(pNowSessionKeyAddr);
		m_UserSectorMap[OverWirteUser.SectorY][OverWirteUser.SectorX].erase(AccountNo);

		//// 끊는다?
		//m_pSessionKeyMemoryPool->Free(pSessionKey);
		//LeaveCriticalSection(&m_SessionKeyMapCS);
		//_LOG(LOG_LEVEL::LOG_DEBUG, L"ERROR", L"SessionKey Map Insert");
		//g_Dump.Crash();
		//return false;
	}

	// SessionKeyMap 임계영역 끝
	LeaveCriticalSection(&m_SessionKeyMapCS);

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
		err.ServerErr = SERIALIZEBUF_SIZE_ERR;
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
		err.ServerErr = SECTOR_RANGE_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	auto &User = *m_UserSessionMap.find(SessionID)->second;
	if (&User == m_UserSessionMap.end()->second)
	{
		st_Error err;
		err.Line = __LINE__;
		err.ServerErr = NOT_IN_USER_MAP_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	if (User.SectorX < dfMAX_SECTOR_X && User.SectorY < dfMAX_SECTOR_Y)
	{
		if (m_UserSectorMap[User.SectorY][User.SectorX].erase(SessionID) == 0)
		{
			st_Error err;
			err.Line = __LINE__;
			err.ServerErr = NOT_IN_USER_SECTOR_MAP_ERR;
			OnError(&err);
			DisConnect(SessionID);
			return false;
		}
	}

	if (User.bIsLoginUser == FALSE)
	{
		st_Error err;
		err.Line = __LINE__;
		err.ServerErr = NOT_LOGIN_USER_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	if (User.uiAccountNO != AccountNo)
	{
		st_Error err;
		err.Line = __LINE__;
		err.ServerErr = INCORRECT_ACCOUNTNO;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	User.uiBeforeRecvTime = m_uiHeartBeatTime;

	User.SectorX = SectorX;
	User.SectorY = SectorY;
	m_UserSectorMap[SectorY][SectorX].insert({ SessionID, &User });

	WORD Type = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
	*pOutSendPacket << Type << AccountNo << SectorX << SectorY;

	CNetServerSerializationBuf::AddRefCount(pOutSendPacket);
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
		err.ServerErr = SERIALIZEBUF_SIZE_ERR;
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
		err.ServerErr = SERIALIZEBUF_SIZE_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	auto &User = *m_UserSessionMap.find(SessionID)->second;
	if (&User == m_UserSessionMap.end()->second)
	{
		st_Error err;
		err.Line = __LINE__;
		err.ServerErr = NOT_IN_USER_MAP_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	if (User.bIsLoginUser == FALSE)
	{
		st_Error err;
		err.Line = __LINE__;
		err.ServerErr = NOT_LOGIN_USER_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	if (User.uiAccountNO != AccountNo)
	{
		st_Error err;
		err.Line = __LINE__;
		err.ServerErr = INCORRECT_ACCOUNTNO;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	if (User.SectorX > dfMAX_SECTOR_X || User.SectorY > dfMAX_SECTOR_Y)
	{
		st_Error err;
		err.Line = __LINE__;
		err.ServerErr = SECTOR_RANGE_ERR;
		OnError(&err);
		DisConnect(SessionID);
		return false;
	}

	User.uiBeforeRecvTime = m_uiHeartBeatTime;

	WORD Type = en_PACKET_CS_CHAT_RES_MESSAGE;

	CNetServerSerializationBuf &SendBuf = *pOutSendPacket;
	SendBuf << Type << AccountNo;
	SendBuf.WriteBuffer((char*)&User.szID, sizeof(WCHAR) * dfID_NICKNAME_LENGTH);
	SendBuf << MessageLength;

	SendBuf.WriteBuffer(pRecvPacket->GetReadBufferPtr(), MessageLength);
	
	BroadcastSectorAroundAll(User.SectorX, User.SectorY, &SendBuf);

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
		err.ServerErr = SERIALIZEBUF_SIZE_ERR;
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
		err.ServerErr = NOT_IN_USER_MAP_ERR;
		OnError(&err);

		CNetServerSerializationBuf::AddRefCount(&SendBuf);
		SendPacket(SessionID, &SendBuf);

		DisConnect(SessionID);
		return false;
	}

	User.uiAccountNO = AccountNo;
	RecvPacket.ReadBuffer((char*)&User.szID, sizeof(WCHAR) * dfID_NICKNAME_LENGTH);
	RecvPacket.ReadBuffer((char*)&User.SessionKey, sizeof(User.SessionKey));
	
	// SessionKeyMap 임계영역 시작
	EnterCriticalSection(&m_SessionKeyMapCS);
	
	st_SessionKey *pSessionKey = m_UserSessionKeyMap.find(AccountNo)->second;
	if (memcmp(pSessionKey, User.SessionKey, dfSESSIONKEY_SIZE) != 0)
		//if (pSessionKey == m_UserSessionKeyMap.end()->second)
	{
		m_iNumOfSessionKeyMiss++;
		Status = FALSE;
		SendBuf << Type << Status << AccountNo;
		st_Error err;
		err.Line = __LINE__;
		err.ServerErr = NOT_IN_USER_ACCOUNTMAP_ERR;
		OnError(&err);
		SendPacket(SessionID, &SendBuf);
		DisConnect(SessionID);
		return false;
	}

	//m_pSessionKeyMemoryPool->Free(pSessionKey);

	// SessionKeyMap 임계영역 끝
	LeaveCriticalSection(&m_SessionKeyMapCS);

	SendBuf << Type  << Status << AccountNo;
	
	User.bIsLoginUser = TRUE;
	User.uiBeforeRecvTime = m_uiHeartBeatTime;

	CNetServerSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(SessionID, &SendBuf);

	return true;
}
