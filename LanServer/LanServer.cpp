#include "PreCompile.h"
#pragma comment(lib, "ws2_32")
#include "LanServer.h"
#include <process.h>
#include <WS2tcpip.h>
#include "CrashDump.h"
#include "LanServerSerializeBuf.h"
#include "Profiler.h"
#include "Log.h"

CLanServer::CLanServer()
{
	
}

CLanServer::~CLanServer()
{

}

bool CLanServer::Start(const WCHAR *szOptionFileName)
{
	if (!LanServerOptionParsing(szOptionFileName))
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

	retval = setsockopt(m_ListenSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&m_bIsNagleOn, sizeof(int));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SETSOCKOPT_ERR;
	}

	m_hWorkerIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, m_byNumOfUsingWorkerThread);
	if (m_hWorkerIOCP == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::WORKERIOCP_NULL_ERR;
		return false;
	}	

	m_pWorkerThreadHandle = new HANDLE[m_byNumOfWorkerThread];
	// static 함수에서 LanServer 객체를 접근하기 위하여 this 포인터를 인자로 넘김
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

	if (WSAGetLastError() != 0)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SETSOCKOPT_ERR;
		OnError(&Error);
	}

	return true;
}

void CLanServer::Stop()
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

bool CLanServer::LanServerOptionParsing(const WCHAR *szOptionFileName)
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

	int DebugLevel;

	if (!parser.GetValue_String(pBuff, L"SERVER", L"BIND_IP", m_IP))
		return false;
	if (!parser.GetValue_Short(pBuff, L"SERVER", L"BIND_PORT", (short*)&m_wPort))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"SERVER", L"WORKER_THREAD", &m_byNumOfWorkerThread))
		return false;
	if (!parser.GetValue_Int(pBuff, L"SERVER", L"CLIENT_MAX", (int*)&m_uiMaxClient))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"SERVER", L"USE_IOCPWORKER", &m_byNumOfUsingWorkerThread))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"SERVER", L"NAGLE_ON", (BYTE*)&m_bIsNagleOn))
		return false;

	if (!parser.GetValue_Int(pBuff, L"SERVER", L"LOG_LEVEL", &DebugLevel))
		return false;

	SetLogLevel(DebugLevel);

	return true;
}

bool CLanServer::ReleaseSession(Session *pSession)
{
	if (InterlockedCompareExchange64((LONG64*)&pSession->IOCount, 0, df_RELEASE_VALUE) != df_RELEASE_VALUE)
		return false;

	int SendBufferRestSize = pSession->SendIOData.lBufferCount;
	int Rest = pSession->SendIOData.SendQ.GetRestSize();
	// SendPost 에서 옮겨졌으나 보내지 못한 직렬화 버퍼들을 반환함
	for (int i = 0; i < SendBufferRestSize; ++i)
	{
		CSerializationBuf::Free(pSession->pSeirializeBufStore[i]);
	}

	// 큐에서 아직 꺼내오지 못한 직렬화 버퍼가 있다면 해당 직렬화 버퍼들을 반환함
	if (Rest > 0)
	{
		CSerializationBuf *DeleteBuf;
		for (int i = 0; i < Rest; ++i)
		{
			pSession->SendIOData.SendQ.Dequeue(&DeleteBuf);
			CSerializationBuf::Free(DeleteBuf);
		}
	}

	closesocket(pSession->sock);
	pSession->IsUseSession = FALSE;

	UINT64 ID = pSession->SessionID;
	WORD StackIndex = (WORD)(ID >> SESSION_INDEX_SHIFT);

	InterlockedDecrement(&m_uiNumOfUser);
	m_pSessionIndexStack->Push(StackIndex);

	OnClientLeave(ID);

	return true;
}

bool CLanServer::DisConnect(UINT64 SessionID)
{
	WORD StackIndex = (WORD)(SessionID >> SESSION_INDEX_SHIFT);

	if (InterlockedIncrement(&m_pSessionArray[StackIndex].IOCount) == 1)
	{
		InterlockedDecrement(&m_pSessionArray[StackIndex].IOCount);
		if(m_pSessionArray[StackIndex].IsUseSession == FALSE)
			ReleaseSession(&m_pSessionArray[StackIndex]);
		return false;
	}

	if (m_pSessionArray[StackIndex].IsUseSession == FALSE)
	{
		// 이후 컨텐츠가 들어간다면 발생할 가능성이 존재함
		//////////////////////////////////////////////////////
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SESSION_NULL_ERR;
		OnError(&Error);
		return false;
		//////////////////////////////////////////////////////
	}
	
	shutdown(m_pSessionArray[StackIndex].sock, SD_BOTH);
	
	if (InterlockedDecrement(&m_pSessionArray[StackIndex].IOCount) == 0)
	{
		ReleaseSession(&m_pSessionArray[StackIndex]);
		return false;
	}
	return true;
}

UINT CLanServer::Accepter()
{
	SOCKET clientsock;
	SOCKADDR_IN clientaddr;
	int retval;
	WCHAR IP[16];

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

		//inet_ntop(AF_INET, (const void*)&clientaddr, (PSTR)IP, sizeof(IP));
		if (!OnConnectionRequest())
		{
			closesocket(clientsock);
			continue;
		}

		UINT64 SessionID = InterlockedIncrement(&m_iIDCount);
		// 가장 상위 2 비트를 Session Stack 의 Index 로 사용함
		UINT64 SessionIdx = 0;
		WORD Index;
		m_pSessionIndexStack->Pop(&Index);
		SessionIdx = Index;
		SessionID += (SessionIdx << SESSION_INDEX_SHIFT);

		InterlockedIncrement(&m_pSessionArray[SessionIdx].IOCount);

		m_pSessionArray[SessionIdx].sock = clientsock;
		m_pSessionArray[SessionIdx].SessionID = SessionID;

		m_pSessionArray[SessionIdx].RecvIOData.wBufferCount = 0;
		m_pSessionArray[SessionIdx].SendIOData.lBufferCount = 0;
		m_pSessionArray[SessionIdx].RecvIOData.IOMode = 0;
		m_pSessionArray[SessionIdx].SendIOData.IOMode = 0;

		ZeroMemory(&m_pSessionArray[SessionIdx].RecvIOData.Overlapped, sizeof(OVERLAPPED));
		ZeroMemory(&m_pSessionArray[SessionIdx].SendIOData.Overlapped, sizeof(OVERLAPPED));

		m_pSessionArray[SessionIdx].RecvIOData.RingBuffer.InitPointer();
		m_pSessionArray[SessionIdx].SendIOData.SendQ.InitQueue();
		
		m_pSessionArray[SessionIdx].IsUseSession = TRUE;

		int SendBufSize = 0;
		retval = setsockopt(clientsock, SOL_SOCKET, SO_SNDBUF, (char*)&SendBufSize, sizeof(SendBufSize));
		if (retval == SOCKET_ERROR)
		{
			st_Error Error;
			Error.GetLastErr = WSAGetLastError();
			Error.ServerErr = SERVER_ERR::SETSOCKOPT_ERR;
			OnError(&Error);
			ReleaseSession(&m_pSessionArray[SessionIdx]);
			continue;
		}

		DWORD flag = 0;
		CreateIoCompletionPort((HANDLE)clientsock, m_hWorkerIOCP, (ULONG_PTR)&m_pSessionArray[SessionIdx], 0);
		InterlockedIncrement(&m_uiNumOfUser);

		OnClientJoin(m_pSessionArray[SessionIdx].SessionID);
		//if (m_pSessionArray[SessionIdx].IsUseSession == FALSE)
		//	continue;
		
		RecvPost(&m_pSessionArray[SessionIdx]);

		if (InterlockedDecrement(&m_pSessionArray[SessionIdx].IOCount) == 0)
			ReleaseSession(&m_pSessionArray[SessionIdx]);
	}

	CSerializationBuf::ChunkFreeForcibly();
	return 0;
}

UINT CLanServer::Worker()
{
	char cPostRetval;
	bool GQCSClear;
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

		GQCSClear = GetQueuedCompletionStatus(m_hWorkerIOCP, &Transferred, (PULONG_PTR)&pSession, &lpOverlapped, INFINITE);
		OnWorkerThreadBegin();
		if (lpOverlapped != NULL)
		{
			// 외부 종료코드에 의한 종료
			if (pSession == NULL)
			{
				PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, NULL);
				break;
			}
			// 클라이언트가 종료함
			else if (Transferred == 0)
			{
				// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
				if (InterlockedDecrement(&pSession->IOCount) == 0)
					ReleaseSession(pSession);

				continue;
			}
			// recv 파트
			if (lpOverlapped == &pSession->RecvIOData.Overlapped)
			{
				pSession->RecvIOData.RingBuffer.MoveWritePos(Transferred);
				int RingBufferRestSize = Transferred;

				while (RingBufferRestSize != 0)
				{
					WORD Header;
					retval = pSession->RecvIOData.RingBuffer.Peek((char*)&Header, sizeof(WORD));
					if (retval < df_LANSERVER_HEADER_SIZE)
						break;
					else if (pSession->RecvIOData.RingBuffer.GetUseSize() < Header + sizeof(WORD))
						break;
					pSession->RecvIOData.RingBuffer.RemoveData(sizeof(WORD));

					CSerializationBuf RecvSerializeBuf;
					//RecvSerializeBuf.SetWriteZero();
					//retval = pSession->RecvIOData.RingBuffer.Dequeue(RecvSerializeBuf.GetWriteBufferPtr(), Header);
					retval = pSession->RecvIOData.RingBuffer.Dequeue(&RecvSerializeBuf.m_pSerializeBuffer[RecvSerializeBuf.m_iWrite], Header);
					RecvSerializeBuf.m_iWrite += retval;
					RingBufferRestSize -= (retval + sizeof(WORD));
					OnRecv(pSession->SessionID, &RecvSerializeBuf);
				}

				cPostRetval = RecvPost(pSession);
			}
			// send 파트
			else if (lpOverlapped == &pSession->SendIOData.Overlapped)
			{
				int BufferCount = pSession->SendIOData.lBufferCount;
				for (int i = 0; i < BufferCount; ++i)
				{
					CSerializationBuf::Free(pSession->pSeirializeBufStore[i]);
				}

				pSession->SendIOData.lBufferCount -= BufferCount;
				//InterlockedAdd(&g_ULLConuntOfDel, BufferCount);
				//InterlockedAdd(&pSession->Del, BufferCount); // 여기 다시 생각해 볼 것
				//pSession->Del += BufferCount;
				//InterlockedAdd(&pSession->SendIOData.lBufferCount, -BufferCount); // 여기 다시 생각해 볼 것

				OnSend(pSession->SessionID, BufferCount);
				InterlockedExchange(&pSession->SendIOData.IOMode, NONSENDING); // 여기 다시 생각해 볼 것

				cPostRetval = SendPost(pSession);
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

	CSerializationBuf::ChunkFreeForcibly();
	return 0;
}

UINT __stdcall CLanServer::AcceptThread(LPVOID pLanServer)
{
	return ((CLanServer*)pLanServer)->Accepter();
}

UINT __stdcall CLanServer::WorkerThread(LPVOID pLanServer)
{
	return ((CLanServer*)pLanServer)->Worker();
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CLanServer::RecvPost(Session *pSession)
{
	if (pSession->RecvIOData.RingBuffer.IsFull())
	{
		// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
		if (InterlockedDecrement(&pSession->IOCount) == 0)
		{
			ReleaseSession(pSession);
			return POST_RETVAL_ERR_SESSION_DELETED;
		}
		return POST_RETVAL_ERR;
	}

	int BrokenSize = pSession->RecvIOData.RingBuffer.GetNotBrokenPutSize();
	int RestSize = pSession->RecvIOData.RingBuffer.GetFreeSize() - BrokenSize;
	int BufCount = 1;
	DWORD Flag = 0;

	WSABUF wsaBuf[2];
	wsaBuf[0].buf = pSession->RecvIOData.RingBuffer.GetWriteBufferPtr();
	wsaBuf[0].len = BrokenSize;
	if (RestSize > 0)
	{
		wsaBuf[1].buf = pSession->RecvIOData.RingBuffer.GetBufferPtr();
		wsaBuf[1].len = RestSize;
		BufCount++;
	}
	InterlockedIncrement(&pSession->IOCount);
	int retval = WSARecv(pSession->sock, wsaBuf, BufCount, NULL, &Flag, &pSession->RecvIOData.Overlapped, 0);
	if (retval == SOCKET_ERROR)
	{
		int GetLastErr = WSAGetLastError();
		if (GetLastErr != ERROR_IO_PENDING)
		{
			// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
			if (InterlockedDecrement(&pSession->IOCount) == 0)
			{
				ReleaseSession(pSession);
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

bool CLanServer::SendPacket(UINT64 SessionID, CSerializationBuf *pSerializeBuf)
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
	
	if (m_pSessionArray[StackIndex].SessionID != SessionID)
	{
		Begin("Free");
		CSerializationBuf::Free(pSerializeBuf);
		End("Free");

		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::INCORRECT_SESSION;
		OnError(&Error);
		return false;
	}

	if (m_pSessionArray[StackIndex].IsUseSession == FALSE)
	{
		Begin("Free");
		CSerializationBuf::Free(pSerializeBuf);
		End("Free");

		// 이후 컨텐츠가 들어간다면 발생할 가능성이 존재함
		//////////////////////////////////////////////////////
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SESSION_NULL_ERR;
		OnError(&Error);
		//////////////////////////////////////////////////////
		return false;
	}

	if (!pSerializeBuf->m_bHeaderInputted)
	{
		pSerializeBuf->m_bHeaderInputted = TRUE;
		WORD HeaderSize = pSerializeBuf->GetUseSize();
		pSerializeBuf->m_iWriteLast = pSerializeBuf->m_iWrite;
		pSerializeBuf->m_iWrite = 0;
		pSerializeBuf->m_iRead = 0;
		*pSerializeBuf << HeaderSize;
	}

	m_pSessionArray[StackIndex].SendIOData.SendQ.Enqueue(pSerializeBuf);

	SendPost(&m_pSessionArray[StackIndex]);

	return true;
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CLanServer::SendPost(Session *pSession)
{
	while (1)
	{
		if (InterlockedCompareExchange(&pSession->SendIOData.IOMode, SENDING, NONSENDING))
			return true;

		int UseSize = pSession->SendIOData.SendQ.GetRestSize();
		if (UseSize == 0)
		{
			InterlockedExchange(&pSession->SendIOData.IOMode, NONSENDING);
			if (pSession->SendIOData.SendQ.GetRestSize() > 0)
				continue;
			return POST_RETVAL_COMPLETE;
		}
		else if (UseSize < 0)
		{
			if (InterlockedDecrement(&pSession->IOCount) == 0)
			{
				ReleaseSession(pSession);
				return POST_RETVAL_ERR_SESSION_DELETED;
			}
			InterlockedExchange(&pSession->SendIOData.IOMode, NONSENDING);
			return POST_RETVAL_ERR;
		}

		WSABUF wsaSendBuf[ONE_SEND_WSABUF_MAX];
		if (ONE_SEND_WSABUF_MAX < UseSize)
			UseSize = ONE_SEND_WSABUF_MAX;

		for (int i = 0; i < UseSize; ++i)
		{
			pSession->SendIOData.SendQ.Dequeue(&pSession->pSeirializeBufStore[i]);
			wsaSendBuf[i].buf = pSession->pSeirializeBufStore[i]->GetBufferPtr();
			wsaSendBuf[i].len = pSession->pSeirializeBufStore[i]->GetAllUseSize();
		}
		pSession->SendIOData.lBufferCount += UseSize;

		InterlockedIncrement(&pSession->IOCount);
		int retval = WSASend(pSession->sock, wsaSendBuf, UseSize, NULL, 0, &pSession->SendIOData.Overlapped, 0);
		if (retval == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			if (err != ERROR_IO_PENDING)
			{
				st_Error Error;
				Error.GetLastErr = err;
				Error.ServerErr = SERVER_ERR::WSASEND_ERR;
				OnError(&Error);

				// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
				if (InterlockedDecrement(&pSession->IOCount) == 0)
				{
					ReleaseSession(pSession);
					return POST_RETVAL_ERR_SESSION_DELETED;
				}

				return POST_RETVAL_ERR;
			}
		}
	}

	return POST_RETVAL_ERR_SESSION_DELETED;
}

UINT CLanServer::GetNumOfUser()
{
	return m_uiNumOfUser;
}

UINT CLanServer::GetStackRestSize()
{
	return m_pSessionIndexStack->GetRestStackSize();
}
