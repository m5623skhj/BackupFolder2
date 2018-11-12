#include "PreCompile.h"

#include "MasterToBattle.h"
#include "MasterServer.h"

#include "LanServerSerializeBuf.h"
#include "Log.h"

#include "Protocol/CommonProtocol.h"

CMasterToBattleServer::CMasterToBattleServer()
{

}

CMasterToBattleServer::~CMasterToBattleServer()
{

}

bool CMasterToBattleServer::MasterToBattleServerStart(const WCHAR *szMasterNetServerFileName, CMasterServer *pCMasterServer)
{	
	InitializeSRWLock(&m_BattleServerInfoMapSRWLock);

	m_pBattleServerInfoMemoryPool = new CTLSMemoryPool<st_BATTLE_SERVER_INFO*>(1, false);
	if (m_pBattleServerInfoMemoryPool == NULL)
		return false;

	m_pRoomInfoMemoryPool = new CTLSMemoryPool<st_ROOM_INFO>(5, false);
	if (m_pRoomInfoMemoryPool == NULL)
		return false;

	m_iBattleServerNo = 0;
	NumOfBattleServerSessionAll = 0;
	NumOfBattleServerLoginSession = 0;

	if (!MasterToBattleServerOptionParsing(szMasterNetServerFileName))
		return false;

	m_pMasterServer = pCMasterServer;

	return true;
}

void CMasterToBattleServer::MasterToBattleServerStop()
{

}

void CMasterToBattleServer::OnClientJoin(UINT64 ClientID/* Client 정보 / ClientID / 기타등등 */)
{
	InterlockedIncrement(&NumOfBattleServerSessionAll);
}

void CMasterToBattleServer::OnClientLeave(UINT64 ClientID)
{
	// m_BattleClientMap 임계영역 시작(읽기)
	AcquireSRWLockShared(&m_BattleServerInfoMapSRWLock);
	//EnterCriticalSection(&m_csBattleServerInfoLock);

	auto BattleServerMapIter = m_BattleServerInfoMap.find(ClientID);
	if (BattleServerMapIter == m_BattleServerInfoMap.end())
	{
		InterlockedDecrement(&NumOfBattleServerSessionAll);

		// m_BattleClientMap 임계영역 끝  (읽기)
		ReleaseSRWLockShared(&m_BattleServerInfoMapSRWLock);
		return;
	}
	int ServerNo = m_BattleServerInfoMap.find(ClientID)->second->iServerNo;

	// m_BattleClientMap 임계영역 끝  (읽기)
	ReleaseSRWLockShared(&m_BattleServerInfoMapSRWLock);

	// m_RoomInfoList 임계영역 시작
	EnterCriticalSection(&m_csRoomInfoLock);

	list<st_ROOM_INFO*>::iterator ListEndIter = m_RoomInfoList.end();
	for (auto TravelIter = m_RoomInfoList.begin(); TravelIter != ListEndIter; ++TravelIter)
	{
		if ((*TravelIter)->iBattleServerNo == ServerNo)
		{
			(*TravelIter)->UserClientKeySet.clear();
			m_pRoomInfoMemoryPool->Free(*TravelIter);
			m_RoomInfoList.erase(TravelIter);
		}
	}

	// m_RoomInfoList 임계영역 끝
	LeaveCriticalSection(&m_csRoomInfoLock);

	// m_PlayStandbyRoomMap 임계영역 시작
	EnterCriticalSection(&m_csPlayStandbyRoomLock);
	
	map<UINT64, st_ROOM_INFO*>::iterator MapEndIter = m_PlayStandbyRoomMap.end();

	for (auto TravelIter = m_PlayStandbyRoomMap.begin(); TravelIter != MapEndIter; ++TravelIter)
	{
		if (TravelIter->second->iBattleServerNo == ServerNo)
		{
			TravelIter->second->UserClientKeySet.clear();
			m_pRoomInfoMemoryPool->Free(TravelIter->second);
			TravelIter = m_PlayStandbyRoomMap.erase(TravelIter);
			--TravelIter;
		}
	}

	// m_PlayStandbyRoomMap 임계영역 끝
	LeaveCriticalSection(&m_csPlayStandbyRoomLock);

	// m_BattleClientMap 임계영역 시작(쓰기)
	AcquireSRWLockExclusive(&m_BattleServerInfoMapSRWLock);
	//EnterCriticalSection(&m_csBattleServerInfoLock);

	m_BattleServerInfoMap.erase(ClientID);

	// m_BattleClientMap 임계영역 끝  (쓰기)
	ReleaseSRWLockExclusive(&m_BattleServerInfoMapSRWLock);
	//LeaveCriticalSection(&m_csBattleServerInfoLock);

	InterlockedDecrement(&NumOfBattleServerSessionAll);
	InterlockedDecrement(&NumOfBattleServerLoginSession);
}

bool CMasterToBattleServer::OnConnectionRequest()
{
	return true;
}

void CMasterToBattleServer::OnRecv(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf)
{
	WORD Type;

	*OutReadBuf >> Type;

	switch (Type)
	{
	// SERVER_ON 을 제외한 모든 프로시저 함수에서는 
	// 반드시 해당 서버가 인증이 된 서버인지를 확인 후 진행하게 해야함
	
	case en_PACKET_BAT_MAS_REQ_LEFT_USER:
		MasterToBattleLeftUserProc(ReceivedSessionID, OutReadBuf);
		break;

	case en_PACKET_BAT_MAS_REQ_CLOSED_ROOM:
		MasterToBattleClosedRoomProc(ReceivedSessionID, OutReadBuf);
		break;

	case en_PACKET_BAT_MAS_REQ_CREATED_ROOM:
		MasterToBattleCreatedRoomProc(ReceivedSessionID, OutReadBuf);
		break;

	case en_PACKET_BAT_MAS_REQ_CONNECT_TOKEN:
		MasterToBattleConnectTokenProc(ReceivedSessionID, OutReadBuf);
		break;

	case en_PACKET_BAT_MAS_REQ_SERVER_ON:
		MasterToBattleServerOnProc(ReceivedSessionID, OutReadBuf);
		break;

	default:
		break;
	}
}

void CMasterToBattleServer::OnSend(UINT64 ClientID, int sendsize)
{

}

void CMasterToBattleServer::OnWorkerThreadBegin()
{

}

void CMasterToBattleServer::OnWorkerThreadEnd()
{

}

void CMasterToBattleServer::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != WSAECONNRESET)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"CHATSERVERERR ", L"%d\n%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

bool CMasterToBattleServer::MasterToBattleServerOptionParsing(const WCHAR *szOptionFileName)
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
