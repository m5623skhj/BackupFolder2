#include "PreCompile.h"
#pragma comment(lib, "ws2_32")
#include "LanClient.h"
#include "CrashDump.h"
#include "LanServerSerializeBuf.h"
#include "Profiler.h"
#include "Log.h"

CLanClient::CLanClient()
{

}

CLanClient::~CLanClient()
{

}

bool CLanClient::Start(const WCHAR *szOptionFileName)
{
	if (!LanClientOptionParsing(szOptionFileName))
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

	m_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_sock == INVALID_SOCKET)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::LISTEN_SOCKET_ERR;
		OnError(&Error);
		return false;
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	InetPton(AF_INET, m_IP, &serveraddr.sin_addr);
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(m_wPort);

	if (connect(m_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::LISTEN_BIND_ERR;
		OnError(&Error);
		return false;
	}

	retval = setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&m_bIsNagleOn, sizeof(int));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SETSOCKOPT_ERR;
	}

	ZeroMemory(&m_RecvIOData.Overlapped, sizeof(OVERLAPPED));
	ZeroMemory(&m_SendIOData.Overlapped, sizeof(OVERLAPPED));

	m_hWorkerIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (m_hWorkerIOCP == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::WORKERIOCP_NULL_ERR;
		return false;
	}
	CreateIoCompletionPort((HANDLE)m_sock, m_hWorkerIOCP, m_sock, 0);

	m_pWorkerThreadHandle = new HANDLE[m_byNumOfWorkerThread];
	// static 함수에서 LanClient 객체를 접근하기 위하여 this 포인터를 인자로 넘김
	for (int i = 0; i < m_byNumOfWorkerThread; i++)
		m_pWorkerThreadHandle[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, NULL);
	if (m_pWorkerThreadHandle == 0)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::BEGINTHREAD_ERR;
		OnError(&Error);
		return false;
	}
	m_byNumOfWorkerThread = m_byNumOfWorkerThread;

	//m_IOCount = 1;
	m_wNumOfReconnect = 0;
	m_SendIOData.IOMode = NONSENDING;
	RecvPost();
	OnConnectionComplete();

	return true;
}

void CLanClient::Stop()
{
	shutdown(m_sock, SD_BOTH);

	PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, NULL);
	WaitForMultipleObjects(m_byNumOfWorkerThread, m_pWorkerThreadHandle, TRUE, INFINITE);

	for (BYTE i = 0; i < m_byNumOfWorkerThread; ++i)
		CloseHandle(m_pWorkerThreadHandle[i]);
	CloseHandle(m_hWorkerIOCP);

	WSACleanup();
}

bool CLanClient::LanClientOptionParsing(const WCHAR *szOptionFileName)
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

	if (!parser.GetValue_String(pBuff, L"LANCLIENT", L"SERVER_IP", m_IP))
		return false;
	if (!parser.GetValue_Short(pBuff, L"LANCLIENT", L"SERVER_PORT", (short*)&m_wPort))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"LANCLIENT", L"WORKER_THREAD", &m_byNumOfWorkerThread))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"LANCLIENT", L"USE_IOCPWORKER", &m_byNumOfUsingWorkerThread))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"LANCLIENT", L"NAGLE_ON", (BYTE*)&m_bIsNagleOn))
		return false;

	return true;
}

bool CLanClient::ReleaseSession()
{
	//////////////////////////
	g_Dump.Crash();
	//////////////////////////

	if (InterlockedCompareExchange64((LONG64*)&m_IOCount, 0, df_RELEASE_VALUE) != df_RELEASE_VALUE)
		return false;

	int SendBufferRestSize = m_SendIOData.lBufferCount;
	int Rest = m_SendIOData.SendQ.GetRestSize();
	// SendPost 에서 옮겨졌으나 보내지 못한 직렬화 버퍼들을 반환함
	for (int i = 0; i < SendBufferRestSize; ++i)
		CSerializationBuf::Free(pSeirializeBufStore[i]);

	// 큐에서 아직 꺼내오지 못한 직렬화 버퍼가 있다면 해당 직렬화 버퍼들을 반환함
	if (Rest > 0)
	{
		CSerializationBuf *DeleteBuf;
		for (int i = 0; i < Rest; ++i)
		{
			m_SendIOData.SendQ.Dequeue(&DeleteBuf);
			CSerializationBuf::Free(DeleteBuf);
		}
	}

	closesocket(m_sock);
	m_sock = INVALID_SOCKET;

	m_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_sock == INVALID_SOCKET)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::LISTEN_SOCKET_ERR;
		OnError(&Error);
		return false;
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	InetPton(AF_INET, m_IP, &serveraddr.sin_addr);
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(m_wPort);

	_LOG(LOG_LEVEL::LOG_WARNING, L"LanClient", L"ReConnection %d / ThreadID : %d", m_wNumOfReconnect, GetCurrentThreadId());

	while (1)
	{
		if (connect(m_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr)) != SOCKET_ERROR)
			break;
		Sleep(1000);
	}

	int retval = setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&m_bIsNagleOn, sizeof(int));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SETSOCKOPT_ERR;
		return false;
	}

	int SendBufSize = 0;
	retval = setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, (char*)&SendBufSize, sizeof(SendBufSize));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SETSOCKOPT_ERR;
		OnError(&Error);
		return false;
	}


	CreateIoCompletionPort((HANDLE)m_sock, m_hWorkerIOCP, (ULONG_PTR)&m_sock, 0);

	RecvPost();

	++m_wNumOfReconnect;
	//++m_IOCount;

	//if (InterlockedDecrement(&m_IOCount) == 0)
	//	ReleaseSession();
	OnConnectionComplete();

	return true;
}

UINT CLanClient::Worker()
{
	char cPostRetval;
	int retval;
	DWORD Transferred;
	SOCKET *pSession;
	LPOVERLAPPED lpOverlapped;
	OVERLAPPEDIODATA &RecvIOData = m_RecvIOData;
	OVERLAPPED_SEND_IO_DATA	&SendIOData = m_SendIOData;
	srand((UINT)&retval);

	while (1)
	{
		cPostRetval = -1;
		Transferred = 0;
		pSession = NULL;
		lpOverlapped = NULL;

		GetQueuedCompletionStatus(m_hWorkerIOCP, &Transferred, (PULONG_PTR)&pSession, &lpOverlapped, INFINITE);
		OnWorkerThreadBegin();
		// GQCS 에 완료 통지가 왔을 경우
		if (lpOverlapped != NULL)
		{
			// 외부 종료코드에 의한 종료
			if (pSession == NULL)
			{
				PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, NULL);
				break;
			}
			// recv 파트
			if (lpOverlapped == &RecvIOData.Overlapped)
			{
				// 서버가 종료함
				// 재접속 필요
				if (Transferred == 0)
				{
					// 현재 m_IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
					if (InterlockedDecrement(&m_IOCount) == 0)
						ReleaseSession();

					continue;
				}

				RecvIOData.RingBuffer.MoveWritePos(Transferred);
				int RingBufferRestSize = RecvIOData.RingBuffer.GetUseSize();

				while (RingBufferRestSize > HEADER_SIZE)
				{
					CSerializationBuf &RecvSerializeBuf = *CSerializationBuf::Alloc();
					RecvSerializeBuf.m_iRead = 0;
					retval = RecvIOData.RingBuffer.Peek((char*)RecvSerializeBuf.m_pSerializeBuffer, HEADER_SIZE);
					if (retval < HEADER_SIZE)
					{
						CSerializationBuf::Free(&RecvSerializeBuf);
						break;
					}
					// PayloadLength 2
					WORD PayloadLength;
					RecvSerializeBuf >> PayloadLength;

					if (RecvIOData.RingBuffer.GetUseSize() < PayloadLength + HEADER_SIZE)
					{
						CSerializationBuf::Free(&RecvSerializeBuf);
						break;
					}
					RecvIOData.RingBuffer.RemoveData(retval);

					retval = RecvIOData.RingBuffer.Dequeue(&RecvSerializeBuf.m_pSerializeBuffer[RecvSerializeBuf.m_iWrite], PayloadLength);
					RecvSerializeBuf.m_iWrite += retval;

					RingBufferRestSize -= (retval + sizeof(WORD));
					OnRecv(&RecvSerializeBuf);
					CSerializationBuf::Free(&RecvSerializeBuf);
				}

				cPostRetval = RecvPost();
			}
			// send 파트
			else if (lpOverlapped == &SendIOData.Overlapped)
			{
				//int BufferCount = 0;
				//while (pSession->pSeirializeBufStore[BufferCount] != NULL)
				//{
				//		CSerializationBuf::Free(pSession->pSeirializeBufStore[BufferCount]);
				//		pSession->pSeirializeBufStore[BufferCount] = NULL;
				//		++BufferCount;
				//}
				int BufferCount = SendIOData.lBufferCount;
				for (int i = 0; i < BufferCount; ++i)
				{
					Begin("Free");
					CSerializationBuf::Free(pSeirializeBufStore[i]);
					End("Free");

					////////////////////////////////////////
					//pSession->pSeirializeBufStore[i] = NULL;
					////////////////////////////////////////
				}

				SendIOData.lBufferCount -= BufferCount;
				//InterlockedAdd(&g_ULLuntOfDel, BufferCount);
				//InterlockedAdd(&pSession->Del, BufferCount); // 여기 다시 생각해 볼 것
				//pSession->Del += BufferCount;
				//InterlockedAdd(&pSession->SendIOData.lBufferCount, -BufferCount); // 여기 다시 생각해 볼 것

				OnSend();
				InterlockedExchange(&SendIOData.IOMode, NONSENDING); // 여기 다시 생각해 볼 것
				cPostRetval = SendPost();
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
		if (InterlockedDecrement(&m_IOCount) == 0)
			ReleaseSession();
	}

	CSerializationBuf::ChunkFreeForcibly();
	return 0;
}

UINT __stdcall CLanClient::WorkerThread(LPVOID pNetServer)
{
	return ((CLanClient*)pNetServer)->Worker();
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CLanClient::RecvPost()
{
	OVERLAPPEDIODATA &RecvIOData = m_RecvIOData;
	if (RecvIOData.RingBuffer.IsFull())
	{
		// 현재 m_IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
		if (InterlockedDecrement(&m_IOCount) == 0)
		{
			ReleaseSession();
			return POST_RETVAL_ERR_SESSION_DELETED;
		}
		return POST_RETVAL_ERR;
	}

	int BrokenSize = RecvIOData.RingBuffer.GetNotBrokenPutSize();
	int RestSize = RecvIOData.RingBuffer.GetFreeSize() - BrokenSize;
	int BufCount = 1;
	DWORD Flag = 0;

	WSABUF wsaBuf[2];
	wsaBuf[0].buf = RecvIOData.RingBuffer.GetWriteBufferPtr();
	wsaBuf[0].len = BrokenSize;
	if (RestSize > 0)
	{
		wsaBuf[1].buf = RecvIOData.RingBuffer.GetBufferPtr();
		wsaBuf[1].len = RestSize;
		BufCount++;
	}
	InterlockedIncrement(&m_IOCount);
	int retval = WSARecv(m_sock, wsaBuf, BufCount, NULL, &Flag, &RecvIOData.Overlapped, 0);
	if (retval == SOCKET_ERROR)
	{
		int GetLastErr = WSAGetLastError();
		if (GetLastErr != ERROR_IO_PENDING)
		{
			// 현재 m_IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
			if (InterlockedDecrement(&m_IOCount) == 0)
			{
				ReleaseSession();
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

bool CLanClient::SendPacket(CSerializationBuf *pSerializeBuf)
{
	if (pSerializeBuf == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SERIALIZEBUF_NULL_ERR;
		OnError(&Error);
		return false;
	}
	
	//if (InterlockedIncrement(&m_IOCount) == 1)
	//{
	//	CSerializationBuf::Free(pSerializeBuf);
	//	InterlockedDecrement(&m_IOCount);
	//	//if (m_pSessionArray.IsUseSession)
	//	//	ReleaseSession(&m_pSessionArray);
	//	return false;
	//}
	
	WORD PayloadSize = pSerializeBuf->GetUseSize();
	pSerializeBuf->m_iWriteLast = pSerializeBuf->m_iWrite;
	pSerializeBuf->m_iWrite = 0;
	pSerializeBuf->m_iRead = 0;
	*pSerializeBuf << PayloadSize;
	
	m_SendIOData.SendQ.Enqueue(pSerializeBuf);
	SendPost();

	//if (InterlockedDecrement(&m_IOCount) == 0)
	//{
	//	//if (m_pSessionArray.IsUseSession)
	//	//	ReleaseSession(&m_pSessionArray);
	//	return false;
	//}

	return true;
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CLanClient::SendPost()
{
	OVERLAPPED_SEND_IO_DATA &SendIOData = m_SendIOData;
	while (1)
	{
		if (InterlockedCompareExchange(&SendIOData.IOMode, SENDING, NONSENDING))
			return POST_RETVAL_COMPLETE;

		int UseSize = SendIOData.SendQ.GetRestSize();
		if (UseSize == 0)
		{
			InterlockedExchange(&SendIOData.IOMode, NONSENDING);
			if (SendIOData.SendQ.GetRestSize() > 0)
				continue;
			return POST_RETVAL_COMPLETE;
		}
		else if (UseSize < 0)
		{
			if (InterlockedDecrement(&m_IOCount) == 0)
			{
				ReleaseSession();
				return POST_RETVAL_ERR_SESSION_DELETED;
			}
			InterlockedExchange(&SendIOData.IOMode, NONSENDING);
			return POST_RETVAL_ERR;
		}

		WSABUF wsaSendBuf[ONE_SEND_WSABUF_MAX];
		if (ONE_SEND_WSABUF_MAX < UseSize)
			UseSize = ONE_SEND_WSABUF_MAX;

		for (int i = 0; i < UseSize; ++i)
		{
			SendIOData.SendQ.Dequeue(&pSeirializeBufStore[i]);
			wsaSendBuf[i].buf = pSeirializeBufStore[i]->GetBufferPtr();
			wsaSendBuf[i].len = pSeirializeBufStore[i]->GetAllUseSize();
		}
		SendIOData.lBufferCount += UseSize;

		InterlockedIncrement(&m_IOCount);
		int retval = WSASend(m_sock, wsaSendBuf, UseSize, NULL, 0, &SendIOData.Overlapped, 0);
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

				// 현재 m_IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
				if (InterlockedDecrement(&m_IOCount) == 0)
				{
					ReleaseSession();
					return POST_RETVAL_ERR_SESSION_DELETED;
				}

				return POST_RETVAL_ERR;
			}
		}
	}
	return POST_RETVAL_ERR_SESSION_DELETED;
}
