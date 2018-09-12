#include "PreCompile.h"
#pragma comment(lib, "ws2_32")
#include "LanClient.h"
#include "CrashDump.h"
#include "LanServerSerializeBuf.h"
#include "Profiler.h"

extern CCrashDump g_dump;

CLanClient::CLanClient()
{

}

CLanClient::~CLanClient()
{

}

bool CLanClient::Start(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle)
{
	if (!LanClientOptionParsing())
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::PARSING_ERR;
		return false;
	}

	memcpy(m_IP, IP, sizeof(m_IP));
	int retval;
	WSADATA Wsa;
	if (WSAStartup(MAKEWORD(2, 2), &Wsa))
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::WSASTARTUP_ERR;
		OnError(&Error);
		return false;
	}

	m_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_sock == INVALID_SOCKET)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::LISTEN_SOCKET_ERR;
		OnError(&Error);
		return false;
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	InetPton(AF_INET, dfSERVER_IP, &serveraddr.sin_addr);
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(dfSERVER_PORT);

	if (connect(m_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::LISTEN_BIND_ERR;
		OnError(&Error);
		return false;
	}

	retval = setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&IsNagle, sizeof(int));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::SETSOCKOPT_ERR;
	}

	ZeroMemory(&m_RecvIOData.Overlapped, sizeof(OVERLAPPED));
	ZeroMemory(&m_SendIOData.Overlapped, sizeof(OVERLAPPED));

	m_pWorkerThreadHandle = new HANDLE[NumOfWorkerThread];
	// static �Լ����� LanClient ��ü�� �����ϱ� ���Ͽ� this �����͸� ���ڷ� �ѱ�
	for (int i = 0; i < NumOfWorkerThread; i++)
		m_pWorkerThreadHandle[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, NULL);
	if (m_pWorkerThreadHandle == 0)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::BEGINTHREAD_ERR;
		OnError(&Error);
		return false;
	}
	m_byNumOfWorkerThread = NumOfWorkerThread;

	m_hWorkerIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (m_hWorkerIOCP == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::WORKERIOCP_NULL_ERR;
		return false;
	}
	CreateIoCompletionPort((HANDLE)m_sock, m_hWorkerIOCP, m_sock, 0);

	m_IOCount = 1;
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

bool CLanClient::LanClientOptionParsing()
{
	_wsetlocale(LC_ALL, L"Korean");

	CParser parser;
	WCHAR cBuffer[BUFFER_MAX];

	FILE *fp;
	_wfopen_s(&fp, L"LanClientOption.txt", L"rt, ccs=UNICODE");

	int iJumpBOM = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int iFileSize = ftell(fp);
	fseek(fp, iJumpBOM, SEEK_SET);
	int FileSize = (int)fread_s(cBuffer, BUFFER_MAX, sizeof(WCHAR), BUFFER_MAX / 2, fp);
	int iAmend = iFileSize - FileSize; // ���� ���ڿ� ���� ����� ���� ������
	fclose(fp);

	cBuffer[iFileSize - iAmend] = '\0';
	WCHAR *pBuff = cBuffer;

	int HeaderCode;

	if (!parser.GetValue_Int(pBuff, L"SerializeBuffer", L"HeaderCode", &HeaderCode))
		return false;

	CSerializationBuf::byHeader = (BYTE)HeaderCode;

	return true;
}

UINT CLanClient::Worker()
{
	char cPostRetval;
	int retval;
	DWORD Transferred;
	SOCKET *pSession;
	LPOVERLAPPED lpOverlapped;
	ULONGLONG m_IOCount;
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
		if (lpOverlapped == NULL)
		{
			st_Error Error;
			Error.GetLastErr = WSAGetLastError();
			Error.LanClientErr = LANCLIENT_ERR::OVERLAPPED_NULL_ERR;
			OnError(&Error);
			continue;
		}
		// GQCS �� �Ϸ� ������ ���� ���
		else
		{
			// �ܺ� �����ڵ忡 ���� ����
			if (pSession == NULL)
			{
				PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, NULL);
				break;
			}
			// recv ��Ʈ
			if (lpOverlapped == &RecvIOData.Overlapped)
			{
				// ������ ������
				// �� �Ұ� �ֳ�?
				if (Transferred == 0)
				{
					// ���� m_IOCount�� �ٿ��� �������� 1�ϰ�� �ش� ������ ������
					m_IOCount = InterlockedDecrement(&m_IOCount);
					//if (m_IOCount == 0)
					//	ReleaseSession(pSession);

					continue;
				}

				RecvIOData.RingBuffer.MoveWritePos(Transferred);
				int RingBufferRestSize = RecvIOData.RingBuffer.GetUseSize();

				while (RingBufferRestSize > HEADER_SIZE)
				{
					CSerializationBuf &RecvSerializeBuf = *CSerializationBuf::Alloc();
					RecvSerializeBuf.m_iWrite = 0;
					RecvIOData.RingBuffer.Peek((char*)RecvSerializeBuf.m_pSerializeBuffer, HEADER_SIZE);
					// Code Size 1 + PayloadLength 2
					RecvSerializeBuf.m_iWrite += HEADER_SIZE;

					BYTE Code;
					WORD PayloadLength;
					RecvSerializeBuf >> Code >> PayloadLength;

					if (Code != CSerializationBuf::byHeader)
					{
						CSerializationBuf::Free(&RecvSerializeBuf);
						InterlockedDecrement(&m_IOCount);
						st_Error Err;
						Err.GetLastErr = 0;
						Err.LanClientErr = HEADER_CODE_ERR;
						OnError(&Err);
						break;
					}
					if (RecvIOData.RingBuffer.GetUseSize() < PayloadLength + HEADER_SIZE)
					{
						CSerializationBuf::Free(&RecvSerializeBuf);
						break;
					}
					RecvIOData.RingBuffer.RemoveData(HEADER_SIZE);

					retval = RecvIOData.RingBuffer.Dequeue(&RecvSerializeBuf.m_pSerializeBuffer[RecvSerializeBuf.m_iWrite], PayloadLength);
					RecvSerializeBuf.m_iWrite += retval;

					RingBufferRestSize -= (retval + sizeof(WORD));
					OnRecv(&RecvSerializeBuf);
					CSerializationBuf::Free(&RecvSerializeBuf);
				}

				cPostRetval = RecvPost();
			}
			// send ��Ʈ
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
				//InterlockedAdd(&pSession->Del, BufferCount); // ���� �ٽ� ������ �� ��
				//pSession->Del += BufferCount;
				//InterlockedAdd(&pSession->SendIOData.lBufferCount, -BufferCount); // ���� �ٽ� ������ �� ��

				OnSend();
				InterlockedExchange(&SendIOData.IOMode, NONSENDING); // ���� �ٽ� ������ �� ��
				cPostRetval = SendPost();
			}
		}

		OnWorkerThreadEnd();

		if (cPostRetval == POST_RETVAL_ERR_SESSION_DELETED)
			continue;
		m_IOCount = InterlockedDecrement(&m_IOCount);
		//if (m_IOCount == 0)
		//	ReleaseSession(pSession);
	}

	CSerializationBuf::ChunkFreeForcibly();
	return 0;
}

UINT __stdcall CLanClient::WorkerThread(LPVOID pNetServer)
{
	return ((CLanClient*)pNetServer)->Worker();
}

// �ش� �Լ��� ����Ϸ� �� pSession�� �������� �ʾ����� true �� �ܿ��� false�� ��ȯ��
char CLanClient::RecvPost()
{
	OVERLAPPEDIODATA &RecvIOData = m_RecvIOData;
	ULONGLONG m_IOCount;
	if (RecvIOData.RingBuffer.IsFull())
	{
		// ���� m_IOCount�� �ٿ��� �������� 1�ϰ�� �ش� ������ ������
		m_IOCount = InterlockedDecrement(&m_IOCount);
		if (m_IOCount == 0)
		{
			//ReleaseSession(pSession);
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
			// ���� m_IOCount�� �ٿ��� �������� 1�ϰ�� �ش� ������ ������
			m_IOCount = InterlockedDecrement(&m_IOCount);
			if (m_IOCount == 0)
			{
				//ReleaseSession(pSession);
				return POST_RETVAL_ERR_SESSION_DELETED;
			}
			st_Error Error;
			Error.GetLastErr = GetLastErr;
			Error.LanClientErr = LANCLIENT_ERR::WSARECV_ERR;
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
		Error.LanClientErr = LANCLIENT_ERR::SERIALIZEBUF_NULL_ERR;
		OnError(&Error);
		return false;
	}
	
	if (InterlockedIncrement(&m_IOCount) == 1)
	{
		CSerializationBuf::Free(pSerializeBuf);
		InterlockedDecrement(&m_IOCount);
		//if (m_pSessionArray.IsUseSession)
		//	ReleaseSession(&m_pSessionArray);

		return false;
	}
	
	WORD PayloadSize = pSerializeBuf->GetUseSize();
	pSerializeBuf->m_iWriteLast = pSerializeBuf->m_iWrite;
	pSerializeBuf->m_iWrite = 0;
	*pSerializeBuf << CSerializationBuf::byHeader << PayloadSize;

	m_SendIOData.SendQ.Enqueue(pSerializeBuf);
	SendPost();

	if (InterlockedDecrement(&m_IOCount) == 0)
	{
		//if (m_pSessionArray.IsUseSession)
		//	ReleaseSession(&m_pSessionArray);

		return false;
	}

	return true;
}

// �ش� �Լ��� ����Ϸ� �� pSession�� �������� �ʾ����� true �� �ܿ��� false�� ��ȯ��
char CLanClient::SendPost()
{
	OVERLAPPED_SEND_IO_DATA &SendIOData = m_SendIOData;
	while (1)
	{
		if (InterlockedCompareExchange(&SendIOData.IOMode, SENDING, NONSENDING))
			return true;

		ULONGLONG m_IOCount;
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
			m_IOCount = InterlockedDecrement(&m_IOCount);
			if (m_IOCount == 0)
			{
				//ReleaseSession(pSession);
				return POST_RETVAL_ERR_SESSION_DELETED;
			}
			InterlockedExchange(&SendIOData.IOMode, NONSENDING);
			return POST_RETVAL_ERR;
		}

		WSABUF wsaSendBuf[ONE_SEND_WSABUF_MAX];
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
				//if (err == WSAENOBUFS)
				//	dump.Crash();
				st_Error Error;
				Error.GetLastErr = err;
				Error.LanClientErr = LANCLIENT_ERR::WSASEND_ERR;
				OnError(&Error);

				// ���� m_IOCount�� �ٿ��� �������� 1�ϰ�� �ش� ������ ������
				m_IOCount = InterlockedDecrement(&m_IOCount);
				if (m_IOCount == 0)
				{
					//ReleaseSession(pSession);
					return POST_RETVAL_ERR_SESSION_DELETED;
				}

				return POST_RETVAL_ERR;
			}
		}
	}
	return POST_RETVAL_ERR_SESSION_DELETED;
}
