#include "PreCompile.h"
#pragma comment(lib, "ws2_32")
#include "NetClient.h"
#include "NetServerSerializeBuffer.h"
#include "CrashDump.h"
#include "Profiler.h"
#include "Log.h"

CNetClient::CNetClient()
{

}

CNetClient::~CNetClient()
{

}

bool CNetClient::Start(const WCHAR *szOptionFileName)
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
		while (1)
		{
			Sleep(1000);

			if (connect(m_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr)) != SOCKET_ERROR)
				break;
		}
		//st_Error Error;
		//Error.GetLastErr = WSAGetLastError();
		//Error.ServerErr = SERVER_ERR::LISTEN_BIND_ERR;
		//OnError(&Error);
		//return false;
	}
	
	retval = setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&m_bIsNagleOn, sizeof(int));
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

	m_hWorkerIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, m_byNumOfUsingWorkerThread);
	if (m_hWorkerIOCP == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::WORKERIOCP_NULL_ERR;
		return false;
	}
	CreateIoCompletionPort((HANDLE)m_sock, m_hWorkerIOCP, (ULONG_PTR)&m_sock, 0);

	m_pWorkerThreadHandle = new HANDLE[m_byNumOfWorkerThread];
	// static 함수에서 NetServer 객체를 접근하기 위하여 this 포인터를 인자로 넘김
	for (int i = 0; i < m_byNumOfWorkerThread; i++)
		m_pWorkerThreadHandle[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, NULL);

	m_IOCount = 1;
	m_wNumOfReconnect = 0;
	RecvPost();

	if (InterlockedDecrement(&m_IOCount) == 0)
		ReleaseSession();
	OnConnectionComplete();

	return true;
}

void CNetClient::Stop()
{
	shutdown(m_sock, SD_BOTH);

	PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, NULL);
	WaitForMultipleObjects(m_byNumOfWorkerThread, m_pWorkerThreadHandle, TRUE, INFINITE);

	for (BYTE i = 0; i < m_byNumOfWorkerThread; ++i)
		CloseHandle(m_pWorkerThreadHandle[i]);
	CloseHandle(m_hWorkerIOCP);

	WSACleanup();
}

bool CNetClient::NetServerOptionParsing(const WCHAR *szOptionFileName)
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
	int FileSize = (int)fread_s(cBuffer, 16384, sizeof(WCHAR), iFileSize / 2, fp);
	int iAmend = iFileSize - FileSize; // 개행 문자와 파일 사이즈에 대한 보정값
	fclose(fp);

	cBuffer[iFileSize - iAmend] = '\0';
	WCHAR *pBuff = cBuffer;

	BYTE HeaderCode, XORCode1, XORCode2, DebugLevel;

	if (!parser.GetValue_String(pBuff, L"NETCLIENT", L"SERVER_IP", m_IP))
		return false;
	if (!parser.GetValue_Short(pBuff, L"NETCLIENT", L"SERVER_PORT", (short*)&m_wPort))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"NETCLIENT", L"WORKER_THREAD", &m_byNumOfWorkerThread))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"NETCLIENT", L"USE_IOCPWORKER", &m_byNumOfUsingWorkerThread))
		return false;
	if (!parser.GetValue_Byte(pBuff, L"NETCLIENT", L"NAGLE_ON", (BYTE*)&m_bIsNagleOn))
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

bool CNetClient::ReleaseSession()
{
	//if (InterlockedCompareExchange64((LONG64*)&m_IOCount, 0, df_RELEASE_VALUE) != df_RELEASE_VALUE)
	//	return false;

	int SendBufferRestSize = m_SendIOData.lBufferCount;
	int Rest = m_SendIOData.SendQ.GetRestSize();
	// SendPost 에서 옮겨졌으나 보내지 못한 직렬화 버퍼들을 반환함
	for (int i = 0; i < SendBufferRestSize; ++i)
		CNetServerSerializationBuf::Free(m_pSeirializeBufStore[i]);

	// 큐에서 아직 꺼내오지 못한 직렬화 버퍼가 있다면 해당 직렬화 버퍼들을 반환함
	if (Rest > 0)
	{
		CNetServerSerializationBuf *DeleteBuf;
		for (int i = 0; i < Rest; ++i)
		{
			m_SendIOData.SendQ.Dequeue(&DeleteBuf);
			CNetServerSerializationBuf::Free(DeleteBuf);
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

	int ConnectCnt = 0;
	_LOG(LOG_LEVEL::LOG_WARNING, L"NetClient", L"ReConnection %d / ThreadID : %d", m_wNumOfReconnect, GetCurrentThreadId());

	while (1)
	{
		if (connect(m_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr)) != SOCKET_ERROR )
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
	++m_IOCount;

	if (InterlockedDecrement(&m_IOCount) == 0)
		ReleaseSession();
	OnConnectionComplete();

	return true;
}

UINT CNetClient::Worker()
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
		if (lpOverlapped != NULL)
		{
			// 외부 종료코드에 의한 종료
			if (pSession == NULL)
			{
				PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, (LPOVERLAPPED)1);
				break;
			}
			// recv 파트
			SOCKET &IOCompleteSession = *pSession;
			if (lpOverlapped == &RecvIOData.Overlapped)
			{
				// 서버가 종료함(~ 끊김)
				if (Transferred == 0)
				{
					// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
					if (InterlockedDecrement(&m_IOCount) == 0)
						ReleaseSession();

					continue;
				}

				bool IsError = false;
				RecvIOData.RingBuffer.MoveWritePos(Transferred);
				int RingBufferRestSize = RecvIOData.RingBuffer.GetUseSize();

				while (RingBufferRestSize > df_HEADER_SIZE)
				{
					CNetServerSerializationBuf &RecvSerializeBuf = *CNetServerSerializationBuf::Alloc();
					RecvIOData.RingBuffer.Peek((char*)RecvSerializeBuf.m_pSerializeBuffer, df_HEADER_SIZE);
					RecvSerializeBuf.m_iRead = 0;

					BYTE Code;
					WORD PayloadLength;
					RecvSerializeBuf >> Code >> PayloadLength;

					if (Code != CNetServerSerializationBuf::m_byHeaderCode)
					{
						cPostRetval = POST_RETVAL_ERR;
						st_Error Err;
						Err.GetLastErr = 0;
						Err.ServerErr = HEADER_CODE_ERR;
						InterlockedIncrement((UINT*)&checksum);
						OnError(&Err);
						IsError = true;
						CNetServerSerializationBuf::Free(&RecvSerializeBuf);
						break;
					}
					if (RingBufferRestSize < PayloadLength + df_HEADER_SIZE)
					{
						if (PayloadLength > dfDEFAULTSIZE)
						{
							cPostRetval = POST_RETVAL_ERR;
							st_Error Err;
							Err.GetLastErr = 0;
							Err.ServerErr = PAYLOAD_SIZE_OVER_ERR;
							InterlockedIncrement((UINT*)&payloadOver);
							OnError(&Err);
							IsError = true;
						}
						CNetServerSerializationBuf::Free(&RecvSerializeBuf);
						break;
					}
					RecvIOData.RingBuffer.RemoveData(df_HEADER_SIZE);

					retval = RecvIOData.RingBuffer.Dequeue(&RecvSerializeBuf.m_pSerializeBuffer[RecvSerializeBuf.m_iWrite], PayloadLength);
					RecvSerializeBuf.m_iWrite += retval;
					if (!RecvSerializeBuf.Decode())
					{
						cPostRetval = POST_RETVAL_ERR;
						st_Error Err;
						Err.GetLastErr = 0;
						Err.ServerErr = CHECKSUM_ERR;
						InterlockedIncrement((UINT*)&HeaderCode);
						OnError(&Err);
						IsError = true;
						CNetServerSerializationBuf::Free(&RecvSerializeBuf);
						break;
					}

					RingBufferRestSize -= (retval + df_HEADER_SIZE);
					OnRecv(&RecvSerializeBuf);
					CNetServerSerializationBuf::Free(&RecvSerializeBuf);
				}
				if (!IsError)
					cPostRetval = RecvPost();
			}
			// send 파트
			else if (lpOverlapped == &SendIOData.Overlapped)
			{
				int BufferCount = SendIOData.lBufferCount;
				for (int i = 0; i < BufferCount; ++i)
					CNetServerSerializationBuf::Free(m_pSeirializeBufStore[i]);

				SendIOData.lBufferCount -= BufferCount;

				OnSend(BufferCount);
				InterlockedExchange(&SendIOData.IOMode, NONSENDING);
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

	CNetServerSerializationBuf::ChunkFreeForcibly();
	return 0;
}

UINT __stdcall CNetClient::WorkerThread(LPVOID pNetServer)
{
	return ((CNetClient*)pNetServer)->Worker();
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CNetClient::RecvPost()
{
	if (m_RecvIOData.RingBuffer.IsFull())
	{
		if (InterlockedDecrement(&m_IOCount) == 0)
		{
			ReleaseSession();
			return POST_RETVAL_ERR_SESSION_DELETED;
		}
		return POST_RETVAL_ERR;
	}

	int BrokenSize = m_RecvIOData.RingBuffer.GetNotBrokenPutSize();
	int RestSize = m_RecvIOData.RingBuffer.GetFreeSize() - BrokenSize;
	int BufCount = 1;
	DWORD Flag = 0;

	WSABUF wsaBuf[2];
	wsaBuf[0].buf = m_RecvIOData.RingBuffer.GetWriteBufferPtr();
	wsaBuf[0].len = BrokenSize;
	if (RestSize > 0)
	{
		wsaBuf[1].buf = m_RecvIOData.RingBuffer.GetBufferPtr();
		wsaBuf[1].len = RestSize;
		BufCount++;
	}

	InterlockedIncrement(&m_IOCount);
	ZeroMemory(&m_RecvIOData.Overlapped, sizeof(OVERLAPPED));
	int retval = WSARecv(m_sock, wsaBuf, BufCount, NULL, &Flag, &m_RecvIOData.Overlapped, 0);
	if (retval == SOCKET_ERROR)
	{
		int GetLastErr = WSAGetLastError();
		if (GetLastErr != ERROR_IO_PENDING)
		{
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

bool CNetClient::SendPacket(CNetServerSerializationBuf *pSerializeBuf)
{
	if (pSerializeBuf == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.ServerErr = SERVER_ERR::SERIALIZEBUF_NULL_ERR;
		OnError(&Error);
		return false;
	}

	if (!SessionAcquireLock())
	{
		SessionAcquireUnLock();
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

	m_SendIOData.SendQ.Enqueue(pSerializeBuf);

	SendPost();

	SessionAcquireUnLock();
	return true;
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CNetClient::SendPost()
{
	while (1)
	{
		if (InterlockedCompareExchange(&m_SendIOData.IOMode, SENDING, NONSENDING))
			return true;

		int UseSize = m_SendIOData.SendQ.GetRestSize();
		if (UseSize == 0)
		{
			InterlockedExchange(&m_SendIOData.IOMode, NONSENDING);
			if (m_SendIOData.SendQ.GetRestSize() > 0)
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
			InterlockedExchange(&m_SendIOData.IOMode, NONSENDING);
			return POST_RETVAL_ERR;
		}

		WSABUF wsaSendBuf[ONE_SEND_WSABUF_MAX];
		if (ONE_SEND_WSABUF_MAX < UseSize)
			UseSize = ONE_SEND_WSABUF_MAX;

		for (int i = 0; i < UseSize; ++i)
		{
			m_SendIOData.SendQ.Dequeue(&m_pSeirializeBufStore[i]);
			wsaSendBuf[i].buf = m_pSeirializeBufStore[i]->GetBufferPtr();
			wsaSendBuf[i].len = m_pSeirializeBufStore[i]->GetAllUseSize();
		}
		m_SendIOData.lBufferCount += UseSize;

		InterlockedIncrement(&m_IOCount);
		ZeroMemory(&m_SendIOData.Overlapped, sizeof(OVERLAPPED));

		int retval = WSASend(m_sock, wsaSendBuf, UseSize, NULL, 0, &m_SendIOData.Overlapped, 0);
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

bool CNetClient::SessionAcquireLock()
{
	InterlockedIncrement(&m_IOCount);

	return true;
}

void CNetClient::SessionAcquireUnLock()
{
	if (InterlockedDecrement(&m_IOCount) == 0)
		ReleaseSession();
}
