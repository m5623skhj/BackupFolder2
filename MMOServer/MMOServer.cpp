#include "PreCompile.h"
#include "MMOServer.h"
#include "ServerCommon.h"
#include "NetServerSerializeBuffer.h"
#include "Log.h"
#include "Queue.h"

#include "Profiler.h"

#pragma comment(lib, "ws2_32")

CMMOServer::CMMOServer()
{

}

CMMOServer::~CMMOServer()
{

}

bool CMMOServer::InitMMOServer(UINT NumOfMaxClient)
{
	m_uiNumOfMaxClient = NumOfMaxClient;
	m_ppSessionPointerArray = new CSession*[m_uiNumOfMaxClient];

	return true;
}

bool CMMOServer::Start(const WCHAR *szMMOServerOptionFileName)
{
	if (!OptionParsing(szMMOServerOptionFileName))
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::PARSING_ERR;
		return false;
	}
	m_uiNumOfUser = 0;

	int retval;
	WSADATA Wsa;
	if (WSAStartup(MAKEWORD(2, 2), &Wsa))
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::WSASTARTUP_ERR;
		OnError(&Error);
		return false;
	}

	m_ListenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_ListenSock == INVALID_SOCKET)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::LISTEN_SOCKET_ERR;
		OnError(&Error);
		return false;
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(m_wPort);
	retval = bind(m_ListenSock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::LISTEN_BIND_ERR;
		OnError(&Error);
		return false;
	}

	retval = listen(m_ListenSock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::LISTEN_LISTEN_ERR;
		OnError(&Error);
		return false;
	}

	//CSession::AcceptUserInfo *AcceptInfoPointerArray = new CSession::AcceptUserInfo [m_uiNumOfMaxClient];
	
	for (int i = m_uiNumOfMaxClient - 1; i >= 0; --i)
	{
		//AcceptInfoPointerArray[i].AcceptedSock = INVALID_SOCKET;
		//AcceptInfoPointerArray[i].wAcceptedPort = 0;
		//AcceptInfoPointerArray[i].wSessionIndex = i;

		//m_SessionInfoStack.Push(&AcceptInfoPointerArray[i]);
		m_SessionInfoStack.Push(i);

		m_ppSessionPointerArray[i]->m_uiIOCount = 0;
	}

	m_pUserInfoMemoryPool = new CTLSMemoryPool<CSession::AcceptUserInfo>(3, false);

	bool KeepAlive = true;
	retval = setsockopt(m_ListenSock, SOL_SOCKET, SO_KEEPALIVE, (char*)&KeepAlive, sizeof(KeepAlive));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SETSOCKOPT_ERR;
		return false;
	}

	retval = setsockopt(m_ListenSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&m_bIsNagleOn, sizeof(int));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SETSOCKOPT_ERR;
		return false;
	}

	m_hWorkerIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, m_byNumOfUsingWorkerThread);
	if (m_hWorkerIOCP == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::WORKERIOCP_NULL_ERR;
		return false;
	}

	NumOfAuthPlayer = 0;
	NumOfGamePlayer = 0;
	NumOfTotalPlayer = 0;

	NumOfTotalAccept = 0;

	RecvTPS = 0;
	SendTPS = 0;
	AcceptTPS = 0;

	SendThreadLoop = 0;
	AuthThreadLoop = 0;
	GameThreadLoop = 0;

	m_hThreadExitEvent[en_EVENT_NUMBER::EVENT_SEND] = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hThreadExitEvent[en_EVENT_NUMBER::EVENT_AUTH] = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hThreadExitEvent[en_EVENT_NUMBER::EVENT_GAME] = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hThreadExitEvent[en_EVENT_NUMBER::EVENT_RELEASE] = CreateEvent(NULL, TRUE, FALSE, NULL);

	m_pWorkerThreadHandle = new HANDLE[m_byNumOfWorkerThread];
	// static 함수에서 NetServer 객체를 접근하기 위하여 this 포인터를 인자로 넘김
	for (int i = 0; i < m_byNumOfWorkerThread; i++)
		m_pWorkerThreadHandle[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThreadStartAddress, this, 0, NULL);
	m_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThreadStartAddress, this, 0, NULL);
	
	m_hSendThread[0] = (HANDLE)_beginthreadex(NULL, 0, SendThreadStartAddress, this, 0, NULL);
	m_hSendThread[1] = (HANDLE)_beginthreadex(NULL, 0, SendThreadStartAddress, this, 0, NULL);
	
	m_hAuthThread = (HANDLE)_beginthreadex(NULL, 0, AuthThreadStartAddress, this, 0, NULL);
	m_hGameThread = (HANDLE)_beginthreadex(NULL, 0, GameThreadStartAddress, this, 0, NULL);
	
	m_hReleaseThread = (HANDLE)_beginthreadex(NULL, 0, ReleaseThreadStartAddress, this, 0, NULL);

	if (m_hAcceptThread == 0 || m_pWorkerThreadHandle == 0 || m_hSendThread == 0
		|| m_hAuthThread == 0 || m_hGameThread == 0 || m_hReleaseThread == 0)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::BEGINTHREAD_ERR;
		OnError(&Error);
		return false;
	}

	return true;
}

void CMMOServer::Stop()
{
	closesocket(m_ListenSock);
	WaitForSingleObject(m_hAcceptThread, INFINITE);
	for (UINT i = 0; i < m_byNumOfUsingWorkerThread; ++i)
	{
		if (m_ppSessionPointerArray[i]->m_byNowMode != CSession::en_SESSION_MODE::MODE_NONE)
		{
			shutdown(m_ppSessionPointerArray[i]->m_UserNetworkInfo.AcceptedSock, SD_BOTH);
		}
	}

	for (int i = 0; i < dfNUM_OF_THREAD; ++i)
		SetEvent(m_hThreadExitEvent[i]);

	WaitForMultipleObjects(dfNUM_OF_THREAD, m_hThreadExitEvent, TRUE, INFINITE);

	PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, (LPOVERLAPPED)1);
	WaitForMultipleObjects(m_byNumOfWorkerThread, m_pWorkerThreadHandle, TRUE, INFINITE);

	//CSession::AcceptUserInfo *AcceptInfo;
	//for (UINT i = 0; i < m_uiNumOfMaxClient; ++i)
	//{
	//	m_SessionInfoStack.Pop(&AcceptInfo);
	//	delete AcceptInfo;
	//}
	delete[] m_ppSessionPointerArray;

	CloseHandle(m_hAcceptThread);
	for (int i = 0; i < dfNUM_OF_THREAD; ++i)
		CloseHandle(m_hThreadExitEvent[i]);

	for (int i = 0; i < m_byNumOfUsingWorkerThread; ++i)
	{
		CloseHandle(m_pWorkerThreadHandle[i]);
	}
	delete[] m_pWorkerThreadHandle;
}

void CMMOServer::LinkSession(CSession *PlayerPointer, UINT PlayerIndex)
{
	m_ppSessionPointerArray[PlayerIndex] = PlayerPointer;
}

UINT CMMOServer::AcceptThread()
{
	SOCKET clientsock;
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);

	while (1)
	{
		clientsock = accept(m_ListenSock, (SOCKADDR*)&clientaddr, &addrlen);
		if (clientsock == INVALID_SOCKET)
		{
			int Err = WSAGetLastError();
			if (Err == WSAEINTR)
			{
				break;
			}
			else
			{
				st_Error Error;
				Error.GetLastErr = Err;
				Error.ServerErr = SERVER_ERR::ACCEPT_ERR;
				OnError(&Error);
			}
		}

		CSession::AcceptUserInfo *UserInfo = m_pUserInfoMemoryPool->Alloc();
		//m_SessionInfoStack.Pop(&UserInfo);
		
		InetNtop(AF_INET, (const void*)&clientaddr.sin_addr, UserInfo->AcceptedIP, 16);
		UserInfo->AcceptedSock = clientsock;
		UserInfo->wAcceptedPort = clientaddr.sin_port;

		m_AcceptUserInfoQueue.Enqueue(UserInfo);
		
		NumOfTotalAccept++;
		InterlockedIncrement(&AcceptTPS);
	}

	return 0;
}

UINT CMMOServer::WorkerThread()
{
	char cPostRetval;
	int retval;
	DWORD Transferred;
	CSession *pSession;
	LPOVERLAPPED lpOverlapped;
	HANDLE &WorkerIOCP = m_hWorkerIOCP;

	while (1)
	{
		cPostRetval = -1;
		Transferred = 0;
		pSession = NULL;
		lpOverlapped = NULL;

		Begin("GQCS");
		GetQueuedCompletionStatus(WorkerIOCP, &Transferred, (PULONG_PTR)&pSession, &lpOverlapped, INFINITE);
		End("GQCS");
		if (lpOverlapped != NULL)
		{
			// 외부 종료코드에 의한 종료
			if (pSession == NULL)
			{
				PostQueuedCompletionStatus(WorkerIOCP, 0, 0, (LPOVERLAPPED)1);
				break;
			}
			// recv 파트
			CSession &IOCompleteSession = *pSession;
			if (lpOverlapped == &IOCompleteSession.m_RecvIOData.RecvOverlapped)
			{
				// 클라이언트가 종료함
				if (Transferred == 0)
				{
					// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
					if (InterlockedDecrement(&IOCompleteSession.m_uiIOCount) == 0)
						IOCompleteSession.m_bLogOutFlag = true;

					continue;
				}

				Begin("Recv");
				IOCompleteSession.m_RecvIOData.RecvRingBuffer.MoveWritePos(Transferred);
				int RingBufferRestSize = IOCompleteSession.m_RecvIOData.RecvRingBuffer.GetUseSize();

				// 해당 세션에 링버퍼 크기가 NetServer 직렬화 버퍼 헤더 사이즈보다 크면
				while (RingBufferRestSize > df_HEADER_SIZE)
				{
					// 새 직렬화 버퍼를 할당 받음
					CNetServerSerializationBuf &RecvSerializeBuf = *CNetServerSerializationBuf::Alloc();
					// 패킷의 크기가 얼마인지 알 수 없으므로 Peek 함수를 이용하여 데이터만 복사함
					IOCompleteSession.m_RecvIOData.RecvRingBuffer.Peek((char*)RecvSerializeBuf.m_pSerializeBuffer, df_HEADER_SIZE);
					// 직렬화 버퍼 Read 가 (헤더로 인하여)5 이므로 임의로 0으로 변경함
					RecvSerializeBuf.m_iRead = 0;

					BYTE Code;
					WORD PayloadLength;
					RecvSerializeBuf >> Code >> PayloadLength;

					if (Code != CNetServerSerializationBuf::m_byHeaderCode)
					{
						shutdown(IOCompleteSession.m_UserNetworkInfo.AcceptedSock, SD_BOTH);
						st_Error Err;
						Err.GetLastErr = 0;
						Err.ServerErr = HEADER_CODE_ERR;
						OnError(&Err);
						CNetServerSerializationBuf::Free(&RecvSerializeBuf);
						break;
					}
					if (RingBufferRestSize < PayloadLength + df_HEADER_SIZE)
					{
						if (PayloadLength > dfDEFAULTSIZE)
						{
							shutdown(IOCompleteSession.m_UserNetworkInfo.AcceptedSock, SD_BOTH);
							st_Error Err;
							Err.GetLastErr = 0;
							Err.ServerErr = PAYLOAD_SIZE_OVER_ERR;
							OnError(&Err);
						}
						CNetServerSerializationBuf::Free(&RecvSerializeBuf);
						break;
					}
					// 위 조건문들에서 링버퍼가 가지고 있는 사이즈가 크거나 같은게 판단되었으므로
					// 헤더 사이즈를 지움
					IOCompleteSession.m_RecvIOData.RecvRingBuffer.RemoveData(df_HEADER_SIZE);

					// 할당받은 직렬화 버퍼에 데이터를 복사함과 동시에 지움
					retval = IOCompleteSession.m_RecvIOData.RecvRingBuffer.Dequeue(&RecvSerializeBuf.m_pSerializeBuffer[RecvSerializeBuf.m_iWrite], PayloadLength);
					RecvSerializeBuf.m_iWrite += retval;
					if (!RecvSerializeBuf.Decode())
					{
						shutdown(IOCompleteSession.m_UserNetworkInfo.AcceptedSock, SD_BOTH);
						st_Error Err;
						Err.GetLastErr = 0;
						Err.ServerErr = CHECKSUM_ERR;
						OnError(&Err);
						CNetServerSerializationBuf::Free(&RecvSerializeBuf);
						break;
					}

					RingBufferRestSize -= (retval + df_HEADER_SIZE);
					// Auth 혹은 Game 스레드가 처리해 주기를 기대하고
					// 각 세션이 지니고 있는 Recv완료큐 에 넣어줌
					IOCompleteSession.m_RecvCompleteQueue.Enqueue(&RecvSerializeBuf);
					InterlockedIncrement(&RecvTPS);
				}

				cPostRetval = RecvPost(&IOCompleteSession);
				End("Recv");
			}
			// send 파트
			else if (lpOverlapped == &IOCompleteSession.m_SendIOData.SendOverlapped)
			{
				Begin("Send");
				int BufferCount = IOCompleteSession.m_SendIOData.uiBufferCount;
				for (int i = 0; i < BufferCount; ++i)
					CNetServerSerializationBuf::Free(IOCompleteSession.pSeirializeBufStore[i]);

				InterlockedAdd((LONG*)&SendTPS, BufferCount);
				// 정상적으로 처리가 완료되었을 경우 버퍼 카운터가 0이 됨
				// 버퍼 카운터는 SendThread 에서만 접근하기 때문임
				IOCompleteSession.m_SendIOData.uiBufferCount = 0;

				IOCompleteSession.m_bSendIOMode = NONSENDING;
				cPostRetval = POST_RETVAL_COMPLETE;
				End("Send");
			}
		}
		// overlapped 가 NULL 이면 정상적인 상황이라고 간주하지 않음
		else
		{
			st_Error Error;
			Error.GetLastErr = WSAGetLastError();
			Error.ServerErr = SERVER_ERR::OVERLAPPED_NULL_ERR;
			OnError(&Error);

			g_Dump.Crash();
		}

		// 이미 세션이 함수부에서 지워졌다면 다시 위로 올라감
		if (cPostRetval == POST_RETVAL_ERR_SESSION_DELETED)
			continue;
		// 세션의 IOCount 를 깎고 0 이면 다른 스레드가 지워주길 기대하며
		// 플래그를 true 로 변경함
		if (InterlockedDecrement(&pSession->m_uiIOCount) == 0)
			pSession->m_bLogOutFlag = true;
	}

	CNetServerSerializationBuf::ChunkFreeForcibly();
	return 0;
}

UINT CMMOServer::SendThread()
{
	UINT NumOfMaxClient = m_uiNumOfMaxClient;

	WORD SendThreadSleepTime = m_wSendThreadSleepTime;
	HANDLE &SendThreadEvent = m_hThreadExitEvent[en_EVENT_NUMBER::EVENT_SEND];

	WORD NumOfSendProgress = m_wNumOfSendProgress;
	
	CSession **SessionPointerArray = m_ppSessionPointerArray;

	UINT SendThreadLoopStart = InterlockedExchange(&m_uiSendThreadLoopStartValue, m_uiSendThreadLoopStartValue + 1);
	UINT i;

	while (1)
	{
		// Stop 함수가 이벤트를 Set 하지 않는 이상 계속해서 SendThreadSleepTime 을 주기로 반복함
		if (WaitForSingleObject(SendThreadEvent, SendThreadSleepTime) == WAIT_TIMEOUT)
		{
			InterlockedIncrement(&SendThreadLoop);
			i = SendThreadLoopStart;
			// Send 스레드를 2개로 나눴기 때문에 2를 더함
			for (; i < NumOfMaxClient; i += 2)
			{
				CSession &SendSession = *SessionPointerArray[i];

				while (1)
				{
					if (SendSession.m_SendIOData.uiBufferCount > 0)
						break;

					if (SendSession.m_bSendIOMode == NONSENDING)
						SendSession.m_bSendIOMode = SENDING;
					else
						break;

					if (SendSession.m_bLogOutFlag)
					{
						SendSession.m_bSendIOMode = NONSENDING;
						break;
					}

					int SendQUseSize = SendSession.m_SendIOData.SendQ.GetRestSize();
					if (SendQUseSize == 0)
					{
						SendSession.m_bSendIOMode = NONSENDING;
						// 굳이 필요 없을 듯 함
						//if (SendSession.m_SendIOData.SendQ.GetRestSize() > 0)
							//continue;
						break;
					}
					//else if (SendQUseSize < 0)
					//{
					//	if (InterlockedDecrement(&SendSession.m_uiIOCount) == 0)
					//		SendSession.m_bLogOutFlag = true;
					//	break;
					//}

					Begin("SendBufInit");
					WSABUF wsaSendBuf[dfONE_SEND_WSABUF_MAX];
					// 지정한 사이즈 만큼만 보낼것임
					if (dfONE_SEND_WSABUF_MAX < SendQUseSize)
						SendQUseSize = dfONE_SEND_WSABUF_MAX;

					for (int SendCount = 0; SendCount < SendQUseSize; ++SendCount)
					{
						SendSession.m_SendIOData.SendQ.Dequeue(&SendSession.pSeirializeBufStore[SendCount]);
						wsaSendBuf[SendCount].buf = SendSession.pSeirializeBufStore[SendCount]->m_pSerializeBuffer;//GetBufferPtr();
						wsaSendBuf[SendCount].len = SendSession.pSeirializeBufStore[SendCount]->m_iWriteLast;
					}

					// 대입된 값은 배열 내부에 얼마나 직렬화 버퍼가 남아있는지를 판단하기 위한 값임
					// 대입된 값은 완료통지에서 0으로 바꿔지거나
					// 혹은 실패하였을 경우 Game 스레드가 안에 있는 릴리즈 파트에서 0으로 바꿀것임
					SendSession.m_SendIOData.uiBufferCount = SendQUseSize;

					InterlockedIncrement(&SendSession.m_uiIOCount);
					ZeroMemory(&SendSession.m_SendIOData.SendOverlapped, sizeof(OVERLAPPED));
					End("SendBufInit");

					Begin("WSASend");
					int retval = WSASend(SendSession.m_UserNetworkInfo.AcceptedSock, wsaSendBuf, SendQUseSize, NULL, 0, &SendSession.m_SendIOData.SendOverlapped, 0);
					if (retval == SOCKET_ERROR)
					{
						int err = WSAGetLastError();
						if (err != ERROR_IO_PENDING)
						{
							if (err == WSAENOBUFS)
								g_Dump.Crash();
							st_Error Error;
							Error.GetLastErr = err;
							Error.ServerErr = SERVER_ERR::WSASEND_ERR;
							OnError(&Error);

							if (InterlockedDecrement(&SendSession.m_uiIOCount) == 0)
								SendSession.m_bLogOutFlag = true;
							//else
							//	shutdown(SendSession.m_pUserNetworkInfo->AcceptedSock, SD_BOTH);

							// 릴리즈 조건이 Send 상태가 아니여야 되므로 강제로 NONSENDING 상태로 변경함
							//InterlockedExchange(&SendSession.m_bSendIOMode, NONSENDING);
							SendSession.m_bSendIOMode = NONSENDING;
							End("WSASend");
							break;
						}
					}
					End("WSASend");
				}
			}
		}
		else
			break;
	}

	return 0;
}

UINT CMMOServer::AuthThread()
{
	UINT NumOfMaxClient = m_uiNumOfMaxClient;

	WORD NumOfAuthProgress = m_wNumOfAuthProgress;
	WORD LoopAuthHandlingPacket = m_wLoopAuthHandlingPacket;
	WORD AuthThreadSleepTime = m_wAuthThreadSleepTime;

	HANDLE &AuthThreadEvent = m_hThreadExitEvent[en_EVENT_NUMBER::EVENT_AUTH];
	
	CSession **SessionPointerArray = m_ppSessionPointerArray;

	HANDLE &WorkerIOCP = m_hWorkerIOCP;

	CLockFreeStack<WORD> &SessionIndexStack = m_SessionInfoStack;

	CTLSMemoryPool<CSession::AcceptUserInfo> &UserInfoMemoryPool = *m_pUserInfoMemoryPool;
	
	while (1)
	{
		// Stop 함수가 이벤트를 Set 하지 않는 이상 계속해서 AuthThreadSleepTime 을 주기로 반복함
		if (WaitForSingleObject(AuthThreadEvent, AuthThreadSleepTime) == WAIT_TIMEOUT)
		{
			InterlockedIncrement(&AuthThreadLoop);
			// 현재 얼마만큼의 유저가 큐에서 대기중인지 확인함
			int NumOfAcceptWaitingUser = m_AcceptUserInfoQueue.GetRestSize();

			// 지정한 값 보다 처리해줄 개수가 많다면 지정한 값 만큼만 처리하게 값을 변경함
			if (NumOfAuthProgress < NumOfAcceptWaitingUser)
				NumOfAcceptWaitingUser = NumOfAuthProgress;

			for (int i = 0; i < NumOfAcceptWaitingUser; ++i)
			{
				Begin("Login");
				CSession::AcceptUserInfo *pNewUserAcceptInfo;
				m_AcceptUserInfoQueue.Dequeue(&pNewUserAcceptInfo);
				//WORD SessionIndex = pNewUserAcceptInfo->wSessionIndex;
				WORD SessionIndex;
				SessionIndexStack.Pop(&SessionIndex);

				CSession &LoginSession = *SessionPointerArray[SessionIndex];
				LoginSession.m_wSessionIndex = SessionIndex;

				memcpy(LoginSession.m_UserNetworkInfo.AcceptedIP, pNewUserAcceptInfo->AcceptedIP, 16);
				LoginSession.m_UserNetworkInfo.AcceptedSock = pNewUserAcceptInfo->AcceptedSock;
				LoginSession.m_UserNetworkInfo.wAcceptedPort = pNewUserAcceptInfo->wAcceptedPort;
				UserInfoMemoryPool.Free(pNewUserAcceptInfo);

				//LoginSession.m_pUserNetworkInfo = pNewUserAcceptInfo;
				//LoginSession.m_Socket = pNewUserAcceptInfo->AcceptedSock;

				CreateIoCompletionPort((HANDLE)LoginSession.m_UserNetworkInfo.AcceptedSock, WorkerIOCP,
					(ULONG_PTR)SessionPointerArray[SessionIndex], NULL);

				LoginSession.OnAuth_ClientJoin();
				LoginSession.m_bLogOutFlag = false;

				RecvPost(&LoginSession);
				// 현재 모드를 바꾸면 이 객체에 대한 처리가 시작되므로
				// 가장 마지막에 바꿔줌
				LoginSession.m_byNowMode = CSession::en_SESSION_MODE::MODE_AUTH;

				InterlockedIncrement(&NumOfAuthPlayer);
				InterlockedIncrement(&NumOfTotalPlayer);
				End("Login");
			}

			for (UINT i = 0; i < NumOfMaxClient; ++i)
			{
				CSession &NowSession = *SessionPointerArray[i];

				if (NowSession.m_byNowMode != CSession::en_SESSION_MODE::MODE_AUTH)
					continue;
				
				CNetServerSerializationBuf *RecvPacket;
				// 지정한 수 만큼만 완료통지가 온 패킷을 처리함
				for (int NumOfDequeue = 0; NumOfDequeue < LoopAuthHandlingPacket; ++NumOfDequeue)
				{
					if (NowSession.m_RecvCompleteQueue.Dequeue(&RecvPacket))
					{
						Begin("AuthRecvPacket");
						NowSession.OnAuth_Packet(RecvPacket);
						CNetServerSerializationBuf::Free(RecvPacket);
						End("AuthRecvPacket");
						// 외부에서 Game 모드로 변경했다면 
						// 더이상 Auth 에서는 처리하면 안되므로 반복문을 탈출함
						if (NowSession.m_bAuthToGame == true)
							break;
					}
					// 더이상 처리할 패킷이 없으면 반복문을 탈출함
					else
						break;
				}
			}

			OnAuth_Update();

			for (UINT i = 0; i < NumOfMaxClient; ++i)
			{
				CSession &NowSession = *SessionPointerArray[i];
				
				if (NowSession.m_bLogOutFlag)
				{
					// Auth 모드이고 Send 중이 아닌 세션만 로그아웃 대기 상태로 진입이 가능함
					if (NowSession.m_byNowMode == CSession::en_SESSION_MODE::MODE_AUTH && NowSession.m_bSendIOMode == NONSENDING)
					{
						NowSession.m_byNowMode = CSession::en_SESSION_MODE::MODE_WAIT_LOGOUT;
						NowSession.OnAuth_ClientLeave(true);

						InterlockedDecrement(&NumOfAuthPlayer);
					}
				}
				// 게임으로 넘어갈 세션이고 동시에 모드가 Auth 모드면
				else if (NowSession.m_byNowMode == CSession::en_SESSION_MODE::MODE_AUTH && NowSession.m_bAuthToGame )
				{
					// Game 스레드가 이 세션을 가져갈 수 있도록 상태를 변경함
					NowSession.m_byNowMode = CSession::en_SESSION_MODE::MODE_AUTH_TO_GAME;
					NowSession.OnAuth_ClientLeave(false);

					InterlockedDecrement(&NumOfAuthPlayer);
				}
			}
		}
		else
			break;
	}

	return 0;
}

UINT CMMOServer::GameThread()
{
	UINT NumOfMaxClient = m_uiNumOfMaxClient;
	UINT ModeChangeStartPoint = 0;

	WORD NumOfGameProgress = m_wNumOfGameProgress;
	WORD LoopGameHandlingPacket = m_wLoopGameHandlingPacket;
	WORD GameThreadSleepTime = m_wGameThreadSleepTime;

	HANDLE &GameThreadEvent = m_hThreadExitEvent[en_EVENT_NUMBER::EVENT_GAME];

	CSession **SessionPointerArray = m_ppSessionPointerArray;
	
	WORD NumOfModeChangeCycle;

	while (1)
	{
		// Stop 함수가 이벤트를 Set 하지 않는 이상 계속해서 GameThreadSleepTime 을 주기로 반복함
		if (WaitForSingleObject(GameThreadEvent, GameThreadSleepTime) == WAIT_TIMEOUT)
		{
			InterlockedIncrement(&GameThreadLoop);
			NumOfModeChangeCycle = 0;

			for (UINT i = ModeChangeStartPoint; i < NumOfMaxClient; ++i)
			{
				CSession &NowSession = *SessionPointerArray[i];
				// Auth 에서 Game 으로 넘어오려는 세션이라면
				if (NowSession.m_byNowMode == CSession::en_SESSION_MODE::MODE_AUTH_TO_GAME)
				{
					Begin("AuthToGame");
					// 모드를 Game 으로 변경함
					NowSession.m_byNowMode = CSession::en_SESSION_MODE::MODE_GAME;
					NowSession.OnGame_ClientJoin();

					++NumOfModeChangeCycle;
					ModeChangeStartPoint = i;

					InterlockedIncrement(&NumOfGamePlayer);
					End("AuthToGame");

					// 지정한 처리 횟수보다 반복문에서 처리한게 크거나 같으면 반복문을 탈출함
					if (NumOfModeChangeCycle >= NumOfGameProgress)
					{
						// 반복한 횟수가 클라이언트 최대치면
						if (i == NumOfMaxClient - 1)
							// 반복문 시작값을 0으로 변경함
							ModeChangeStartPoint = 0;
						break;
					}
				}
			}

			// 지정한 처리횟수가 처리 횟수보다 크면 처리 횟수를 0으로 변경함
			if (NumOfModeChangeCycle < NumOfGameProgress)
				ModeChangeStartPoint = 0;

			for (UINT i = 0; i < NumOfMaxClient; ++i)
			{
				CSession &NowSession = *SessionPointerArray[i];

				if (NowSession.m_byNowMode != CSession::en_SESSION_MODE::MODE_GAME)
					continue;

				CNetServerSerializationBuf *RecvPacket;
				// 지정한 처리 횟수만큼만 처리함
				for (int NumOfDequeue = 0; NumOfDequeue < LoopGameHandlingPacket; ++NumOfDequeue)
				{
					if (NowSession.m_RecvCompleteQueue.Dequeue(&RecvPacket))
					{
						Begin("GameRecv");
						NowSession.OnGame_Packet(RecvPacket);
						CNetServerSerializationBuf::Free(RecvPacket);
						End("GameRecv");
					}
					// 더이상 완료된 패킷이 없다면 반복문을 탈출함
					else
						break;
				}
			}

			OnGame_Update();

			//Begin("GameRelease");
			for (UINT i = 0; i < NumOfMaxClient; ++i)
			{
				CSession &NowSession = *SessionPointerArray[i];
				if (NowSession.m_bLogOutFlag)
				{
					// Game 상태이고 Send 중이 아니라면 
					if (NowSession.m_byNowMode == CSession::en_SESSION_MODE::MODE_GAME && NowSession.m_bSendIOMode == NONSENDING)
					{
						// 로그아웃 대기 상태로 변경함
						NowSession.m_byNowMode = CSession::en_SESSION_MODE::MODE_WAIT_LOGOUT;
						NowSession.OnAuth_ClientLeave(true);
						InterlockedDecrement(&NumOfGamePlayer);
					}
				}
			}

			/*
			//	if (NowSession.m_byNowMode == CSession::en_SESSION_MODE::MODE_WAIT_LOGOUT)
			//	{
			//		int SendBufferRestSize = NowSession.m_SendIOData.uiBufferCount;
			//		int Rest = NowSession.m_SendIOData.SendQ.GetRestSize();
			//		// Send 스레드 에서 옮겨졌으나 보내지 못한 직렬화 버퍼들을 반환함
			//		for (int SendRestCnt = 0; SendRestCnt < SendBufferRestSize; ++SendRestCnt)
			//			CNetServerSerializationBuf::Free(NowSession.pSeirializeBufStore[SendRestCnt]);

			//		NowSession.m_SendIOData.uiBufferCount = 0;

			//		// 큐에서 아직 꺼내오지 못한 직렬화 버퍼가 있다면 해당 직렬화 버퍼들을 반환함
			//		if (Rest > 0)
			//		{
			//			CNetServerSerializationBuf *DeleteBuf;
			//			for (int SendQRestSize = 0; SendQRestSize < Rest; ++SendQRestSize)
			//			{
			//				NowSession.m_SendIOData.SendQ.Dequeue(&DeleteBuf);
			//				CNetServerSerializationBuf::Free(DeleteBuf);
			//			}
			//		}

			//		// 아직 완료큐에서 꺼내지지 않은 직렬화 버퍼가 있다면 해당 직렬화 버퍼들을 반환함
			//		CNetServerSerializationBuf *DeleteBuf;
			//		while (1)
			//		{
			//			if (!NowSession.m_RecvCompleteQueue.Dequeue(&DeleteBuf))
			//				break;
			//			CNetServerSerializationBuf::Free(DeleteBuf);
			//		}

			//		CSession::AcceptUserInfo *UserNetworkInfo = NowSession.m_pUserNetworkInfo;
			//		WORD SessionIndex = NowSession.m_wSessionIndex;
			//		closesocket(UserNetworkInfo->AcceptedSock);
			//		UserNetworkInfo->AcceptedSock = INVALID_SOCKET;

			//		NowSession.m_bAuthToGame = false;
			//		NowSession.m_byNowMode = CSession::en_SESSION_MODE::MODE_NONE;
			//		NowSession.m_pUserNetworkInfo = NULL;
			//		NowSession.m_Socket = INVALID_SOCKET;
			//		NowSession.m_RecvIOData.RecvRingBuffer.InitPointer();

			//		// 해당 UserNetworkInfo 를 재사용 할 수 있도록 스택에 넣음
			//		m_SessionInfoStack.Push(SessionIndex);
			//		m_pUserInfoMemoryPool->Free(UserNetworkInfo);

			//		NowSession.OnGame_ClientRelease();

			//		InterlockedDecrement(&NumOfTotalPlayer);
			//	}
			//}
			//End("GameRelease");
			*/
		}
		else
			break;
	}

	return 0;
}

UINT CMMOServer::ReleaseThread()
{
	UINT NumOfMaxClient = m_uiNumOfMaxClient;

	WORD GameThreadSleepTime = m_wReleaseThreadSleepTime;

	HANDLE &ReleaseThreadEvent = m_hThreadExitEvent[en_EVENT_NUMBER::EVENT_RELEASE];

	CSession **SessionPointerArray = m_ppSessionPointerArray;

	CLockFreeStack<WORD> &SessionIndexStack = m_SessionInfoStack;

	while (1)
	{
		// Stop 함수가 이벤트를 Set 하지 않는 이상 계속해서 ReleaseThreadSleepTime 을 주기로 반복함
		if (WaitForSingleObject(ReleaseThreadEvent, GameThreadSleepTime) == WAIT_TIMEOUT)
		{
			for (UINT i = 0; i < NumOfMaxClient; ++i)
			{
				CSession &NowSession = *SessionPointerArray[i];

				if (NowSession.m_byNowMode == CSession::en_SESSION_MODE::MODE_WAIT_LOGOUT)
				{
					Begin("GameRelease");
					int SendBufferRestSize = NowSession.m_SendIOData.uiBufferCount;
					int Rest = NowSession.m_SendIOData.SendQ.GetRestSize();
					// Send 스레드 에서 옮겨졌으나 보내지 못한 직렬화 버퍼들을 반환함
					for (int SendRestCnt = 0; SendRestCnt < SendBufferRestSize; ++SendRestCnt)
						CNetServerSerializationBuf::Free(NowSession.pSeirializeBufStore[SendRestCnt]);

					NowSession.m_SendIOData.uiBufferCount = 0;

					// 큐에서 아직 꺼내오지 못한 직렬화 버퍼가 있다면 해당 직렬화 버퍼들을 반환함
					if (Rest > 0)
					{
						CNetServerSerializationBuf *DeleteBuf;
						for (int SendQRestSize = 0; SendQRestSize < Rest; ++SendQRestSize)
						{
							NowSession.m_SendIOData.SendQ.Dequeue(&DeleteBuf);
							CNetServerSerializationBuf::Free(DeleteBuf);
						}
					}

					// 아직 완료큐에서 꺼내지지 않은 직렬화 버퍼가 있다면 해당 직렬화 버퍼들을 반환함
					CNetServerSerializationBuf *DeleteBuf;
					while (1)
					{
						if (!NowSession.m_RecvCompleteQueue.Dequeue(&DeleteBuf))
							break;
						CNetServerSerializationBuf::Free(DeleteBuf);
					}

					//CSession::AcceptUserInfo *UserNetworkInfo = NowSession.m_pUserNetworkInfo;
					closesocket(NowSession.m_UserNetworkInfo.AcceptedSock);
					//UserNetworkInfo->AcceptedSock = INVALID_SOCKET;

					NowSession.m_bAuthToGame = false;
					NowSession.m_byNowMode = CSession::en_SESSION_MODE::MODE_NONE;
					NowSession.m_UserNetworkInfo.AcceptedIP[0] = L'\0';
					NowSession.m_UserNetworkInfo.wAcceptedPort = NULL;
					NowSession.m_UserNetworkInfo.AcceptedSock = INVALID_SOCKET;
					NowSession.m_RecvIOData.RecvRingBuffer.InitPointer();

					// 해당 UserNetworkInfo 를 재사용 할 수 있도록 스택에 넣음
					SessionIndexStack.Push(NowSession.m_wSessionIndex);
					//m_pUserInfoMemoryPool->Free(UserNetworkInfo);

					NowSession.OnGame_ClientRelease();

					InterlockedDecrement(&NumOfTotalPlayer);
					End("GameRelease");
				}
			}
		}
		else
			break;
	}

	return 0;
}

char CMMOServer::RecvPost(CSession *pSession)
{
	CSession &RecvSession = *pSession;
	// 링버퍼가 꽉 찼다면 정상적인 세션으로 간주하지 않음
	if (RecvSession.m_RecvIOData.RecvRingBuffer.IsFull())
	{
		if (InterlockedDecrement(&RecvSession.m_uiIOCount) == 0)
		{
			RecvSession.m_bLogOutFlag = true;
			return POST_RETVAL_ERR_SESSION_DELETED;
		}
		return POST_RETVAL_ERR;
	}

	Begin("RecvPost");
	int BrokenSize = RecvSession.m_RecvIOData.RecvRingBuffer.GetNotBrokenPutSize();
	int RestSize = RecvSession.m_RecvIOData.RecvRingBuffer.GetFreeSize() - BrokenSize;
	int BufCount = 1;
	DWORD Flag = 0;

	WSABUF wsaBuf[2];
	wsaBuf[0].buf = RecvSession.m_RecvIOData.RecvRingBuffer.GetWriteBufferPtr();
	wsaBuf[0].len = BrokenSize;
	if (RestSize > 0)
	{
		wsaBuf[1].buf = RecvSession.m_RecvIOData.RecvRingBuffer.GetBufferPtr();
		wsaBuf[1].len = RestSize;
		BufCount++;
	}

	InterlockedIncrement(&RecvSession.m_uiIOCount);
	ZeroMemory(&RecvSession.m_RecvIOData.RecvOverlapped, sizeof(OVERLAPPED));
	int retval = WSARecv(RecvSession.m_UserNetworkInfo.AcceptedSock, wsaBuf, BufCount, NULL, &Flag, &RecvSession.m_RecvIOData.RecvOverlapped, 0);
	if (retval == SOCKET_ERROR)
	{
		int GetLastErr = WSAGetLastError();
		if (GetLastErr != ERROR_IO_PENDING)
		{
			if (InterlockedDecrement(&RecvSession.m_uiIOCount) == 0)
			{
				RecvSession.m_bLogOutFlag = true;
				End("RecvPost");
				return POST_RETVAL_ERR_SESSION_DELETED;
			}
			st_Error Error;
			Error.GetLastErr = GetLastErr;
			Error.ServerErr = SERVER_ERR::WSARECV_ERR;
			OnError(&Error);
			End("RecvPost");
			return POST_RETVAL_ERR;
		}
	}
	End("RecvPost");
	return POST_RETVAL_COMPLETE;
}

UINT CMMOServer::AcceptThreadStartAddress(LPVOID CMMOServerPointer)
{
	return ((CMMOServer*)CMMOServerPointer)->AcceptThread();
}

UINT CMMOServer::WorkerThreadStartAddress(LPVOID CMMOServerPointer)
{
	return ((CMMOServer*)CMMOServerPointer)->WorkerThread();
}

UINT CMMOServer::SendThreadStartAddress(LPVOID CMMOServerPointer)
{
	return ((CMMOServer*)CMMOServerPointer)->SendThread();
}

UINT CMMOServer::AuthThreadStartAddress(LPVOID CMMOServerPointer)
{
	return ((CMMOServer*)CMMOServerPointer)->AuthThread();
}

UINT CMMOServer::GameThreadStartAddress(LPVOID CMMOServerPointer)
{
	return ((CMMOServer*)CMMOServerPointer)->GameThread();
}

UINT CMMOServer::ReleaseThreadStartAddress(LPVOID CMMOServerPointer)
{
	return ((CMMOServer*)CMMOServerPointer)->ReleaseThread();
}

bool CMMOServer::OptionParsing(const WCHAR *szMMOServerOptionFileName)
{
	_wsetlocale(LC_ALL, L"Korean");

	CParser parser;
	WCHAR cBuffer[BUFFER_MAX];

	FILE *fp;
	_wfopen_s(&fp, szMMOServerOptionFileName, L"rt, ccs=UNICODE");

	int iJumpBOM = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int iFileSize = ftell(fp);
	fseek(fp, iJumpBOM, SEEK_SET);
	int FileSize = (int)fread_s(cBuffer, BUFFER_MAX, sizeof(WCHAR), BUFFER_MAX / 2, fp);
	int iAmend = iFileSize - FileSize; // 개행 문자와 파일 사이즈에 대한 보정값
	fclose(fp);

	cBuffer[iFileSize - iAmend] = '\0';
	WCHAR *pBuff = cBuffer;

	BYTE HeaderCode, XORCode1, XORCode2, DebugLevel;

	if (!parser.GetValue_Short(pBuff, L"MMOSERVER", L"SEND_SLEEPTIME", (short*)&m_wSendThreadSleepTime))
		return false;
	if (!parser.GetValue_Short(pBuff, L"MMOSERVER", L"AUTH_SLEEPTIME", (short*)&m_wAuthThreadSleepTime))
		return false;
	if (!parser.GetValue_Short(pBuff, L"MMOSERVER", L"GAME_SLEEPTIME", (short*)&m_wGameThreadSleepTime))
		return false;
	if (!parser.GetValue_Short(pBuff, L"MMOSERVER", L"RELEASE_SLEEPTIME", (short*)&m_wReleaseThreadSleepTime))
		return false;
	if (!parser.GetValue_Short(pBuff, L"MMOSERVER", L"SEND_NUMOFPROGRESS", (short*)&m_wNumOfSendProgress))
		return false;
	if (!parser.GetValue_Short(pBuff, L"MMOSERVER", L"AUTH_NUMOFPROGRESS", (short*)&m_wNumOfAuthProgress))
		return false;
	if (!parser.GetValue_Short(pBuff, L"MMOSERVER", L"GAME_NUMOFPROGRESS", (short*)&m_wNumOfGameProgress))
		return false;
	if (!parser.GetValue_Short(pBuff, L"MMOSERVER", L"AUTH_NUMOFPACKET", (short*)&m_wLoopAuthHandlingPacket))
		return false;
	if (!parser.GetValue_Short(pBuff, L"MMOSERVER", L"GAME_NUMOFPACKET", (short*)&m_wLoopGameHandlingPacket))
		return false;

	if (!parser.GetValue_String(pBuff, L"MMOSERVER", L"BIND_IP", m_IP))
		return false;
	if (!parser.GetValue_Short(pBuff, L"MMOSERVER", L"BIND_PORT", (short*)&m_wPort))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"MMOSERVER", L"WORKER_THREAD", &m_byNumOfWorkerThread))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"MMOSERVER", L"USE_IOCPWORKER", &m_byNumOfUsingWorkerThread))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"MMOSERVER", L"NAGLE_ON", (BYTE*)&m_bIsNagleOn))
		return false;

	if (!parser.GetValue_Byte(pBuff, L"SERIALIZEBUF", L"PACKET_CODE", &HeaderCode))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"SERIALIZEBUF", L"PACKET_KEY1", &XORCode1))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"SERIALIZEBUF", L"PACKET_KEY2", &XORCode2))
		return false;

	if (!parser.GetValue_Byte(pBuff, L"OPTION", L"LOG_LEVEL", &DebugLevel))
		return false;

	CNetServerSerializationBuf::m_byHeaderCode = HeaderCode;
	CNetServerSerializationBuf::m_byXORCode = XORCode1 ^ XORCode2;
	SetLogLevel(DebugLevel);

	return true;
}

UINT CMMOServer::GetAcceptTPSAndReset()
{
	UINT NowAcceptTPS = AcceptTPS;
	InterlockedExchange(&AcceptTPS, 0);

	return NowAcceptTPS;
}

UINT CMMOServer::GetRecvTPSAndReset()
{
	UINT NowRecvTPS = RecvTPS;
	InterlockedExchange(&RecvTPS, 0);

	return NowRecvTPS;
}

UINT CMMOServer::GetSendTPSAndReset()
{
	UINT NowSendTPS = SendTPS;
	InterlockedExchange(&SendTPS, 0);

	return NowSendTPS;
}

UINT CMMOServer::GetSendThreadFPSAndReset()
{
	UINT NowSendFPS = SendThreadLoop;
	InterlockedExchange(&SendThreadLoop, 0);

	return NowSendFPS;
}

UINT CMMOServer::GetAuthThreadFPSAndReset()
{
	UINT NowAuthFPS = AuthThreadLoop;
	InterlockedExchange(&AuthThreadLoop, 0);

	return NowAuthFPS;
}

UINT CMMOServer::GetGameThreadFPSAndReset()
{
	UINT NowGameFPS = GameThreadLoop;
	InterlockedExchange(&GameThreadLoop, 0);

	return NowGameFPS;
}

int CMMOServer::GetRecvCompleteQueueTotalNodeCount()
{
	return CListBaseQueue<CNetServerSerializationBuf*>::GetMemoryPoolTotalNodeCount();
}