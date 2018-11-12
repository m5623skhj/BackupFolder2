#include "PreCompile.h"
#pragma comment(lib, "ws2_32")
#include "NetServer.h"
#include "NetServerSerializeBuffer.h"
#include "CrashDump.h"
#include "Profiler.h"
#include "Log.h"

CNetServer::CNetServer() : m_uiAcceptTotal(0), m_uiAcceptTPS(0)
{
}

CNetServer::~CNetServer()
{

}

bool CNetServer::Start(const WCHAR *szOptionFileName)
{
	if (!NetServerOptionParsing(szOptionFileName))
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::PARSING_ERR;
		return false;
	}

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
	
	m_pSessionArray = new Session[m_uiMaxClient];
	m_pSessionIndexStack = new CLockFreeStack<WORD>();
	for (int i = m_uiMaxClient - 1; i >= 0; --i)
	{
		m_pSessionArray[i].IOCount = 0;
		m_pSessionIndexStack->Push(i);
	}
	m_uiNumOfUser = 0;

	bool KeepAlive = true;
	retval = setsockopt(m_ListenSock, SOL_SOCKET, SO_KEEPALIVE, (char*)&KeepAlive, sizeof(KeepAlive));
	if(retval == SOCKET_ERROR)
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

	//int SendBufSize = 0;
	//retval = setsockopt(m_ListenSock, SOL_SOCKET, SO_SNDBUF, (char*)&SendBufSize, sizeof(SendBufSize));
	//if (retval == SOCKET_ERROR)
	//{
	//	st_Error Error;
	//	Error.GetLastErr = WSAGetLastError();
	//	Error.ServerErr = SERVER_ERR::SETSOCKOPT_ERR;
	//	OnError(&Error);
	//	return false;
	//}

	m_hWorkerIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, m_byNumOfUsingWorkerThread);
	if (m_hWorkerIOCP == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::WORKERIOCP_NULL_ERR;
		return false;
	}

	m_pWorkerThreadHandle = new HANDLE[m_byNumOfWorkerThread];
	// static 함수에서 NetServer 객체를 접근하기 위하여 this 포인터를 인자로 넘김
	for (int i = 0; i < m_byNumOfWorkerThread; i++)
		m_pWorkerThreadHandle[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, NULL);
	m_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, NULL);
	if (m_hAcceptThread == 0 || m_pWorkerThreadHandle == 0)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::BEGINTHREAD_ERR;
		OnError(&Error);
		return false;
	}
	
	return true;
}

void CNetServer::Stop()
{
	closesocket(m_ListenSock);
	WaitForSingleObject(m_hAcceptThread, INFINITE);
	for (UINT i = 0; i < m_uiMaxClient; ++i)
	{
		if (m_pSessionArray[i].IsUseSession)
		{
			shutdown(m_pSessionArray[i].sock, SD_BOTH);
		}
	}

	PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, (LPOVERLAPPED)1);
	WaitForMultipleObjects(m_byNumOfWorkerThread, m_pWorkerThreadHandle, TRUE, INFINITE);

	delete[] m_pSessionArray;
	delete m_pSessionIndexStack;

	CloseHandle(m_hAcceptThread);
	for (BYTE i = 0; i < m_byNumOfWorkerThread; ++i)
		CloseHandle(m_pWorkerThreadHandle[i]);
	CloseHandle(m_hWorkerIOCP);

	WSACleanup();
}

bool CNetServer::NetServerOptionParsing(const WCHAR *szOptionFileName)
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

	BYTE HeaderCode, XORCode1, XORCode2, DebugLevel;

	if (!parser.GetValue_String(pBuff, L"SERVER", L"BIND_IP", m_IP))
		return false;
	if (!parser.GetValue_Short(pBuff, L"SERVER", L"BIND_PORT", (short*)&m_wPort))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"SERVER", L"WORKER_THREAD", &m_byNumOfWorkerThread))
		return false;
	if (!parser.GetValue_Int(pBuff, L"SERVER", L"CLIENT_MAX", (int*)&m_uiMaxClient))
		return false;
	//if (!parser.GetValue_Int(pBuff, L"SERVER", L"MONITOR_NO", MonitorNo))
	//	return false;
	if (!parser.GetValue_Byte(pBuff, L"SERVER", L"USE_IOCPWORKER", &m_byNumOfUsingWorkerThread))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"SERVER", L"NAGLE_ON", (BYTE*)&m_bIsNagleOn))
		return false;

	if (!parser.GetValue_Byte(pBuff, L"SERVER", L"PACKET_CODE", &HeaderCode))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"SERVER", L"PACKET_KEY1", &XORCode1))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"SERVER", L"PACKET_KEY2", &XORCode2))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"SERVER", L"LOG_LEVEL", &DebugLevel))
		return false;

	CNetServerSerializationBuf::m_byHeaderCode = HeaderCode;
	CNetServerSerializationBuf::m_byXORCode = XORCode1 ^ XORCode2;
	SetLogLevel(DebugLevel);

	return true;
}

bool CNetServer::ReleaseSession(Session *pSession)
{
	if (InterlockedCompareExchange64((LONG64*)&pSession->IOCount, 0, df_RELEASE_VALUE) != df_RELEASE_VALUE)
		return false;

	int SendBufferRestSize = pSession->SendIOData.lBufferCount;
	int Rest = pSession->SendIOData.SendQ.GetRestSize();
	// SendPost 에서 옮겨졌으나 보내지 못한 직렬화 버퍼들을 반환함
	for (int i = 0; i < SendBufferRestSize; ++i)
		CNetServerSerializationBuf::Free(pSession->pSeirializeBufStore[i]);

	// 큐에서 아직 꺼내오지 못한 직렬화 버퍼가 있다면 해당 직렬화 버퍼들을 반환함
	if (Rest > 0)
	{
		CNetServerSerializationBuf *DeleteBuf;
		for (int i = 0; i < Rest; ++i)
		{
			pSession->SendIOData.SendQ.Dequeue(&DeleteBuf);
			CNetServerSerializationBuf::Free(DeleteBuf);
		}
	}
	closesocket(pSession->sock);

	UINT64 ID = pSession->SessionID;

	InterlockedDecrement(&m_uiNumOfUser);
	OnClientLeave(ID);

	m_pSessionIndexStack->Push((WORD)(ID >> SESSION_INDEX_SHIFT));

	return true;
}

bool CNetServer::DisConnect(UINT64 SessionID)
{
	WORD StackIndex = (WORD)(SessionID >> SESSION_INDEX_SHIFT);
	if (!SessionAcquireLock(SessionID))
	{
		SessionAcquireUnLock(StackIndex);
		return false;
	}

	shutdown(m_pSessionArray[StackIndex].sock, SD_BOTH);

	SessionAcquireUnLock(StackIndex);

	return true;
}

UINT CNetServer::Accepter()
{
	SOCKET clientsock;
	SOCKADDR_IN clientaddr;
	WCHAR IP[16];
	int addrlen = sizeof(clientaddr);

	CLockFreeStack<WORD> &SesseionIndexStack = *m_pSessionIndexStack;

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

		++m_uiAcceptTotal;
		InterlockedIncrement(&m_uiAcceptTPS);

		inet_ntop(AF_INET, (const void*)&clientaddr, (PSTR)IP, sizeof(IP));
		if (!OnConnectionRequest(IP) || m_uiNumOfUser >= m_uiMaxClient)
		{
			closesocket(clientsock);
			continue;
		}
		
		UINT64 SessionID = InterlockedIncrement(&m_iIDCount);
		// 가장 상위 2 바이트를 Session Stack 의 Index 로 사용함
		WORD Index;
		SesseionIndexStack.Pop(&Index);
		UINT64 SessionIdx = Index;
		SessionID += (SessionIdx << SESSION_INDEX_SHIFT);

		Session &AcceptSession = m_pSessionArray[SessionIdx];
		InterlockedIncrement(&AcceptSession.IOCount);

		AcceptSession.bSendDisconnect = FALSE;
		AcceptSession.sock = clientsock;
		AcceptSession.SessionID = SessionID;

		AcceptSession.RecvIOData.wBufferCount = 0;
		AcceptSession.SendIOData.lBufferCount = 0;
		AcceptSession.RecvIOData.IOMode = 0;
		AcceptSession.SendIOData.IOMode = 0;
		AcceptSession.NowPostQueuing = NONSENDING;

		ZeroMemory(&AcceptSession.RecvIOData.Overlapped, sizeof(OVERLAPPED));
		ZeroMemory(&AcceptSession.SendIOData.Overlapped, sizeof(OVERLAPPED));

		AcceptSession.RecvIOData.RingBuffer.InitPointer();
		AcceptSession.IsUseSession = TRUE;
				
		CreateIoCompletionPort((HANDLE)clientsock, m_hWorkerIOCP, (ULONG_PTR)&AcceptSession, 0);
		InterlockedIncrement(&m_uiNumOfUser);

		OnClientJoin(AcceptSession.SessionID);

		RecvPost(&AcceptSession);

		if (InterlockedDecrement(&AcceptSession.IOCount) == 0)
			ReleaseSession(&AcceptSession);
	}

	CNetServerSerializationBuf::ChunkFreeForcibly();
	return 0;
}

UINT CNetServer::Worker()
{
	char cPostRetval;
	int retval;
	DWORD Transferred;
	Session *pSession;
	LPOVERLAPPED lpOverlapped;

	while (1)
	{
		cPostRetval = -1;
		Transferred = 0;
		pSession = NULL;
		lpOverlapped = NULL;

		GetQueuedCompletionStatus(m_hWorkerIOCP, &Transferred, (PULONG_PTR)&pSession, &lpOverlapped, INFINITE);
		OnWorkerThreadBegin();
		if (lpOverlapped != NULL)
		{
			// recv 파트
			if (pSession != NULL)
			{
				Session &IOCompleteSession = *pSession;
				if (lpOverlapped == &IOCompleteSession.RecvIOData.Overlapped)
				{
					// 클라이언트가 종료함
					if (Transferred == 0)
					{
						// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
						if (InterlockedDecrement(&IOCompleteSession.IOCount) == 0)
							ReleaseSession(&IOCompleteSession);

						continue;
					}

					IOCompleteSession.RecvIOData.RingBuffer.MoveWritePos(Transferred);
					int RingBufferRestSize = IOCompleteSession.RecvIOData.RingBuffer.GetUseSize();
					bool bPacketError = false;

					while (RingBufferRestSize > df_HEADER_SIZE)
					{
						CNetServerSerializationBuf &RecvSerializeBuf = *CNetServerSerializationBuf::Alloc();
						IOCompleteSession.RecvIOData.RingBuffer.Peek((char*)RecvSerializeBuf.m_pSerializeBuffer, df_HEADER_SIZE);
						RecvSerializeBuf.m_iRead = 0;

						BYTE Code;
						WORD PayloadLength;
						RecvSerializeBuf >> Code >> PayloadLength;

						if (Code != CNetServerSerializationBuf::m_byHeaderCode)
						{
							bPacketError = true;
							//cPostRetval = POST_RETVAL_ERR;
							st_Error Err;
							Err.GetLastErr = 0;
							Err.ServerErr = HEADER_CODE_ERR;
							InterlockedIncrement((UINT*)&checksum);
							OnError(&Err);
							CNetServerSerializationBuf::Free(&RecvSerializeBuf);
							break;
						}
						if (RingBufferRestSize < PayloadLength + df_HEADER_SIZE)
						{
							if (PayloadLength > dfDEFAULTSIZE)
							{
								bPacketError = true;
								//cPostRetval = POST_RETVAL_ERR;
								st_Error Err;
								Err.GetLastErr = 0;
								Err.ServerErr = PAYLOAD_SIZE_OVER_ERR;
								InterlockedIncrement((UINT*)&payloadOver);
								OnError(&Err);
							}
							CNetServerSerializationBuf::Free(&RecvSerializeBuf);
							break;
						}
						IOCompleteSession.RecvIOData.RingBuffer.RemoveData(df_HEADER_SIZE);

						retval = IOCompleteSession.RecvIOData.RingBuffer.Dequeue(&RecvSerializeBuf.m_pSerializeBuffer[RecvSerializeBuf.m_iWrite], PayloadLength);
						RecvSerializeBuf.m_iWrite += retval;
						if (!RecvSerializeBuf.Decode())
						{
							bPacketError = true;
							//cPostRetval = POST_RETVAL_ERR;
							st_Error Err;
							Err.GetLastErr = 0;
							Err.ServerErr = CHECKSUM_ERR;
							InterlockedIncrement((UINT*)&HeaderCode);
							OnError(&Err);
							CNetServerSerializationBuf::Free(&RecvSerializeBuf);
							break;
						}

						RingBufferRestSize -= (retval + df_HEADER_SIZE);
						OnRecv(IOCompleteSession.SessionID, &RecvSerializeBuf);
						CNetServerSerializationBuf::Free(&RecvSerializeBuf);
					}
					cPostRetval = RecvPost(&IOCompleteSession);
					if (bPacketError)
						DisConnect(IOCompleteSession.SessionID);
				}
				// send 파트
				else if (lpOverlapped == &IOCompleteSession.SendIOData.Overlapped)
				{
					// 클라이언트가 종료함
					if (Transferred == 0)
					{
						// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
						if (InterlockedDecrement(&IOCompleteSession.IOCount) == 0)
							ReleaseSession(&IOCompleteSession);

						continue;
					}

					int BufferCount = IOCompleteSession.SendIOData.lBufferCount;
					for (int i = 0; i < BufferCount; ++i)
						CNetServerSerializationBuf::Free(IOCompleteSession.pSeirializeBufStore[i]);

					IOCompleteSession.SendIOData.lBufferCount -= BufferCount;

					OnSend(pSession->SessionID, BufferCount);
					if (!IOCompleteSession.bSendDisconnect)
					{
						InterlockedExchange(&IOCompleteSession.SendIOData.IOMode, NONSENDING);
						cPostRetval = SendPost(&IOCompleteSession);
						InterlockedExchange(&IOCompleteSession.NowPostQueuing, NONSENDING);
					}
					else
					{
						DisConnect(IOCompleteSession.SessionID);
						cPostRetval = POST_RETVAL_COMPLETE;
					}
				}
				// send 보내기 파트
				else if ((UINT64)lpOverlapped == dfSEND_POST_QUEUING_OVERLAPPED)
				{
					cPostRetval = SendPost(&IOCompleteSession);
					InterlockedExchange(&IOCompleteSession.NowPostQueuing, NONSENDING);
				}
			}
			// 외부 종료코드에 의한 종료
			else
			{
				PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, (LPOVERLAPPED)1);
				break;
			}
		}
		else
		{
			st_Error Error;
			Error.GetLastErr = WSAGetLastError();
			Error.ServerErr = SERVER_ERR::OVERLAPPED_NULL_ERR;
			OnError(&Error);

			g_Dump.Crash();
		}

		OnWorkerThreadEnd();

		if (cPostRetval == POST_RETVAL_ERR_SESSION_DELETED)
			continue;
		if (InterlockedDecrement(&pSession->IOCount) == 0)
			ReleaseSession(pSession);
	}

	CNetServerSerializationBuf::ChunkFreeForcibly();
	return 0;
}

UINT __stdcall CNetServer::AcceptThread(LPVOID pNetServer)
{
	return ((CNetServer*)pNetServer)->Accepter();
}

UINT __stdcall CNetServer::WorkerThread(LPVOID pNetServer)
{
	return ((CNetServer*)pNetServer)->Worker();
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CNetServer::RecvPost(Session *pSession)
{
	Session &RecvSession = *pSession;
	if (RecvSession.RecvIOData.RingBuffer.IsFull())
	{
		if (InterlockedDecrement(&RecvSession.IOCount) == 0)
		{
			ReleaseSession(&RecvSession);
			return POST_RETVAL_ERR_SESSION_DELETED;
		}
		return POST_RETVAL_ERR;
	}

	int BrokenSize = RecvSession.RecvIOData.RingBuffer.GetNotBrokenPutSize();
	int RestSize = RecvSession.RecvIOData.RingBuffer.GetFreeSize() - BrokenSize;
	int BufCount = 1;
	DWORD Flag = 0;

	WSABUF wsaBuf[2];
	wsaBuf[0].buf = RecvSession.RecvIOData.RingBuffer.GetWriteBufferPtr();
	wsaBuf[0].len = BrokenSize;
	if (RestSize > 0)
	{
		wsaBuf[1].buf = RecvSession.RecvIOData.RingBuffer.GetBufferPtr();
		wsaBuf[1].len = RestSize;
		BufCount++;
	}
	
	InterlockedIncrement(&RecvSession.IOCount);
	ZeroMemory(&RecvSession.RecvIOData.Overlapped, sizeof(OVERLAPPED));
	int retval = WSARecv(pSession->sock, wsaBuf, BufCount, NULL, &Flag, &pSession->RecvIOData.Overlapped, 0);
	if (retval == SOCKET_ERROR)
	{
		int GetLastErr = WSAGetLastError();
		if (GetLastErr != ERROR_IO_PENDING)
		{
			if (InterlockedDecrement(&RecvSession.IOCount) == 0)
			{
				ReleaseSession(&RecvSession);
				return POST_RETVAL_ERR_SESSION_DELETED;
			}
			st_Error Error;
			Error.GetLastErr = GetLastErr;
			Error.ServerErr = SERVER_ERR::WSARECV_ERR;
			OnError(&Error);
			return POST_RETVAL_ERR;
		}
	}
	return POST_RETVAL_COMPLETE;
}

bool CNetServer::SendPacket(UINT64 SessionID, CNetServerSerializationBuf *pSerializeBuf)
{
	WORD StackIndex = (WORD)(SessionID >> SESSION_INDEX_SHIFT);

	if (pSerializeBuf == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SERIALIZEBUF_NULL_ERR;
		OnError(&Error);
		return false;
	}
	
	if (!SessionAcquireLock(SessionID))
	{
		SessionAcquireUnLock(StackIndex);
		CNetServerSerializationBuf::Free(pSerializeBuf);
		return false;
	}

	if (!pSerializeBuf->m_bIsEncoded)
	{
		pSerializeBuf->m_iWriteLast = pSerializeBuf->m_iWrite;
		pSerializeBuf->m_iWrite = 0;
		pSerializeBuf->m_iRead = 0;
		pSerializeBuf->Encode();
	}

	Session &AcquireSession = m_pSessionArray[StackIndex];
	AcquireSession.SendIOData.SendQ.Enqueue(pSerializeBuf);

	if (InterlockedExchange(&AcquireSession.NowPostQueuing, SENDING) == NONSENDING)
	{
		InterlockedIncrement(&AcquireSession.IOCount);
		PostQueuedCompletionStatus(m_hWorkerIOCP, 1, (ULONG_PTR)&AcquireSession, (LPOVERLAPPED)dfSEND_POST_QUEUING_OVERLAPPED);
	}

	//SendPost(&AcquireSession);

	SessionAcquireUnLock(StackIndex);
	return true;
}

bool CNetServer::SendPacketAndDisConnect(UINT64 SessionID, CNetServerSerializationBuf *pSendBuf)
{
	WORD StackIndex = (WORD)(SessionID >> SESSION_INDEX_SHIFT);

	if (pSendBuf == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SERIALIZEBUF_NULL_ERR;
		OnError(&Error);
		return false;
	}

	if (!SessionAcquireLock(SessionID))
	{
		SessionAcquireUnLock(StackIndex);
		return false;
	}

	if (!pSendBuf->m_bIsEncoded)
	{
		pSendBuf->m_iWriteLast = pSendBuf->m_iWrite;
		pSendBuf->m_iWrite = 0;
		pSendBuf->m_iRead = 0;
		pSendBuf->Encode();
	}

	Session &AcquireSession = m_pSessionArray[StackIndex];

	AcquireSession.SendIOData.SendQ.Enqueue(pSendBuf);

	AcquireSession.bSendDisconnect = TRUE;

	if (InterlockedExchange(&AcquireSession.NowPostQueuing, SENDING) == NONSENDING)
	{
		InterlockedIncrement(&AcquireSession.IOCount);
		PostQueuedCompletionStatus(m_hWorkerIOCP, 1, (ULONG_PTR)&AcquireSession, (LPOVERLAPPED)dfSEND_POST_QUEUING_OVERLAPPED);
	}

	//SendPost(&AcquireSession);

	SessionAcquireUnLock(StackIndex);
	return true;
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CNetServer::SendPost(Session *pSession)
{
	Session &SendSession = *pSession;
	while (1)
	{
		if (InterlockedExchange(&SendSession.SendIOData.IOMode, SENDING) != NONSENDING)
		//if (InterlockedCompareExchange(&SendSession.SendIOData.IOMode, SENDING, NONSENDING))
			return POST_RETVAL_COMPLETE;

		int UseSize = SendSession.SendIOData.SendQ.GetRestSize();
		if (UseSize == 0)
		{
			InterlockedExchange(&SendSession.SendIOData.IOMode, NONSENDING);
			if (SendSession.SendIOData.SendQ.GetRestSize() > 0)
				continue;
			return POST_RETVAL_COMPLETE;
		}
		//else if (UseSize < 0)
		//{
		//	if (InterlockedDecrement(&SendSession.IOCount) == 0)
		//	{
		//		ReleaseSession(&SendSession);
		//		return POST_RETVAL_ERR_SESSION_DELETED;
		//	}
		//	InterlockedExchange(&SendSession.SendIOData.IOMode, NONSENDING);
		//	return POST_RETVAL_ERR;
		//}

		WSABUF wsaSendBuf[ONE_SEND_WSABUF_MAX];
		if (ONE_SEND_WSABUF_MAX < UseSize)
			UseSize = ONE_SEND_WSABUF_MAX;
	
		for (int i = 0; i < UseSize; ++i)
		{
			SendSession.SendIOData.SendQ.Dequeue(&SendSession.pSeirializeBufStore[i]);
			wsaSendBuf[i].buf = SendSession.pSeirializeBufStore[i]->GetBufferPtr();
			wsaSendBuf[i].len = SendSession.pSeirializeBufStore[i]->GetAllUseSize();
		}
		SendSession.SendIOData.lBufferCount = UseSize;
	
		InterlockedIncrement(&SendSession.IOCount);
		ZeroMemory(&pSession->SendIOData.Overlapped, sizeof(OVERLAPPED));

		int retval = WSASend(SendSession.sock, wsaSendBuf, UseSize, NULL, 0, &SendSession.SendIOData.Overlapped, 0);
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

				// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
				if (InterlockedDecrement(&SendSession.IOCount) == 0)
				{
					ReleaseSession(&SendSession);
					return POST_RETVAL_ERR_SESSION_DELETED;
				}
				return POST_RETVAL_ERR;
			}
		}
		
	}
	return POST_RETVAL_ERR_SESSION_DELETED;
}

UINT CNetServer::GetNumOfUser()
{
	return m_uiNumOfUser;
}

UINT CNetServer::GetStackRestSize()
{
	return m_pSessionIndexStack->GetRestStackSize();
}

UINT CNetServer::GetAcceptTotal()
{
	return m_uiAcceptTotal;
}

UINT CNetServer::GetAcceptTPSAndReset()
{
	UINT AcceptTPS = m_uiAcceptTPS;
	InterlockedExchange(&m_uiAcceptTPS, 0);

	return AcceptTPS;
}

bool CNetServer::SessionAcquireLock(UINT64 SessionID)
{
	WORD SessionIndex = SessionID >> SESSION_INDEX_SHIFT;
	Session &AcquireSession = m_pSessionArray[SessionIndex];
	InterlockedIncrement(&AcquireSession.IOCount);
	if (AcquireSession.IsUseSession == FALSE)
	{
		return false;
	}
	if (AcquireSession.SessionID != SessionID)
	{
		return false;
	}

	return true;
}

void CNetServer::SessionAcquireUnLock(WORD SessionIndex)
{
	if (InterlockedDecrement(&m_pSessionArray[SessionIndex].IOCount) == 0)
		ReleaseSession(&m_pSessionArray[SessionIndex]);
}
