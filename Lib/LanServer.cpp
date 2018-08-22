#include "stdafx.h"
#pragma comment(lib, "ws2_32")
#include "LanServer.h"
#include <process.h>
#include <WS2tcpip.h>
#include "CrashDump.h"
#include "LanServerSerializeBuf.h"

CCrashDump dump;

CLanServer::CLanServer()
{

}

CLanServer::~CLanServer()
{

}

bool CLanServer::Start(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle, UINT MaxClient)
{
	memcpy(m_IP, IP, sizeof(m_IP));
	int retval;

	WSADATA Wsa;
	if (WSAStartup(MAKEWORD(2, 2), &Wsa))
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanServerErr = LANSERVER_ERR::WSASTARTUP_ERR;
		OnError(&Error);
		return false;
	}

	m_ListenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_ListenSock == INVALID_SOCKET)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanServerErr = LANSERVER_ERR::LISTEN_SOCKET_ERR;
		OnError(&Error);
		return false;
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(PORT);
	retval = bind(m_ListenSock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanServerErr = LANSERVER_ERR::LISTEN_BIND_ERR;
		OnError(&Error);
		return false;
	}

	retval = listen(m_ListenSock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanServerErr = LANSERVER_ERR::LISTEN_LISTEN_ERR;
		OnError(&Error);
		return false;
	}


	m_uiMaxClient = MaxClient;
	m_pSessionArray = new Session[MaxClient];
	//m_pSessionIndexStack = new Stack<WORD>(MaxClient);
	m_pSessionIndexStack = new CLockFreeStack<WORD>();
	for (int i = MaxClient - 1; i >= 0; --i)
		m_pSessionIndexStack->Push(i);
	//InitializeCriticalSection(&m_IndexStackCriticalSection);
	m_uiNumOfUser = 0;

	m_pWorkerThreadHandle = new HANDLE[NumOfWorkerThread];
	// static 함수에서 LanServer 객체를 접근하기 위하여 this 포인터를 인자로 넘김
	for (int i = 0; i < NumOfWorkerThread; i++)
		m_pWorkerThreadHandle[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, NULL);
	m_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, NULL);
	if (m_hAcceptThread == 0 || m_pWorkerThreadHandle == 0)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanServerErr = LANSERVER_ERR::BEGINTHREAD_ERR;
		OnError(&Error);
		return false;
	}
	m_byNumOfWorkerThread = NumOfWorkerThread;


	retval = setsockopt(m_ListenSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&IsNagle, sizeof(int));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanServerErr = LANSERVER_ERR::SETSOCKOPT_ERR;
	}

	m_hWorkerIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, NumOfWorkerThread);
	if (m_hWorkerIOCP == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanServerErr = LANSERVER_ERR::WORKERIOCP_NULL_ERR;
		return false;
	}

	return true;
}

void CLanServer::Stop()
{
	closesocket(m_ListenSock);
	WaitForSingleObject(m_hAcceptThread, INFINITE);
	PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, NULL);
	WaitForMultipleObjects(m_byNumOfWorkerThread, m_pWorkerThreadHandle, TRUE, INFINITE);

	delete[] m_pSessionArray;
	delete m_pSessionIndexStack;
	//DeleteCriticalSection(&m_IndexStackCriticalSection);

	CloseHandle(m_hAcceptThread);
	for (BYTE i = 0; i < m_byNumOfWorkerThread; ++i)
		CloseHandle(m_pWorkerThreadHandle[i]);
	CloseHandle(m_hWorkerIOCP);

	WSACleanup();
}

bool CLanServer::ReleaseSession(Session *pSession)
{
	__int64 pDest[ONE_SEND_WSABUF_MAX] = { 0 };
	int ReadSendRingBufferSize = pSession->SendIOData.RingBuffer.GetUseSize();
	pSession->SendIOData.RingBuffer.Dequeue((char*)&pDest, (ReadSendRingBufferSize));
	for (int i = 0; i < ReadSendRingBufferSize >> 3; ++i)
	{
		CSerializationBuf::Free((CSerializationBuf*)pDest[i]);
	}
	InterlockedAdd(&pSession->Del, ReadSendRingBufferSize >> 3);
	InterlockedAdd(&g_ULLConuntOfDel, ReadSendRingBufferSize >> 3);

	if (pSession->New != pSession->Del)
	{
		dump.Crash();
	}
	InterlockedAdd(&pSession->ReadSendRingbuffer, -ReadSendRingBufferSize);

	closesocket(pSession->sock);
	pSession->IsUseSession = FALSE;

	UINT64 ID = pSession->SessionID;
	WORD StackIndex = (WORD)(ID >> SESSION_INDEX_SHIFT);

	//EnterCriticalSection(&m_IndexStackCriticalSection);
	//if (StackIndex > 199)
	//	printf("A");
	m_pSessionIndexStack->Push(StackIndex);
	//LeaveCriticalSection(&m_IndexStackCriticalSection);

	InterlockedDecrement(&m_uiNumOfUser);
	OnClientLeave(ID);

	return true;
}

bool CLanServer::DisConnect(UINT64 SessionID)
{
	WORD StackIndex = (WORD)(SessionID >> SESSION_INDEX_SHIFT);

	if (m_pSessionArray[StackIndex].IsUseSession == FALSE)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanServerErr = LANSERVER_ERR::SESSION_NULL_ERR;
		OnError(&Error);
		return false;
	}
	shutdown(m_pSessionArray[StackIndex].sock, SD_BOTH);

	return true;
}

bool CLanServer::SendPacket(UINT64 SessionID, CSerializationBuf *pSerializeBuf)
{

	WORD StackIndex = (WORD)(SessionID >> SESSION_INDEX_SHIFT);
	if (pSerializeBuf == NULL)
	{
		ReleaseSession(&m_pSessionArray[StackIndex]);
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanServerErr = LANSERVER_ERR::SERIALIZEBUF_NULL_ERR;
		OnError(&Error);
		return false;
	}
	
	if (m_pSessionArray[StackIndex].IsUseSession == FALSE)
	{
		CSerializationBuf::Free(pSerializeBuf);
		InterlockedIncrement(&g_ULLConuntOfDel);
		//delete pSerializeBuf;
		InterlockedIncrement(&m_pSessionArray[StackIndex].Del);

		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanServerErr = LANSERVER_ERR::SESSION_NULL_ERR;
		OnError(&Error);
		return false;
	}

	InterlockedIncrement(&m_pSessionArray[StackIndex].New);
	WORD HeaderSize = 8;
	pSerializeBuf->WritePtrSetHeader();
	*pSerializeBuf << HeaderSize;

	m_pSessionArray[StackIndex].SendIOData.RingBuffer.Enqueue/*.LockEnqueue*/((char*)&pSerializeBuf, 8);
	SendPost(&m_pSessionArray[StackIndex]);
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
			if (Err == WSAEWOULDBLOCK)
			{
				continue;
			}
			else if (Err == WSAEINTR)
			{
				break;
			}
			else
			{
				st_Error Error;
				Error.GetLastErr = Err;
				Error.LanServerErr = LANSERVER_ERR::ACCEPT_ERR;
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
		//EnterCriticalSection(&m_IndexStackCriticalSection);
		UINT64 SessionIdx = 0;
		WORD Index;
		m_pSessionIndexStack->Pop(&Index);
		SessionIdx = Index;
		//LeaveCriticalSection(&m_IndexStackCriticalSection);
		SessionID += (SessionIdx << SESSION_INDEX_SHIFT);

		m_pSessionArray[SessionIdx].IOCount = 1;
		m_pSessionArray[SessionIdx].IsUseSession = TRUE;
		m_pSessionArray[SessionIdx].sock = clientsock;
		m_pSessionArray[SessionIdx].SessionID = SessionID;
		m_pSessionArray[SessionIdx].ReadSendRingbuffer = 0;

		m_pSessionArray[SessionIdx].RecvIOData.wBufferCount = 0;
		m_pSessionArray[SessionIdx].SendIOData.wBufferCount = 0;
		m_pSessionArray[SessionIdx].RecvIOData.IOMode = 0;
		m_pSessionArray[SessionIdx].SendIOData.IOMode = 0;
		ZeroMemory(&m_pSessionArray[SessionIdx].RecvIOData.Overlapped, sizeof(OVERLAPPED));
		ZeroMemory(&m_pSessionArray[SessionIdx].SendIOData.Overlapped, sizeof(OVERLAPPED));
		m_pSessionArray[SessionIdx].RecvIOData.RingBuffer.InitPointer();
		m_pSessionArray[SessionIdx].SendIOData.RingBuffer.InitPointer();

		/////////////////////////////////////////////////////////////////////////
		m_pSessionArray[SessionIdx].New = 0;
		m_pSessionArray[SessionIdx].Del = 0;
		/////////////////////////////////////////////////////////////////////////

		int SendBufSize = 0;
		retval = setsockopt(clientsock, SOL_SOCKET, SO_SNDBUF, (char*)&SendBufSize, sizeof(SendBufSize));
		if (retval == SOCKET_ERROR)
		{
			st_Error Error;
			Error.GetLastErr = WSAGetLastError();
			Error.LanServerErr = LANSERVER_ERR::SETSOCKOPT_ERR;
			OnError(&Error);
			ReleaseSession(&m_pSessionArray[SessionIdx]);
			continue;
		}

		DWORD flag = 0;
		CreateIoCompletionPort((HANDLE)clientsock, m_hWorkerIOCP, (ULONG_PTR)&m_pSessionArray[SessionIdx], 0);
		InterlockedIncrement(&m_uiNumOfUser);

		OnClientJoin(m_pSessionArray[SessionIdx].SessionID);
		if (m_pSessionArray[SessionIdx].IsUseSession == FALSE)
			continue;
		RecvPost(&m_pSessionArray[SessionIdx]);
		UINT IOCnt = InterlockedDecrement(&m_pSessionArray[SessionIdx].IOCount);
		if (IOCnt == 0)
			ReleaseSession(&m_pSessionArray[SessionIdx]);
	}

	return 0;
}

UINT CLanServer::Worker()
{
	char cPostRetval;
	int retval;
	DWORD Transferred;
	Session *pSession;
	LPOVERLAPPED lpOverlapped;
	ULONGLONG IOCount;

	while (1)
	{
		cPostRetval = -1;
		Transferred = 0;
		pSession = NULL;
		lpOverlapped = NULL;

		GetQueuedCompletionStatus(m_hWorkerIOCP, &Transferred, (PULONG_PTR)&pSession, &lpOverlapped, INFINITE);
		OnWorkerThreadBegin();
		// 외부 종료코드에 의한 종료
		if (pSession == NULL)
		{
			PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, NULL);
			break;
		}
		else if (lpOverlapped == NULL)
		{
			st_Error Error;
			Error.GetLastErr = WSAGetLastError();
			Error.LanServerErr = LANSERVER_ERR::OVERLAPPED_NULL_ERR;
			OnError(&Error);
			continue;
		}
		// 클라이언트가 종료함
		else if (Transferred == 0)
		{
			// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
			IOCount = InterlockedDecrement(&pSession->IOCount);
			if (IOCount == 0)
				ReleaseSession(pSession);

			continue;
		}
		else if (lpOverlapped == &pSession->RecvIOData.Overlapped)
		{
			// recv 파트
			pSession->RecvIOData.RingBuffer.MoveWritePos/*LockMoveWritePos*/(Transferred);
			int RingBufferRestSize = Transferred;

			while (RingBufferRestSize != 0)
			{
				WORD Header;
				pSession->RecvIOData.RingBuffer.Peek((char*)&Header, sizeof(WORD));
				if (Header != 8)
				{
					IOCount = InterlockedDecrement(&pSession->IOCount);
					break;
				}
				else if (pSession->RecvIOData.RingBuffer.GetUseSize() < Header + sizeof(WORD))
					break;
				pSession->RecvIOData.RingBuffer.RemoveData(sizeof(WORD));

				CSerializationBuf RecvSerializeBuf;
				RecvSerializeBuf.SetWriteZero();
				retval = pSession->RecvIOData.RingBuffer.Dequeue(RecvSerializeBuf.GetWriteBufferPtr(), Header);
				RecvSerializeBuf.MoveWritePos(retval);
				RingBufferRestSize -= (retval + sizeof(WORD));
				OnRecv(pSession->SessionID, &RecvSerializeBuf);
			}

			cPostRetval = RecvPost(pSession);
		}
		else if (lpOverlapped == &pSession->SendIOData.Overlapped)
		{
			// send 파트
			__int64 pDest[ONE_SEND_WSABUF_MAX] = { 0 };
			int ReadSendRingBufferSize = pSession->ReadSendRingbuffer;
			pSession->SendIOData.RingBuffer.Dequeue((char*)&pDest, (ReadSendRingBufferSize << 3));
			for (int i = 0; i < ReadSendRingBufferSize; ++i)
			{
				CSerializationBuf::Free((CSerializationBuf*)pDest[i]);
				InterlockedIncrement(&g_ULLConuntOfDel);
				//delete (CSerializationBuf*)pDest[i];
			}
			InterlockedAdd(&pSession->Del, ReadSendRingBufferSize);
			InterlockedAdd(&pSession->ReadSendRingbuffer, -ReadSendRingBufferSize);

			OnSend();
			InterlockedExchange(&pSession->SendIOData.IOMode, NONSENDING);
			cPostRetval = SendPost(pSession);
		}

		OnWorkerThreadEnd();

		if (cPostRetval == POST_RETVAL_ERR_SESSION_DELETED)
			continue;
		IOCount = InterlockedDecrement(&pSession->IOCount);
		if (IOCount == 0)
			ReleaseSession(pSession);
	}

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
	ULONGLONG IOCount;
	if (pSession->RecvIOData.RingBuffer.IsFull())
	{
		// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
		IOCount = InterlockedDecrement(&pSession->IOCount);
		if (IOCount == 0)
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
			IOCount = InterlockedDecrement(&pSession->IOCount);
			if (IOCount == 0)
			{
				ReleaseSession(pSession);
				return POST_RETVAL_ERR_SESSION_DELETED;
			}
			st_Error Error;
			Error.GetLastErr = GetLastErr;
			Error.LanServerErr = LANSERVER_ERR::WSARECV_ERR;
			OnError(&Error);
			return POST_RETVAL_ERR;
		}
	}
	return POST_RETVAL_COMPLETE;
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CLanServer::SendPost(Session *pSession)
{
	while (1)
	{
		if (InterlockedCompareExchange(&pSession->SendIOData.IOMode, SENDING, NONSENDING))
			return true;

		ULONGLONG IOCount;
		int UseSize = pSession->SendIOData.RingBuffer.GetUseSize();
		if (UseSize == 0)
		{
			InterlockedExchange(&pSession->SendIOData.IOMode, NONSENDING);
			if (pSession->SendIOData.RingBuffer.GetUseSize() > 0)
				continue;
			return POST_RETVAL_COMPLETE;
		}
		else if (UseSize < 0)
		{
			IOCount = InterlockedDecrement(&pSession->IOCount);
			if (IOCount == 0)
			{
				ReleaseSession(pSession);
				return POST_RETVAL_ERR_SESSION_DELETED;
			}
			InterlockedExchange(&pSession->SendIOData.IOMode, NONSENDING);
			return POST_RETVAL_ERR;
		}

		int BrokenSize = pSession->SendIOData.RingBuffer.GetNotBrokenGetSize();
		int RestSize = UseSize - BrokenSize;

		BYTE wsaBufCount = 0;
		WSABUF wsaSendBuf[ONE_SEND_WSABUF_MAX];
		CSerializationBuf *pRingBufferDest[ONE_SEND_WSABUF_MAX];
		int UseRingBufferPtrSize = pSession->SendIOData.RingBuffer.GetUseSize() >> 3;
		pSession->SendIOData.RingBuffer.Peek((char*)pRingBufferDest, UseRingBufferPtrSize << 3);

		for (int i = 0; i < UseRingBufferPtrSize; ++i)
		{
			wsaSendBuf[i].buf = pRingBufferDest[i]->GetBufferPtr();
			wsaSendBuf[i].len = pRingBufferDest[i]->GetAllUseSize();
		}
		InterlockedAdd(&pSession->ReadSendRingbuffer, UseRingBufferPtrSize);
		int BufCount = UseRingBufferPtrSize;
		
		//while (pSession->SendIOData.RingBuffer.GetUseSize() == 0)
		//{
		//	wsaSendBuf->buf = ;
		//	wsaSendBuf->len = ;
		//	pSession->SendIOData.RingBuffer.Peek((char*)&wsaSendBuf[wsaBufCount], 8);
		//	++wsaBufCount;
		//}
		//pSession->SendIOData.RingBuffer.Peek(, UseSize);
		/////////////////////////////////////////////////////////////////////////////////////////////
		//WSABUF wsaBuf[2];
		//wsaBuf[0].buf = pSession->SendIOData.RingBuffer.GetReadBufferPtr();
		//wsaBuf[0].len = BrokenSize;
		//if (RestSize > 0)
		//{
		//	wsaBuf[1].buf = pSession->SendIOData.RingBuffer.GetBufferPtr();
		//	wsaBuf[1].len = RestSize;
		//	BufCount++;
		//}

		InterlockedIncrement(&pSession->IOCount);
		int retval = WSASend(pSession->sock, wsaSendBuf, BufCount, NULL, 0, &pSession->SendIOData.Overlapped, 0);
		if (retval == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			if (err != ERROR_IO_PENDING)
			{
				if (err == WSAENOBUFS)
					dump.Crash();
				st_Error Error;
				Error.GetLastErr = err;
				Error.LanServerErr = LANSERVER_ERR::WSASEND_ERR;
				OnError(&Error);

				// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
				IOCount = InterlockedDecrement(&pSession->IOCount);
				if (IOCount == 0)
				{
					ReleaseSession(pSession);
					return POST_RETVAL_ERR_SESSION_DELETED;
				}
				InterlockedExchange(&pSession->SendIOData.IOMode, NONSENDING);
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
