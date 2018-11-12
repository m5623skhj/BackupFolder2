#include "PreCompile.h"

#include "MasterServer.h"
#include "MasterToBattle.h"
#include "MasterServerMonitoringClient.h"

#include "LanServerSerializeBuf.h"
#include "Log.h"

#include "Protocol/CommonProtocol.h"

CMasterServer::CMasterServer()
{

}

CMasterServer::~CMasterServer()
{

}

bool CMasterServer::MasterServerStart(const WCHAR *szMasterLanServerFileName, const WCHAR *szMasterToBattleServerFileName,
	const WCHAR *szMasterMonitoringClientFileName)
{
	InitializeSRWLock(&m_AuthorizedSessionMapSRWLock);
	m_ClientInfoMapFindClientKey.reserve(dfRESERVER_CLIENTKEYMAP_SIZE);

	NumOfMatchmakingSessionAll = 0;
	NumOfMatchmakingLoginSession = 0;

	//m_pClientInfoMemoryPool = new CTLSMemoryPool<st_CLIENT_INFO>(10, false);
	//if (m_pClientInfoMemoryPool == NULL)
	//	return false;

	m_pMasterToBattleServer = new CMasterToBattleServer();
	m_pMasterMonitoringClient = new CMasterMonitoringClient();
	if (m_pMasterToBattleServer == NULL || m_pMasterMonitoringClient == NULL)
		return false;

	if (!m_pMasterToBattleServer->MasterToBattleServerStart(szMasterToBattleServerFileName, this))
		return false;
	if (!m_pMasterMonitoringClient->MasterMonitoringClientStart(szMasterMonitoringClientFileName, this))
		return false;
	if (!MasterLanServerOptionParsing(szMasterLanServerFileName))
		return false;

	pNumOfBattleServerSessionAll = &m_pMasterToBattleServer->NumOfBattleServerSessionAll;
	pNumOfBattleServerLoginSession = &m_pMasterToBattleServer->NumOfBattleServerLoginSession;
	
	return true;
}

void CMasterServer::MasterServerStop()
{

}

void CMasterServer::OnClientJoin(UINT64 ClientID/* Client 정보 / ClientID / 기타등등 */)
{
	st_MATCHMAKING_CLIENT MatchmakingInfo;

	// m_AuthorizedSessionMap 임계영역 시작 (쓰기)
	AcquireSRWLockExclusive(&m_AuthorizedSessionMapSRWLock);

	m_AuthorizedSessionMap.insert({ ClientID, MatchmakingInfo });

	// m_AuthorizedSessionMap 임계영역 끝   (쓰기)
	ReleaseSRWLockExclusive(&m_AuthorizedSessionMapSRWLock);

	InterlockedIncrement(&NumOfMatchmakingSessionAll);
}

void CMasterServer::OnClientLeave(UINT64 ClientID)
{
	// m_AuthorizedSessionMap 임계영역 시작 (쓰기)
	AcquireSRWLockExclusive(&m_AuthorizedSessionMapSRWLock);

	if (m_AuthorizedSessionMap.find(ClientID)->second.bIsAuthorized)
		InterlockedDecrement(&NumOfMatchmakingLoginSession);
	m_AuthorizedSessionMap.erase(ClientID);

	// m_AuthorizedSessionMap 임계영역 끝   (쓰기)
	ReleaseSRWLockExclusive(&m_AuthorizedSessionMapSRWLock);

	InterlockedIncrement(&NumOfMatchmakingSessionAll);
}

bool CMasterServer::OnConnectionRequest()
{
	return true;
}

void CMasterServer::OnRecv(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf)
{
	CSerializationBuf &RecvBuf = *OutReadBuf;
	
	WORD Type;
	RecvBuf >> Type;

	switch (Type)
	{
	
	case en_PACKET_MAT_MAS_REQ_GAME_ROOM:
	{
		// 마스터 서버가 가지고있는 방들 중
		// 가장 인원이 많이 들어있는 방을 찾아서 그 정보를 회신함

		if (RecvBuf.GetUseSize() != dfSIZE_GAME_ROOM)
		{
			DisConnect(ReceivedSessionID);
			break;
		}
		AcquireSRWLockShared(&m_AuthorizedSessionMapSRWLock);
		if (!m_AuthorizedSessionMap.find(ReceivedSessionID)->second.bIsAuthorized)
		{
			ReleaseSRWLockShared(&m_AuthorizedSessionMapSRWLock);
			DisConnect(ReceivedSessionID);
			break;
		}
		ReleaseSRWLockShared(&m_AuthorizedSessionMapSRWLock);

		UINT64 ClientKey;
		RecvBuf >> ClientKey;
		
		CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
		Type = en_PACKET_MAT_MAS_RES_GAME_ROOM;
		SendBuf << Type << ClientKey;

		m_pMasterToBattleServer->MatchToMasterRequireRoom(&SendBuf);

		SendPacket(ReceivedSessionID, &SendBuf);
	}
	break;

	case en_PACKET_MAT_MAS_REQ_ROOM_ENTER_SUCCESS:
	{
		// 해당 인원이 접속을 성공했음

		if (RecvBuf.GetUseSize() != dfSIZE_ENTER_SUCCESS)
		{
			DisConnect(ReceivedSessionID);
			break;
		}
		AcquireSRWLockShared(&m_AuthorizedSessionMapSRWLock);
		if (!m_AuthorizedSessionMap.find(ReceivedSessionID)->second.bIsAuthorized)
		{
			ReleaseSRWLockShared(&m_AuthorizedSessionMapSRWLock);
			DisConnect(ReceivedSessionID);
			break;
		}
		ReleaseSRWLockShared(&m_AuthorizedSessionMapSRWLock);

		WORD BattleServerNo;
		int RoomNo;
		UINT64 ClientKey;

		RecvBuf >> BattleServerNo >> RoomNo >> ClientKey;

		// m_ClientInfoMapFindClientKey 임계영역 시작
		EnterCriticalSection(&m_csClientInfoMapFindClientKey);

		if (m_ClientInfoMapFindClientKey.erase(ClientKey) == 0)
		{
			// 해당 맵은 성공 혹은 실패에서만 지우는데
			// 성공과 실패가 같이 올 수는 없으므로 없을 경우 에러라고 판단함
			g_Dump.Crash();
		}

		// m_ClientInfoMapFindClientKey 임계영역 끝
		LeaveCriticalSection(&m_csClientInfoMapFindClientKey);

		// 해당 인원을 확인 해 줄 수 있나?
		m_pMasterToBattleServer->MatchToMasterEnterRoomSuccess(ClientKey, BattleServerNo, RoomNo);
	}
	break;

	case en_PACKET_MAT_MAS_REQ_ROOM_ENTER_FAIL:
	{
		// 해당 인원이 접속을 실패했음
		// 마스터 서버는 클라이언트 키로
		// 어떤 클라이언트가 방 입장에 실패했는지 파악 후 방 인원을 되돌림

		if (RecvBuf.GetUseSize() != dfSIZE_ENTER_FAIL)
		{
			DisConnect(ReceivedSessionID);
			break;
		}
		AcquireSRWLockShared(&m_AuthorizedSessionMapSRWLock);
		if (!m_AuthorizedSessionMap.find(ReceivedSessionID)->second.bIsAuthorized)
		{
			ReleaseSRWLockShared(&m_AuthorizedSessionMapSRWLock);
			DisConnect(ReceivedSessionID);
			break;
		}
		ReleaseSRWLockShared(&m_AuthorizedSessionMapSRWLock);

		UINT64 ClientKey;
		RecvBuf >> ClientKey;

		// m_ClientInfoMapFindClientKey 임계영역 시작
		EnterCriticalSection(&m_csClientInfoMapFindClientKey);
		
		unordered_map<UINT64, UINT64>::iterator ClientInfoIter = m_ClientInfoMapFindClientKey.find(ClientKey);
		if (ClientInfoIter == m_ClientInfoMapFindClientKey.end())
		{
			// 해당 맵은 여기 혹은 성공에서만 지우는데
			// 성공과 실패가 같이 올 수는 없으므로 없을 경우 에러라고 판단함
			g_Dump.Crash();
			// m_ClientInfoMapFindClientKey 임계영역 끝
			//LeaveCriticalSection(&m_csClientInfoMapFindClientKey);
			//break;
		}
		
		UINT64 BattleRoomInfo = ClientInfoIter->second;
		m_ClientInfoMapFindClientKey.erase(ClientKey);

		// m_ClientInfoMapFindClientKey 임계영역 끝
		LeaveCriticalSection(&m_csClientInfoMapFindClientKey);

		m_pMasterToBattleServer->MatchToMasterEnterRoomFail(ClientKey, BattleRoomInfo);
	}
	break;

	case en_PACKET_MAT_MAS_REQ_SERVER_ON:
	{
		// 매치메이킹 서버가 마스터 서버에게 접속 한 이후
		// 가장 처음으로 보내는 패킷임
		// 이 패킷이 와야만 매치메이킹 서버와 마스터 간의 통신이 가능함

		if (RecvBuf.GetUseSize() != dfSIZE_SERVER_ON)
		{
			DisConnect(ReceivedSessionID);
			break;
		}

		int ServerNo;
		char RecvMasterToken[32];

		RecvBuf >> ServerNo;
		RecvBuf.ReadBuffer(RecvMasterToken, sizeof(RecvMasterToken));

		if (memcmp(m_MasterToken, RecvMasterToken, sizeof(RecvMasterToken) != 0))
		{
			DisConnect(ReceivedSessionID);
			break;
		}

		AcquireSRWLockShared(&m_AuthorizedSessionMapSRWLock);

		st_MATCHMAKING_CLIENT &MatchmakingClient = m_AuthorizedSessionMap.find(ReceivedSessionID)->second;

		ReleaseSRWLockShared(&m_AuthorizedSessionMapSRWLock);

		MatchmakingClient.bIsAuthorized = true;
		MatchmakingClient.iMatchmakingServerNo = ServerNo;

		CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();

		Type = en_PACKET_MAT_MAS_RES_SERVER_ON;
		SendBuf << Type << ServerNo;

		SendPacket(ReceivedSessionID, &SendBuf);

		InterlockedIncrement(&NumOfMatchmakingLoginSession);
	}
	break;

	default:
		break;
	}

}

void CMasterServer::OnSend(UINT64 ClientID, int sendsize)
{

}

void CMasterServer::OnWorkerThreadBegin()
{

}

void CMasterServer::OnWorkerThreadEnd()
{

}

void CMasterServer::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != WSAECONNRESET)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"CHATSERVERERR ", L"%d\n%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

bool CMasterServer::MasterLanServerOptionParsing(const WCHAR *szOptionFileName)
{
	_wsetlocale(LC_ALL, L"Korean");

	CParser parser;
	WCHAR cBuffer[BUFFER_MAX];

	FILE *fp;
	_wfopen_s(&fp, szOptionFileName, L"rt, ccs=UNICODE");

	int iJumpBOM = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int iFileSize = ftell(fp);
	fseek(fp, iJumpBOM, SEEK_SET);
	int FileSize = (int)fread_s(cBuffer, BUFFER_MAX, sizeof(WCHAR), BUFFER_MAX / 2, fp);
	int iAmend = iFileSize - FileSize; // 개행 문자와 파일 사이즈에 대한 보정값
	fclose(fp);

	cBuffer[iFileSize - iAmend] = '\0';
	WCHAR *pBuff = cBuffer;

	//if (!parser.GetValue_String(pBuff, L"DATABASE", L"DB_IP", m_DBIP))
	//	return false;
	//if (!parser.GetValue_String(pBuff, L"DATABASE", L"DB_USERNAME", m_DBUserName))
	//	return false;
	//if (!parser.GetValue_String(pBuff, L"DATABASE", L"DB_PASSWORD", m_DBPassWord))
	//	return false;
	//if (!parser.GetValue_String(pBuff, L"DATABASE", L"DB_NAME", m_DBName))
	//	return false;
	//if (!parser.GetValue_Short(pBuff, L"DATABASE", L"DB_PORT", (short*)&m_wDBPort))
	//	return false;

	//if (!parser.GetValue_Short(pBuff, L"MATCHMAKING_SERVER", L"DB_SAVETIME", (short*)&m_wshDBSaveTime))
	//	return false;
	//if (!parser.GetValue_Int(pBuff, L"MATCHMAKING_SERVER", L"VERSION_CODE", (int*)&m_uiServerVersionCode))
	//	return false;
	//if (!parser.GetValue_Int(pBuff, L"MATCHMAKING_SERVER", L"SERVER_NO", &m_wServerNo))
	//	return false;

	return true;
}

int CMasterServer::GetNumOfClientInfo()
{
	return (int)m_ClientInfoMapFindClientKey.size(); 
}

int CMasterServer::GetUsingLanServerSessionBufCount() 
{
	return CSerializationBuf::GetUsingSerializeBufNodeCount();
}

int CMasterServer::GetUsingLanSerializeChunkCount()
{
	return CSerializationBuf::GetUsingSerializeBufChunkCount();
}

int CMasterServer::GetAllocLanSerializeBufCount()
{
	return CSerializationBuf::GetAllocSerializeBufChunkCount();
}

int CMasterServer::CallGetRoomInfoMemoryPoolChunkAllocSize()
{
	return m_pMasterToBattleServer->GetRoomInfoMemoryPoolChunkAllocSize();
}

int CMasterServer::CallGetRoomInfoMemoryPoolChunkUseSize()
{
	return m_pMasterToBattleServer->GetRoomInfoMemoryPoolChunkUseSize();
}

int CMasterServer::CallGetRoomInfoMemoryPoolBufUseSize()
{
	return m_pMasterToBattleServer->GetRoomInfoMemoryPoolBufUseSize();
}

int CMasterServer::CallGetBattleStanbyRoomSize()
{
	return m_pMasterToBattleServer->GetBattleStanbyRoomSize();
}
