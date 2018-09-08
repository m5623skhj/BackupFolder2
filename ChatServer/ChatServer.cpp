#include "PreComfile.h"
#include "ChatServer.h"
#include "Log.h"
#include "NetServerSerializeBuffer.h"
#include "CommonProtocol.h"

CChatServer::CChatServer(/*const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle, UINT MaxClient*/) :
	/*m_bIsUpdate(TRUE),*/ m_uiAccountNo(0), m_uiUpdateTPS(0)
{
	//_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR", L"Start\n");
	//m_pUserMemoryPool = new CTLSMemoryPool<st_USER>(200, false);
	//m_pMessageMemoryPool = new CTLSMemoryPool<st_MESSAGE>(1000, false);
	//m_pMessageQueue = new CLockFreeQueue<st_MESSAGE*>(100);

	//SetLogLevel(LOG_LEVEL::LOG_DEBUG);

	//m_hUpdateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	//m_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	//m_hUpdateThreadHandle = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, this, 0, NULL);
	////m_hUpdateIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	//Start(IP, PORT, NumOfWorkerThread, IsNagle, MaxClient);
}

CChatServer::~CChatServer()
{
	//Stop();
	//SetEvent(m_hExitEvent);
	////PostQueuedCompletionStatus(m_hUpdateIOCP, dfSTOP_SERVER, NULL, NULL);
	////m_bIsUpdate = FALSE;
	////while (1)
	////{
	////	SetEvent(m_hUpdateEvent);
	////	if (WaitForSingleObject(m_hUpdateThreadHandle, dfWAIT_UPDATETHREAD_TIMEOUT) != WAIT_TIMEOUT)
	////		break;
	////}

	//for (int i = 0; i < dfMAX_SECTOR_Y; ++i)
	//{
	//	for (int j = 0; j < dfMAX_SECTOR_X; ++j)
	//	{
	//		m_UserSectorMap[i][j].clear();
	//	}
	//}
	//for (auto iter = m_UserSessionMap.begin(); iter != m_UserSessionMap.end(); ++iter)
	//{
	//	delete (iter->second);
	//	m_UserSessionMap.erase(iter);
	//}

	//CloseHandle(m_hUpdateThreadHandle);
	//CloseHandle(m_hExitEvent);
	//CloseHandle(m_hUpdateEvent);

	//delete m_pMessageQueue;
	//delete m_pMessageMemoryPool;
	//delete m_pUserMemoryPool;

	//_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR", L"End\n%d");
}

bool CChatServer::ChattingServerStart(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle, UINT MaxClient)
{
	_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR", L"Start\n");
	InitializeCriticalSection(&m_UserSessionLock);
	m_pUserMemoryPool = new CTLSMemoryPool<st_USER>(200, false);
	m_pMessageMemoryPool = new CTLSMemoryPool<st_MESSAGE>(50, false);
	m_pMessageQueue = new CLockFreeQueue<st_MESSAGE*>(0);

	SetLogLevel(LOG_LEVEL::LOG_DEBUG);

	m_hUpdateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	//m_hUpdateIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (!Start(IP, PORT, NumOfWorkerThread, IsNagle, MaxClient))
		return false;
	m_hUpdateThreadHandle = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, this, 0, NULL);

	return true;
}

void CChatServer::ChattingServerStop()
{
	Stop();
	SetEvent(m_hExitEvent);
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
		m_UserSessionMap.erase(iter);
	}

	CloseHandle(m_hUpdateThreadHandle);
	CloseHandle(m_hExitEvent);
	CloseHandle(m_hUpdateEvent);

	delete m_pMessageQueue;
	delete m_pMessageMemoryPool;
	delete m_pUserMemoryPool;

	_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR", L"End\n%d");
	WritingProfile();
}

void CChatServer::OnClientJoin(UINT64 JoinClientID)
{
	st_MESSAGE *pMessage = m_pMessageMemoryPool->Alloc();
	//CNetServerSerializationBuf *PlayerJoinPacket = CNetServerSerializationBuf::Alloc();

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
	//CNetServerSerializationBuf *PlayerLeavePacket = CNetServerSerializationBuf::Alloc();

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
	//	Err.NetServerErr = NOT_IN_USER_MAP_ERR;
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
	//	Err.NetServerErr = NOT_IN_USER_SECTORMAP_ERR;
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
	//PostQueuedCompletionStatus(m_hUpdateIOCP, dfRECVED_PACKET, NULL, NULL);
	SetEvent(m_hUpdateEvent);
}

void CChatServer::OnSend()
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
		//_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n", OutError->GetLastErr, OutError->NetServerErr, OutError->Line);
		//printf_s("==============================================================\n");
	}
}

UINT __stdcall CChatServer::UpdateThread(LPVOID pChatServer)
{
	return ((CChatServer*)pChatServer)->Updater();
}

UINT CChatServer::Updater()
{
	//HANDLE UpdateHandle = m_hUpdateIOCP;
	HANDLE UpdateHandle[2];
	UpdateHandle[0] = m_hUpdateEvent;
	UpdateHandle[1] = m_hExitEvent;

	st_MESSAGE *pRecvMessage = nullptr;
	CLockFreeQueue<st_MESSAGE*> &MessageQ = *m_pMessageQueue;

	while (1)
	{
		//DWORD Transferred = 0;
		//GetQueuedCompletionStatus(UpdateHandle, &Transferred, NULL, NULL, INFINITE);

		if (WaitForMultipleObjects(2, UpdateHandle, FALSE, INFINITE) == WAIT_OBJECT_0/*m_bIsUpdate*//*Transferred != dfSTOP_SERVER*/)
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
					Begin("SectorMove");
					pSendPacket = CNetServerSerializationBuf::Alloc();
					PacketProc_SectorMove(pPacket, pSendPacket, SessionID);
					CNetServerSerializationBuf::Free(pSendPacket);
					End("SectorMove");
					break;
				case en_PACKET_CS_CHAT_REQ_MESSAGE:
					Begin("Chatting");
					pSendPacket = CNetServerSerializationBuf::Alloc();
					PacketProc_ChatMessage(pPacket, pSendPacket, SessionID);
					CNetServerSerializationBuf::Free(pSendPacket);
					End("Chatting");
					break;
				case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
					//PacketProc_HeartBeat(pPacket, pSendPacket);
					break;
				case en_PACKET_CS_CHAT_REQ_LOGIN:
					Begin("LogIn");
					pSendPacket = CNetServerSerializationBuf::Alloc();
					PacketProc_Login(pPacket, pSendPacket, SessionID);
					CNetServerSerializationBuf::Free(pSendPacket);
					End("LogIn");
					break;
				case dfPACKET_TYPE_JOIN:
					Begin("Join");
					PacketProc_PlayerJoin(SessionID);
					m_pMessageMemoryPool->Free(pRecvMessage);
					End("Join");
					continue;
				case dfPACKET_TYPE_LEAVE:
					Begin("Leave");
					PacketProc_PlayerLeave(SessionID);
					m_pMessageMemoryPool->Free(pRecvMessage);
					End("Leave");
					continue;
				default:
					// 에러 및 해당 세션 끊기
					st_Error Err;
					Err.GetLastErr = 0;
					Err.NetServerErr = INCORREC_PACKET_ERR;
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
			//if (!m_bIsUpdate)
			//	break;

		}
		else
		{
			break;
		}
	}

	// 청크들을 어떻게 반환하지?
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
				//CNetServerSerializationBuf::AddRefCount(SendBuf);
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

