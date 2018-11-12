#include "PreCompile.h"
#include "MatchmakingServer.h"
#include "MatchmakingLanClient.h"
#include "MatchmakingMonitoringClient.h"

#include "Log.h"
#include "DBConnector.h"

#include "Protocol/CommonProtocol.h"

CMatchmakingServer::CMatchmakingServer()
{

}

CMatchmakingServer::~CMatchmakingServer()
{

}

bool CMatchmakingServer::MatchmakingServerStart(const WCHAR *szMatchmakingNetServerOptionFile, const WCHAR *szMatchmakingLanClientOptionFile, const WCHAR *szMatchmakingMonitoringClientOptionFile)
{
	m_uiNumOfLoginCompleteUser = 0;
	m_uiLoginSuccessTPS = 0;
	m_uiNumOfConnectUser = 0;
	m_uiUserNoCount = 0;

	InitializeCriticalSection(&m_csUserMapLock);
	InitializeCriticalSection(&m_csSessionMapLock);

	m_pUserMemoryPool = new CTLSMemoryPool<st_USER>(5, false);

	m_pMatchmakingLanclient = new CMatchmakingLanClient();
	if(m_pMatchmakingLanclient == NULL)
		return false;
	wprintf_s(L"LanClient On\n");

	m_pMatchmakingMonitoringClient = new CMatchmakingMonitoringClient();
	if(m_pMatchmakingMonitoringClient == NULL)
		return false;
	wprintf_s(L"Monitoring Client On\n");

	if (!Start(szMatchmakingNetServerOptionFile))
		return false;

	if (!MatchmakingNetServerOptionParsing(szMatchmakingNetServerOptionFile))
		return false;
	if (!m_pMatchmakingLanclient->MatchmakingLanClientStart(this, m_wServerNo, szMatchmakingLanClientOptionFile))
		return false;
	if (!m_pMatchmakingMonitoringClient->MatchmakingMonitoringClientStart(szMatchmakingMonitoringClientOptionFile, &m_uiNumOfConnectUser, &m_uiNumOfLoginCompleteUser))
		return false;

	m_hDBSendThreadHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hSendEventHandle[en_USER_CONNECT_MAX_IN_DB_SAVE_TIME] = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hSendEventHandle[en_SEND_THREAD_EXIT] = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_hDBSendThreadHandle == NULL || m_hSendEventHandle[en_USER_CONNECT_MAX_IN_DB_SAVE_TIME] == NULL
		|| m_hSendEventHandle[en_SEND_THREAD_EXIT] == NULL)
		return false;

	m_hDBSendThreadHandle = (HANDLE)_beginthreadex(NULL, 0, shDBSendThreadAddress, NULL, 0, NULL);
	if (m_hDBSendThreadHandle == NULL)
		return false;

	wprintf_s(L"Matchmaking Server On\n");

	return true;
}

void CMatchmakingServer::MatchmakingServerStop()
{
	m_pMatchmakingLanclient->MatchmakingLanClientStop();
	m_pMatchmakingMonitoringClient->MatchmakingMonitoringClientStop();
	Stop();

	SetEvent(m_hSendEventHandle[en_SEND_THREAD_EXIT]);
	WaitForSingleObject(m_hDBSendThreadHandle, INFINITE);

	CloseHandle(m_hSendEventHandle[en_SEND_THREAD_EXIT]);
	CloseHandle(m_hSendEventHandle[en_USER_CONNECT_MAX_IN_DB_SAVE_TIME]);
	CloseHandle(m_hDBSendThreadHandle);

	DeleteCriticalSection(&m_csUserMapLock);
	DeleteCriticalSection(&m_csSessionMapLock);

	auto UserMapIterEnd = m_UserMap.end();
	for (auto iter = m_UserMap.begin(); iter != UserMapIterEnd; ++iter)
	{
		m_pUserMemoryPool->Free(iter->second);
		iter = m_UserMap.erase(iter);
		--iter;
	}

	auto SessionIDMapIterEnd = m_SessionIDMap.end();
	for (auto iter = m_SessionIDMap.begin(); iter != SessionIDMapIterEnd; ++iter)
	{
		iter = m_SessionIDMap.erase(iter);
		--iter;
	}

	delete m_pMatchmakingLanclient;
	delete m_pMatchmakingMonitoringClient;
	delete m_pUserMemoryPool;
}

void CMatchmakingServer::OnClientJoin(UINT64 ClientID)
{
	InterlockedIncrement(&m_uiNumOfConnectUser);
	st_USER *userInfo = m_pUserMemoryPool->Alloc();
	
	// UserMap 임계영역 시작
	EnterCriticalSection(&m_csUserMapLock);
	
	m_UserMap.insert({ ClientID, userInfo });

	// UserMap 임계영역 끝
	LeaveCriticalSection(&m_csUserMapLock);
}

void CMatchmakingServer::OnClientLeave(UINT64 ClientID)
{
	// UserMap 임계영역 시작
	EnterCriticalSection(&m_csUserMapLock);

	unordered_map<UINT64, st_USER*>::iterator UserMapIter = m_UserMap.find(ClientID);
	if (UserMapIter == m_UserMap.end())
	{
		g_Dump.Crash();
	}
	st_USER *pLeaveUser = UserMapIter->second;

	// 해당 유저가 로그인 한 유저라면
	if (pLeaveUser->byStatus != en_STATUS_CREATE)
	{
		// 로그인 된 유저의 수를 차감함
		InterlockedDecrement(&m_uiNumOfLoginCompleteUser);
		// 해당 유저가 배틀 서버로 가는 도중에 접속을 해제하였다면 
		if (pLeaveUser->byStatus == en_STATUS_GO_TO_BATTLE)
			// 이 상황을 마스터에게 알려주어 마스터가 방 접속 인원을 +1 하도록 유도시키기 위하여
			// 유저가 배틀 서버로 가는것을 실패했다고 알려줌
			m_pMatchmakingLanclient->SendToMaster_GameRoomEnterFail(pLeaveUser->uiClientKey);
	}

	m_UserMap.erase(ClientID);

	// UserMap 임계영역 끝
	LeaveCriticalSection(&m_csUserMapLock);

	m_pUserMemoryPool->Free(pLeaveUser);

	// SessionIDMap 임계영역 시작
	EnterCriticalSection(&m_csSessionMapLock);

	m_SessionIDMap.erase(ClientID);

	// SessionIDMap 임계영역 끝
	LeaveCriticalSection(&m_csSessionMapLock);

	InterlockedDecrement(&m_uiNumOfConnectUser);
}

bool CMatchmakingServer::OnConnectionRequest(const WCHAR *IP)
{
	return true;
}

void CMatchmakingServer::OnRecv(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf)
{
	WORD Type;
	*OutReadBuf >> Type;

	switch (Type)
	{
	case en_PACKET_CS_MATCH_REQ_LOGIN:
		LoginProc(ReceivedSessionID, OutReadBuf);
		break;
	case en_PACKET_CS_MATCH_REQ_GAME_ROOM:
	{
		// UserMap 임계영역 시작
		EnterCriticalSection(&m_csUserMapLock);

		unordered_map<UINT64, st_USER*>::iterator UserMapIter = m_UserMap.find(ReceivedSessionID);
		if (UserMapIter == m_UserMap.end())
		{
			g_Dump.Crash();
		}
		st_USER &UserInfo = *UserMapIter->second;

		// UserMap 임계영역 끝
		LeaveCriticalSection(&m_csUserMapLock);

		m_pMatchmakingLanclient->SendToMaster_GameRoom(UserInfo.uiClientKey, UserInfo.iAccountNo);
		UserInfo.byStatus = en_STATUS_ACQUIRE_ROOM;
		break;
	}
	case en_PACKET_CS_MATCH_REQ_GAME_ROOM_ENTER:
	{
		WORD BattleServerNo;
		int RoomNo;
		if (OutReadBuf->GetUseSize() != 6)
			break;
		*OutReadBuf >> BattleServerNo >> RoomNo;

		// UserMap 임계영역 시작
		EnterCriticalSection(&m_csUserMapLock);

		unordered_map<UINT64, st_USER*>::iterator UserMapIter = m_UserMap.find(ReceivedSessionID);
		if (UserMapIter == m_UserMap.end())
		{
			g_Dump.Crash();
		}
		st_USER &UserInfo = *UserMapIter->second;

		// UserMap 임계영역 끝
		LeaveCriticalSection(&m_csUserMapLock);

		// 유저가 방 입장에 성공하였음을 마스터에게 알려줌
		m_pMatchmakingLanclient->SendToMaster_GameRoomEnterSuccess(BattleServerNo, RoomNo,
			UserInfo.uiClientKey);
		UserInfo.byStatus = en_STATUS_ENTER_BATTLE_COMPLETE;

		CNetServerSerializationBuf &SendBuf = *CNetServerSerializationBuf::Alloc();

		Type = en_PACKET_CS_MATCH_RES_GAME_ROOM_ENTER;
		SendBuf << Type;

		// 유저가 스스로 매치메이킹 서버와의 연결을 끊는것을 유도하기 위하여
		// 위의 패킷을 회신해줌
		SendPacket(ReceivedSessionID, &SendBuf);
		break;
	}
	default:
		break;
	}
}

void CMatchmakingServer::OnSend(UINT64 ClientID, int sendsize)
{

}

void CMatchmakingServer::OnWorkerThreadBegin()
{

}

void CMatchmakingServer::OnWorkerThreadEnd()
{

}

void CMatchmakingServer::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != WSAECONNRESET)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"CHATSERVERERR ", L"%d\n%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

void CMatchmakingServer::SendToClient_GameRoom(CSerializationBuf *pRecvBuf)
{
	WORD Type = en_PACKET_CS_MATCH_RES_GAME_ROOM;
	UINT64 ClientKey;
	BYTE Status;

	CNetServerSerializationBuf &SendBuf = *CNetServerSerializationBuf::Alloc();
	CSerializationBuf &RecvBuf = *pRecvBuf;

	RecvBuf >> ClientKey >> Status;

	SendBuf << Type << Status;
	
	// 방을 배정 받기에 실패했다면
	if (Status == 0)
	{
		// m_SessionIDMap 임계영역 시작
		EnterCriticalSection(&m_csSessionMapLock);

		unordered_map<UINT64, UINT64>::iterator SessionMapIter = m_SessionIDMap.find(ClientKey);
		if (SessionMapIter == m_SessionIDMap.end())
		{
			g_Dump.Crash();
		}

		// 클라이언트에게 실패를 통보함
		SendPacket(SessionMapIter->second, &SendBuf);

		// m_SessionIDMap 임계영역 끝
		EnterCriticalSection(&m_csSessionMapLock);

		return;
	}

	WORD BattleServerNo, Port, ChatServerPort;
	int RoomNo;
	char ConnectToken[32], EnterToken[32];
	WCHAR IP[16], ChatServerIP[16];

	RecvBuf >> BattleServerNo;
	RecvBuf.ReadBuffer((char*)IP, sizeof(IP));
	RecvBuf >> Port >> RoomNo;
	RecvBuf.ReadBuffer((char*)ConnectToken, sizeof(ConnectToken));
	RecvBuf.ReadBuffer((char*)EnterToken, sizeof(EnterToken));
	RecvBuf.ReadBuffer((char*)ChatServerIP, sizeof(ChatServerIP));
	RecvBuf >> ChatServerPort;

	SendBuf << BattleServerNo;
	SendBuf.WriteBuffer((char*)IP, sizeof(IP));
	SendBuf << Port << RoomNo;
	SendBuf.WriteBuffer((char*)ConnectToken, sizeof(ConnectToken));
	SendBuf.WriteBuffer((char*)EnterToken, sizeof(EnterToken));
	SendBuf.WriteBuffer((char*)ChatServerIP, sizeof(ChatServerIP));
	SendBuf << ChatServerPort;

	// m_SessionIDMap 임계영역 시작
	EnterCriticalSection(&m_csSessionMapLock);

	unordered_map<UINT64, UINT64>::iterator SessionMapIter = m_SessionIDMap.find(ClientKey);
	if (SessionMapIter == m_SessionIDMap.end())
	{
		g_Dump.Crash();
	}

	// 클라이언트에게 실패를 통보함
	SendPacket(SessionMapIter->second, &SendBuf);

	// m_SessionIDMap 임계영역 끝
	EnterCriticalSection(&m_csSessionMapLock);
}

UINT __stdcall CMatchmakingServer::shDBSendThreadAddress(LPVOID MatchmakingNetServer)
{
	return ((CMatchmakingServer*)MatchmakingNetServer)->shDBSendThread();
}

UINT CMatchmakingServer::shDBSendThread()
{
	CDBConnector::InitTLSDBConnet(m_DBIP, m_DBUserName, m_DBPassWord, m_DBName, m_wDBPort);
	CDBConnector &DBConnector = *CDBConnector::GetDBConnector();

	UINT &NumOfConnectUser = m_uiNumOfConnectUser;

	DWORD WaitObjectRetval = 99999;

	DWORD StandardTime = m_wshDBSaveTime;

	// 메인 루프를 돌기 이전에 자신이 가지고 있는 정보를 미리 DB 에 반영시켜 놓음
	// 만약 DB 에 해당 정보가 없다면 INSERT 시킴
	if (!DBConnector.SendQuery_Save(L"INSERT INTO matchmaking_status.server (`ip`, `port`, `connectuser`, `heartbeat`) VALUES(\'%s\', %d, %d, NOW()) ONDUPLICATE KEY UPDATE `ip`=\"%s\", `port`=%d, `connectuser`=%d, `heartbeat`=NOW()", m_DBIP, m_wDBPort, NumOfConnectUser))
		g_Dump.Crash();

	// 해당 서버의 serverno 를 얻어오기 위하여 SELECT 쿼리를 보냄
	if (!DBConnector.SendQuery(L"SELECT matchmaking_status.server WHERE `ip`=\'%s\' AND `port`=%d"), m_DBIP, m_wDBPort)
		g_Dump.Crash();
	int ServerNo = atoi(DBConnector.FetchRow()[0]);
	DBConnector.FreeResult();

	ULONGLONG NowTime, TotalTime;
	ULONGLONG BeforeTime = GetTickCount64();

	// StandardTime 만큼마다 DB 에 매치메이킹 서버의 상태를 보내준다
	// 일정 인원수 이상이 StandardTime 내에 한꺼번에 들어왔다면
	// 그 때도 DB 에 상태를 보내며
	// 위의 처리등으로 보내는것이 3초가 밀렸다면 한 번 더 보낸다
	while (1)
	{
		NowTime = GetTickCount64();
		// WAIT_TIMEOUT 의 경우 StandardTime 만큼 쉬고 들어왔으므로 그 시간은 빼고 계산한다
		if (WaitObjectRetval == WAIT_TIMEOUT)
			TotalTime += NowTime - BeforeTime - StandardTime;
		else
			TotalTime += NowTime - BeforeTime;
		
		// 총합 시간이 기준 시간보다 많다면
		if (StandardTime < TotalTime)
		{
			// DB 에 다시 한 번 매치메이킹 서버의 상태를 반영해준다
			DBConnector.SendQuery_Save(L"UPDATE matchmaking_status.server SET `connectuser`=%d, `heartbeat`=NOW() WHERE serverno=%d", NumOfConnectUser, ServerNo);
			TotalTime -= StandardTime;
		}
		
		WaitObjectRetval = WaitForMultipleObjects(2, m_hSendEventHandle, FALSE, StandardTime);
		// StandardTime 만큼 지났거나 혹은 그 시간 이내에 정해놓은 인원 이상이 들어왔을 경우
		if (WaitObjectRetval == WAIT_TIMEOUT || WaitObjectRetval == WAIT_OBJECT_0)
			// DB 에 현재 매치메이킹 서버의 상태를 보내준다
			DBConnector.SendQuery_Save(L"UPDATE matchmaking_status.server SET `connectuser`=%d, `heartbeat`=NOW() WHERE serverno=%d", NumOfConnectUser, ServerNo);
		// 위의 상황이 아닐경우
		else
			// 외부에서 이 스레드를 종료시킴
			break;
	}

	return 0;
}

UINT CMatchmakingServer::GetLoginSuccessTPSAndReset()
{
	UINT LoginSuccessTPS = m_uiLoginSuccessTPS;
	InterlockedExchange(&m_uiLoginSuccessTPS, 0);

	return LoginSuccessTPS;
}

UINT CMatchmakingServer::GetAcceptTPSAndReset()
{
	UINT AcceptTPS = m_uiAcceptTPS;
	InterlockedExchange(&m_uiAcceptTPS, 0);

	return AcceptTPS;
}

bool CMatchmakingServer::MatchmakingNetServerOptionParsing(const WCHAR *szOptionFileName)
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

	if (!parser.GetValue_String(pBuff, L"DATABASE", L"DB_IP", m_DBIP))
		return false;
	if (!parser.GetValue_String(pBuff, L"DATABASE", L"DB_USERNAME", m_DBUserName))
		return false;
	if (!parser.GetValue_String(pBuff, L"DATABASE", L"DB_PASSWORD", m_DBPassWord))
		return false;
	if (!parser.GetValue_String(pBuff, L"DATABASE", L"DB_NAME", m_DBName))
		return false;
	if (!parser.GetValue_Short(pBuff, L"DATABASE", L"DB_PORT", (short*)&m_wDBPort))
		return false;

	if (!parser.GetValue_Short(pBuff, L"MATCHMAKING_SERVER", L"DB_SAVETIME", (short*)&m_wshDBSaveTime))
		return false;
	if (!parser.GetValue_Int(pBuff, L"MATCHMAKING_SERVER", L"VERSION_CODE", (int*)&m_uiServerVersionCode))
		return false;
	if (!parser.GetValue_Int(pBuff, L"MATCHMAKING_SERVER", L"SERVER_NO", &m_wServerNo))
		return false;

	return true;
}
