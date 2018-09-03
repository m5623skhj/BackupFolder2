#include "PreComfile.h"
#include "ChatServer.h"
#include "Log.h"
#include "NetServerSerializeBuffer.h"
#include "CommonProtocol.h"

CChatServer::CChatServer(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle, UINT MaxClient) :
	/*m_bIsUpdate(TRUE),*/ m_uiAccountNo(0)
{
	_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR", L"Start\n");
	m_pMemoryPool = new CTLSMemoryPool<st_USER>(200, false);
	m_pMessageMemoryPool = new CTLSMemoryPool<st_MESSAGE>(1000, false);
	m_pMessageQueue = new CLockFreeQueue<st_MESSAGE*>(100);

	SetLogLevel(LOG_LEVEL::LOG_DEBUG);

	m_hUpdateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hUpdateThreadHandle = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, this, 0, NULL);
	//m_hUpdateIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	Start(IP, PORT, NumOfWorkerThread, IsNagle, MaxClient);
}

CChatServer::~CChatServer()
{
	Stop();
	SetEvent(m_hExitEvent);
	//PostQueuedCompletionStatus(m_hUpdateIOCP, dfSTOP_SERVER, NULL, NULL);
	//m_bIsUpdate = FALSE;
	//while (1)
	//{
	//	SetEvent(m_hUpdateEvent);
	//	if (WaitForSingleObject(m_hUpdateThreadHandle, dfWAIT_UPDATETHREAD_TIMEOUT) != WAIT_TIMEOUT)
	//		break;
	//}

	for (int i = 0; i < dfMAX_SECTOR_Y; ++i)
	{
		for (int j = 0; j < dfMAX_SECTOR_X; ++j)
		{
			m_UserSectorMap[i][j].clear();
		}
	}
	for (auto iter = m_UserSessionMap.begin(); iter != m_UserSessionMap.end(); ++iter)
	{
		delete (iter->second);
		m_UserSessionMap.erase(iter);
	}

	_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR", L"End\n%d");
}

void CChatServer::OnClientJoin(UINT64 JoinClientID)
{
	st_USER &pNewClient = *m_pMemoryPool->Alloc();
	pNewClient.uiSessionID = JoinClientID;

	m_UserSessionMap.insert({ JoinClientID, &pNewClient });
}

void CChatServer::OnClientLeave(UINT64 LeaveClientID)
{
	st_USER &LeaveClient = *m_UserSessionMap.find(LeaveClientID)->second;
	if (&LeaveClient == m_UserSessionMap.end()->second)
	{
		st_Error Err;
		Err.GetLastErr = 0;
		Err.NetServerErr = NOT_IN_USER_MAP_ERR;
		OnError(&Err);
		return;
	}
	m_UserSessionMap.erase(LeaveClientID);

	if (m_UserSectorMap[LeaveClient.SectorX][LeaveClient.SectorY].erase(LeaveClientID) == 0)
	{
		st_Error Err;
		Err.GetLastErr = 0;
		Err.NetServerErr = NOT_IN_USER_MAP_ERR;
		OnError(&Err);
		return;
	}

	m_pMemoryPool->Free(&LeaveClient);
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
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n", OutError->GetLastErr, OutError->NetServerErr, OutError->Line);
		printf_s("==============================================================\n");
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

	while (1)
	{
		//DWORD Transferred = 0;
		//GetQueuedCompletionStatus(UpdateHandle, &Transferred, NULL, NULL, INFINITE);
		if (WaitForMultipleObjects(2, UpdateHandle, FALSE, INFINITE) == WAIT_OBJECT_0/*m_bIsUpdate*//*Transferred != dfSTOP_SERVER*/)
		{
			st_MESSAGE *pRecvMessage = nullptr;
			CLockFreeQueue<st_MESSAGE*> &MessageQ = *m_pMessageQueue;
			
			while (MessageQ.GetRestSize() != 0)
			{
				// 업데이트 큐에서 뽑아오기
				// 해당 업데이트 큐에 뽑아오는 구조체에 세션 ID 가 들어있으므로
				// 그 구조체의 세션 ID 를 기준으로 맵에서 찾아내어 유저를 찾아옴
				MessageQ.Dequeue(&pRecvMessage);

				CNetServerSerializationBuf *pSendPacket = CNetServerSerializationBuf::Alloc();
				CNetServerSerializationBuf *pPacket = pRecvMessage->Packet;
				UINT64 SessionID = pRecvMessage->uiSessionID;
				WORD PacketType;
				*pPacket >> PacketType;

				switch (PacketType)
				{
				case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
					PacketProc_SectorMove(pPacket, pSendPacket, SessionID);
					break;
				case en_PACKET_CS_CHAT_REQ_MESSAGE:
					PacketProc_ChatMessage(pPacket, pSendPacket, SessionID);
					break;
				case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
					//PacketProc_HeartBeat(pPacket, pSendPacket);
					break;
				case en_PACKET_CS_CHAT_REQ_LOGIN:
					//PacketProc_Login(pPacket, pSendPacket, SessionID);
					break;

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
				SendPacket(SessionID, pSendPacket);

				m_pMessageMemoryPool->Free(pRecvMessage);
				CNetServerSerializationBuf::Free(pPacket);
				CNetServerSerializationBuf::Free(pSendPacket);
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
			for (auto iter = m_UserSectorMap[SecotrY][SecotrX].begin(); iter != m_UserSectorMap[SecotrY][SecotrX].end(); ++iter)
			{
				CNetServerSerializationBuf::AddRefCount(SendBuf);
				SendPacket(iter->second->uiSessionID, SendBuf);
			}
		}
	}
}