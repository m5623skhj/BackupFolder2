#include "PreCompile.h"
#include "ChatServer.h"
#include "Log.h"
#include "NetServerSerializeBuffer.h"
#include "CommonProtocol.h"
#include "ChattingLanClient.h"
#include "ChatMonitoringLanClient.h"

CChatServer::CChatServer() : 
	m_uiAccountNo(0), m_uiUpdateTPS(0), m_iNumOfRecvJoinPacket(0), m_iNumOfSessionKeyMiss(0), 
	m_iNumOfSessionKeyNotFound(0)
{
}

CChatServer::~CChatServer()
{

}

bool CChatServer::ChattingServerStart(const WCHAR *szChatServerOptionFileName, const WCHAR *szChatServerLanClientFileName, const WCHAR *szMonitoringClientFileName)
{
	_LOG(LOG_LEVEL::LOG_DEBUG, L"ServerStart", L"Start\n");
	m_pSessionKeyMemoryPool = new CTLSMemoryPool<st_SessionKey>(5, false);
	m_pUserMemoryPool = new CTLSMemoryPool<st_USER>(20, false);
	m_pMessageMemoryPool = new CTLSMemoryPool<st_MESSAGE>(30, false);
	m_pMessageQueue = new CLockFreeQueue<st_MESSAGE*>;
	m_pChattingLanClient = new CChattingLanClient();
	m_pMonitoringLanClient = new CChatMonitoringLanClient();

	SetLogLevel(LOG_LEVEL::LOG_DEBUG);

	m_hUpdateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	m_uiNumOfSessionAll = 0;
	m_uiNumOfLoginCompleteUser = 0;

	if (!Start(szChatServerOptionFileName))
		return false;
	wprintf(L"NetServer Start\n");
	if (!m_pChattingLanClient->ChattingLanClientStart(this, szChatServerLanClientFileName, m_pSessionKeyMemoryPool, (UINT64)&m_UserSessionKeyMap))
		return false;
	wprintf(L"LanClient Start\n");
	if (!m_pMonitoringLanClient->MonitoringLanClientStart(szMonitoringClientFileName, &m_uiNumOfSessionAll, &m_uiNumOfLoginCompleteUser))
		return false;
	wprintf(L"Monitoring Client Connect\n");

	m_hUpdateThreadHandle = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, this, 0, NULL);
	m_hSessionKeyHeartBeatThreadHandle = (HANDLE)_beginthreadex(NULL, 0, SessionKeyHeartbeatCheckThread, this, 0, NULL);

	m_pSessionKeyMapCS = m_pChattingLanClient->GetSessionKeyMapCS();

	wprintf(L"Chatting Server Start\n");
	return true;
}

void CChatServer::ChattingServerStop()
{
	Stop();
	m_pChattingLanClient->ChattingLanClientStop();
	m_pMonitoringLanClient->MonitoringLanClientStop();

	SetEvent(m_hExitEvent);
	// HeartBeat
	// WaitForMultipleObject(~~~);
	WaitForSingleObject(m_hUpdateThreadHandle, INFINITE);

	for (int i = 0; i < dfMAX_SECTOR_Y; ++i)
	{
		for (int j = 0; j < dfMAX_SECTOR_X; ++j)
		{
			m_UserSectorMap[i][j].clear();
		}
	}

	auto iterEnd = m_UserSessionMap.end();
	for (auto iter = m_UserSessionMap.begin(); iter != iterEnd; ++iter)
	{
		delete (iter->second);
		iter = m_UserSessionMap.erase(iter);
		--iter;
	}

	CloseHandle(m_hUpdateThreadHandle);
	CloseHandle(m_hExitEvent);
	CloseHandle(m_hUpdateEvent);

	delete m_pMessageQueue;
	delete m_pMessageMemoryPool;
	delete m_pUserMemoryPool;
	delete m_pSessionKeyMemoryPool;

	_LOG(LOG_LEVEL::LOG_DEBUG, L"ServerEnd", L"End\n%d");
	WritingProfile();
}

void CChatServer::OnClientJoin(UINT64 JoinClientID)
{
	st_MESSAGE *pMessage = m_pMessageMemoryPool->Alloc();

	//WORD Type = dfPACKET_TYPE_JOIN;
	//*PlayerJoinPacket << Type;
	pMessage->wType = dfPACKET_TYPE_JOIN;
	pMessage->uiSessionID = JoinClientID;
	m_pMessageQueue->Enqueue(pMessage);
	SetEvent(m_hUpdateEvent);

	//EnterCriticalSection(&m_UserSessionLock);
	//m_UserSessionMap.insert({ JoinClientID, &NewClient });
	//LeaveCriticalSection(&m_UserSessionLock);
}

void CChatServer::OnClientLeave(UINT64 LeaveClientID)
{
	st_MESSAGE *pMessage = m_pMessageMemoryPool->Alloc();

	//WORD Type = dfPACKET_TYPE_LEAVE;
	//*PlayerLeavePacket << Type;
	pMessage->wType = dfPACKET_TYPE_LEAVE;
	pMessage->uiSessionID = LeaveClientID;
	m_pMessageQueue->Enqueue(pMessage);
	SetEvent(m_hUpdateEvent);

	//st_USER &LeaveClient = *m_UserSessionMap.find(LeaveClientID)->second;
	//if (&LeaveClient == m_UserSessionMap.end()->second)
	//{
	//	st_Error Err;
	//	Err.GetLastErr = 0;
	//	Err.ServerErr = NOT_IN_USER_MAP_ERR;
	//	OnError(&Err);
	//	return;
	//}
	//EnterCriticalSection(&m_UserSessionLock);
	//m_UserSessionMap.erase(LeaveClientID);
	//LeaveCriticalSection(&m_UserSessionLock);
	//if (LeaveClient.SectorX > dfMAX_SECTOR_X || LeaveClient.SectorY > dfMAX_SECTOR_Y)
	//{
	//	m_pUserMemoryPool->Free(&LeaveClient);
	//	return;
	//}
	//if (m_UserSectorMap[LeaveClient.SectorY][LeaveClient.SectorX].erase(LeaveClientID) == 0)
	//{
	//	st_Error Err;
	//	Err.GetLastErr = 0;
	//	Err.ServerErr = NOT_IN_USER_SECTORMAP_ERR;
	//	OnError(&Err);
	//	return;
	//}
	//m_pUserMemoryPool->Free(&LeaveClient);
}

bool CChatServer::OnConnectionRequest(const WCHAR *IP)
{
	return true;
}

void CChatServer::OnRecv(UINT64 ReceivedSessionID, CNetServerSerializationBuf *ServerReceivedBuffer)
{
	CNetServerSerializationBuf::AddRefCount(ServerReceivedBuffer);
	st_MESSAGE *pMessage = m_pMessageMemoryPool->Alloc();
	pMessage->uiSessionID = ReceivedSessionID;
	pMessage->Packet = ServerReceivedBuffer;
	*ServerReceivedBuffer >> pMessage->wType;

	m_pMessageQueue->Enqueue(pMessage);
	SetEvent(m_hUpdateEvent);
}

void CChatServer::OnSend(UINT64 ClientID, int sendsize)
{

}

void CChatServer::OnWorkerThreadBegin()
{

}

void CChatServer::OnWorkerThreadEnd()
{

}

void CChatServer::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != 10054)
	{
		//g_Dump.Crash();
		_LOG(LOG_LEVEL::LOG_DEBUG, L"CHATSERVERERR ", L"%d\n%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		//printf_s("==============================================================\n");
	}
}

UINT __stdcall CChatServer::UpdateThread(LPVOID pChatServer)
{
	return ((CChatServer*)pChatServer)->Updater();
}

UINT __stdcall CChatServer::HeartbeatCheckThread(LPVOID pChatServer)
{
	return ((CChatServer*)pChatServer)->HeartbeatChecker();
}

UINT __stdcall CChatServer::SessionKeyHeartbeatCheckThread(LPVOID pChatServer)
{
	return ((CChatServer*)pChatServer)->SessionKeyHeartbeatChecker();
}

bool CChatServer::LoginPacketRecvedFromLoginServer(UINT64 AccountNo, st_SessionKey *SessionKey)
{
	//if (m_UserSessionKeyMap.find(AccountNo) != m_UserSessionKeyMap.end())
	//	return false;

	CNetServerSerializationBuf *RecvLoginServerPacket = CNetServerSerializationBuf::Alloc();
	st_MESSAGE *pMessage = m_pMessageMemoryPool->Alloc();

	*RecvLoginServerPacket << AccountNo << (UINT64)SessionKey;
	pMessage->Packet = RecvLoginServerPacket;
	pMessage->wType = dfPACKET_TYPE_RECV_FROM_LOGIN_SERVER;

	m_pMessageQueue->Enqueue(pMessage);
	SetEvent(m_hUpdateEvent);

	return true;
}

void CChatServer::OverWriteSessionKey(UINT64 AccountNo, st_SessionKey *pSessionKey)
{
	// 세션키를 지우기 위하여 이전 세션키를 받음
	st_SessionKey *NowSessionKey = m_UserSessionKeyMap.find(AccountNo)->second;
	// 이전 세션키를 지움
	m_pSessionKeyMemoryPool->Free(NowSessionKey);
	// 현재 받아온 세션키를 이전 정보에 덮어 씀
	NowSessionKey = pSessionKey;

	//st_USER &OverWirteUser = *m_UserSessionMap.find(AccountNo)->second;
	//
	//// 차후 Leave 에서 현재 정보를 삭제하지 못하도록 플래그를 올림
	//++OverWirteUser.byNumOfOverWriteSessionKey;
	//// 이전에 섹터 맵에 자신이 존재할 경우 자신을 지움
	//if (OverWirteUser.SectorX < dfMAX_SECTOR_X && OverWirteUser.SectorY < dfMAX_SECTOR_Y)
	//	m_UserSectorMap[OverWirteUser.SectorY][OverWirteUser.SectorX].erase(AccountNo);
}

UINT CChatServer::Updater()
{
	HANDLE UpdateHandle[2];
	UpdateHandle[0] = m_hUpdateEvent;
	UpdateHandle[1] = m_hExitEvent;

	st_MESSAGE *pRecvMessage = nullptr;
	CLockFreeQueue<st_MESSAGE*> &MessageQ = *m_pMessageQueue;

	while (1)
	{
		if (WaitForMultipleObjects(2, UpdateHandle, FALSE, INFINITE) == WAIT_OBJECT_0)
		{
			// 메시지 큐가 빌 때 까지
			while (MessageQ.GetRestSize() != 0)
			{
				// 업데이트 큐에서 뽑아오기
				// 해당 업데이트 큐에 뽑아오는 구조체에 세션 ID 가 들어있으므로
				// 그 구조체의 세션 ID 를 기준으로 맵에서 찾아내어 유저를 찾아옴
				MessageQ.Dequeue(&pRecvMessage);

				CNetServerSerializationBuf *pSendPacket;
				CNetServerSerializationBuf *pPacket = pRecvMessage->Packet;
				UINT64 SessionID = pRecvMessage->uiSessionID;

				switch (pRecvMessage->wType)
				{
				case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
					pSendPacket = CNetServerSerializationBuf::Alloc();
					PacketProc_SectorMove(pPacket, pSendPacket, SessionID);
					CNetServerSerializationBuf::Free(pSendPacket);
					break;
				case en_PACKET_CS_CHAT_REQ_MESSAGE:
					pSendPacket = CNetServerSerializationBuf::Alloc();
					PacketProc_ChatMessage(pPacket, pSendPacket, SessionID);
					CNetServerSerializationBuf::Free(pSendPacket);
					break;
				case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
					PacketProc_SessionKeyHeartBeat(pPacket);
					break;
					//PacketProc_HeartBeat(pPacket, pSendPacket);
				case en_PACKET_CS_CHAT_REQ_LOGIN:
					pSendPacket = CNetServerSerializationBuf::Alloc();
					PacketProc_Login(pPacket, pSendPacket, SessionID);
					CNetServerSerializationBuf::Free(pSendPacket);
					break;
				case dfPACKET_TYPE_JOIN:
					PacketProc_PlayerJoin(SessionID);
					m_pMessageMemoryPool->Free(pRecvMessage);
					++m_iNumOfRecvJoinPacket;
					continue;
				case dfPACKET_TYPE_LEAVE:
					PacketProc_PlayerLeave(SessionID);
					m_pMessageMemoryPool->Free(pRecvMessage);
					continue;
				case dfPACKET_TYPE_RECV_FROM_LOGIN_SERVER:
					//PacketProc_PlayerLoginFromLoginServer(pPacket);
					break;
				default:
					// 에러 및 해당 세션 끊기
					st_Error Err;
					Err.GetLastErr = 0;
					Err.ServerErr = INCORREC_PACKET_ERR;
					Err.Line = __LINE__;
					OnError(&Err);
					DisConnect(SessionID);
					break;
				}

				// 다 사용한 메시지 / Recv 패킷 / Send 패킷 을 해제시킴
 				CNetServerSerializationBuf::Free(pPacket);
				m_pMessageMemoryPool->Free(pRecvMessage);
				InterlockedIncrement(&m_uiUpdateTPS);
			}
		}
		else
		{
			break;
		}
	}

	// 청크들을 어떻게 반환하지?
	return 0;
}

UINT CChatServer::HeartbeatChecker()
{
	//HANDLE ExitHandle = m_hExitEvent;
	//while (1)
	//{
	//	if (WaitForSingleObject(ExitHandle, 1000) == WAIT_TIMEOUT)
	//	{
	//		++m_uiHeartBeatTime;
	//		UINT64 NowTime = m_uiHeartBeatTime - dfHEARTBEAT_TIMEOVER;

	//		std::unordered_map<UINT64, st_USER*>::iterator TravelIter = m_UserSessionMap.begin();
	//		std::unordered_map<UINT64, st_USER*>::iterator Enditer = m_UserSessionMap.end();

	//		for (; TravelIter != Enditer; ++TravelIter)
	//		{
	//			if (TravelIter->second->uiBeforeRecvTime < NowTime)
	//			{
	//				CNetServerSerializationBuf *RecvLoginServerPacket = CNetServerSerializationBuf::Alloc();
	//				st_MESSAGE *pMessage = m_pMessageMemoryPool->Alloc();

	//				pMessage->Packet = RecvLoginServerPacket;
	//				pMessage->wType = en_PACKET_CS_CHAT_REQ_HEARTBEAT;

	//				m_pMessageQueue->M_Enqueue(pMessage);
	//				SetEvent(m_hUpdateEvent);
	//			}
	//		}
	//	}
	//	else
	//		break;
	//}

	return 0;
}

UINT CChatServer::SessionKeyHeartbeatChecker()
{
	HANDLE ExitHandle = m_hExitEvent;
	while (1)
	{
		if (WaitForSingleObject(ExitHandle, 5000) == WAIT_TIMEOUT)
		{
			CNetServerSerializationBuf *SendBuf = CNetServerSerializationBuf::Alloc();
			st_MESSAGE *pMessage = m_pMessageMemoryPool->Alloc();
			pMessage->Packet = SendBuf;
			pMessage->wType = en_PACKET_CS_CHAT_REQ_HEARTBEAT;

			m_pMessageQueue->Enqueue(pMessage);
			SetEvent(m_hUpdateEvent);
		}
		else
			break;
	}

	return 0;
}

void CChatServer::BroadcastSectorAroundAll(WORD CharacterSectorX, WORD CharacterSectorY, CNetServerSerializationBuf *SendBuf)
{
	for (int SecotrY = CharacterSectorY - 1; SecotrY < CharacterSectorY + 2; ++SecotrY)
	{
		if (SecotrY < 0)
			continue;
		else if (SecotrY > dfMAX_SECTOR_Y)
			break;
		for (int SecotrX = CharacterSectorX - 1; SecotrX < CharacterSectorX + 2; ++SecotrX)
		{
			if (SecotrX < 0)
				continue;
			else if (SecotrX > dfMAX_SECTOR_X)
				break;
			auto iterEnd = m_UserSectorMap[SecotrY][SecotrX].end();
			for (auto iter = m_UserSectorMap[SecotrY][SecotrX].begin(); iter != iterEnd; ++iter)
			{
				CNetServerSerializationBuf::AddRefCount(SendBuf);
				SendPacket(iter->second->uiSessionID, SendBuf);
			}
		}
	}
}

int CChatServer::GetNumOfPlayer()
{
	return (int)m_UserSessionMap.size();
}

int CChatServer::GetNumOfAllocPlayer()
{
	return m_pUserMemoryPool->GetUseNodeCount();
}

int CChatServer::GetNumOfAllocPlayerChunk()
{
	return m_pUserMemoryPool->GetUseChunkCount();
}

int CChatServer::GetNumOfMessageInPool()
{
	return m_pMessageMemoryPool->GetUseNodeCount();
}

int CChatServer::GetNumOfMessageInPoolChunk()
{
	return m_pMessageMemoryPool->GetUseChunkCount();
}

int CChatServer::GetRestMessageInQueue()
{
	return m_pMessageQueue->GetRestSize();
}

UINT CChatServer::GetUpdateTPSAndReset()
{
	UINT UpdateTPS = m_uiUpdateTPS;
	InterlockedExchange(&m_uiUpdateTPS, 0);

	return UpdateTPS;
}

int CChatServer::GetNumOfClientRecvPacket()
{
	return m_pChattingLanClient->NumOfRecvPacket;
}

int CChatServer::GetNumOfRecvJoinPacket()
{
	return m_iNumOfRecvJoinPacket;
}

int CChatServer::GetNumOfSessionKeyMiss()
{
	return m_iNumOfSessionKeyMiss;
}

int CChatServer::GetNumOfSessionKeyNotFound()
{
	return m_iNumOfSessionKeyNotFound;
}

int CChatServer::GetUsingNetServerBufCount()
{
	return CNetServerSerializationBuf::GetUsingSerializeBufNodeCount();
}

int CChatServer::GetUsingLanServerBufCount()
{
	return m_pChattingLanClient->GetUsingLanServerBufCount();
}

int CChatServer::GetUsingSessionKeyChunkCount()
{
	return m_pSessionKeyMemoryPool->GetUseChunkCount();
}

int CChatServer::GetUsingSessionKeyNodeCount()
{
	return m_pSessionKeyMemoryPool->GetUseNodeCount();
}