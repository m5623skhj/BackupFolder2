#include "PreComfile.h"
#pragma comment(lib, "ws2_32")
#include "LanClient.h"
#include "CrashDump.h"
#include "NetServerSerializeBuffer.h"
#include "Profiler.h"

extern CCrashDump g_dump;

CLanClient::CLanClient()
{

}

CLanClient::~CLanClient()
{

}

bool CLanClient::Start(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle, UINT MaxClient)
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

	m_Connet = socket(AF_INET, SOCK_STREAM, 0);
	if (m_Connet == INVALID_SOCKET)
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

	if (connect(m_Connet, (SOCKADDR*)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::LISTEN_BIND_ERR;
		OnError(&Error);
		return false;
	}

	retval = setsockopt(m_Connet, IPPROTO_TCP, TCP_NODELAY, (const char*)&IsNagle, sizeof(int));
	if (retval == SOCKET_ERROR)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::SETSOCKOPT_ERR;
	}

	m_pWorkerThreadHandle = new HANDLE[NumOfWorkerThread];
	// static 함수에서 LanClient 객체를 접근하기 위하여 this 포인터를 인자로 넘김
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

	m_hWorkerIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, /*NumOfWorkerThread*/2);
	if (m_hWorkerIOCP == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::WORKERIOCP_NULL_ERR;
		return false;
	}

	return true;
}

void CLanClient::Stop()
{
	closesocket(m_Connet);
	WaitForSingleObject(m_hAcceptThread, INFINITE);
	if (m_pSessionArray.IsUseSession)
	{
		shutdown(m_pSessionArray.sock, SD_BOTH);
	}

	// 이거 괜찮은거 맞음?
	// 아직 못 받은 데이터가 존재할 수 있는데
	// 종료용 완료통지를 넣어도 되는가?
	PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, NULL);
	WaitForMultipleObjects(m_byNumOfWorkerThread, m_pWorkerThreadHandle, TRUE, INFINITE);

	CloseHandle(m_hAcceptThread);
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
	int iAmend = iFileSize - FileSize; // 개행 문자와 파일 사이즈에 대한 보정값
	fclose(fp);

	cBuffer[iFileSize - iAmend] = '\0';
	WCHAR *pBuff = cBuffer;

	int HeaderCode, XORCode;

	if (!parser.GetValue_Int(pBuff, L"SerializeBuffer", L"HeaderCode", &HeaderCode))
		return false;
	if (!parser.GetValue_Int(pBuff, L"SerializeBuffer", L"XORKey", &XORCode))
		return false;

	CNetServerSerializationBuf::m_byHeaderCode = (BYTE)HeaderCode;
	CNetServerSerializationBuf::m_byXORCode = (BYTE)XORCode;

	return true;
}

bool CLanClient::ReleaseSession(Session *pSession)
{
	//int Rest = pSession->SendIOData.SendQ.GetRestSize();
	//int StoreNumber = 0;
	//while(pSession->pSeirializeBufStore[StoreNumber] != NULL)
	//{
	//	CNetServerSerializationBuf::Free(pSession->pSeirializeBufStore[StoreNumber]);
	//	pSession->pSeirializeBufStore[StoreNumber] = NULL;
	//	++StoreNumber;
	//}
	//pSession->Del += StoreNumber;
	//InterlockedAdd(&g_ULLConuntOfDel, Rest);

	if (InterlockedCompareExchange64((LONG64*)&pSession->IOCount, 0, df_RELEASE_VALUE) != df_RELEASE_VALUE)
		return false;

	int SendBufferRestSize = pSession->SendIOData.lBufferCount;
	int Rest = pSession->SendIOData.SendQ.GetRestSize();
	// SendPost 에서 옮겨졌으나 보내지 못한 직렬화 버퍼들을 반환함
	for (int i = 0; i < SendBufferRestSize; ++i)
	{
		Begin("Free");
		CNetServerSerializationBuf::Free(pSession->pSeirializeBufStore[i]);
		End("Free");
	}

	// 큐에서 아직 꺼내오지 못한 직렬화 버퍼가 있다면 해당 직렬화 버퍼들을 반환함
	if (Rest > 0)
	{
		CNetServerSerializationBuf *DeleteBuf;
		for (int i = 0; i < Rest; ++i)
		{
			pSession->SendIOData.SendQ.Dequeue(&DeleteBuf);
			Begin("Free");
			CNetServerSerializationBuf::Free(DeleteBuf);
			End("Free");
		}
		//pSession->Del += Rest;
	}

	//int Total = SendBufferRestSize + Rest;
	//pSession->Del += SendBufferRestSize;
	//InterlockedAdd(&g_ULLConuntOfDel, Total);
	//if (pSession->New != pSession->Del)
	//{
	//	dump.Crash();
	//}

	closesocket(pSession->sock);
	pSession->IsUseSession = FALSE;

	UINT64 ID = pSession->SessionID;
	WORD StackIndex = (WORD)(ID >> SESSION_INDEX_SHIFT);

	OnClientLeave(ID);

	return true;
}

bool CLanClient::DisConnect(UINT64 SessionID)
{
	WORD StackIndex = (WORD)(SessionID >> SESSION_INDEX_SHIFT);

	if (InterlockedIncrement(&m_pSessionArray.IOCount) == 1)
	{
		InterlockedDecrement(&m_pSessionArray.IOCount);
		if (m_pSessionArray.IsUseSession == FALSE)
			ReleaseSession(&m_pSessionArray);
		return false;
	}

	if (m_pSessionArray.IsUseSession == FALSE)
	{
		// 이후 컨텐츠가 들어간다면 발생할 가능성이 존재함
		//////////////////////////////////////////////////////
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::SESSION_NULL_ERR;
		OnError(&Error);
		return false;
		//////////////////////////////////////////////////////
	}

	shutdown(m_pSessionArray.sock, SD_BOTH);

	if (InterlockedDecrement(&m_pSessionArray.IOCount) == 0)
	{
		ReleaseSession(&m_pSessionArray);
		return false;
	}

	OnClientLeave(SessionID);
	return true;
}

UINT CLanClient::Worker()
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
		if (lpOverlapped == NULL)
		{
			st_Error Error;
			Error.GetLastErr = WSAGetLastError();
			Error.LanClientErr = LANCLIENT_ERR::OVERLAPPED_NULL_ERR;
			OnError(&Error);
			continue;
		}
		// GQCS 에 완료 통지가 왔을 경우
		else
		{
			// 외부 종료코드에 의한 종료
			if (pSession == NULL)
			{
				PostQueuedCompletionStatus(m_hWorkerIOCP, 0, 0, NULL);
				break;
			}
			// recv 파트
			if (lpOverlapped == &pSession->RecvIOData.Overlapped)
			{
				// 클라이언트가 종료함
				if (Transferred == 0)
				{
					// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
					IOCount = InterlockedDecrement(&pSession->IOCount);
					if (IOCount == 0)
						ReleaseSession(pSession);

					continue;
				}

				pSession->RecvIOData.RingBuffer.MoveWritePos(Transferred);
				int RingBufferRestSize = pSession->RecvIOData.RingBuffer.GetUseSize();

				while (RingBufferRestSize > df_HEADER_SIZE)
				{
					CNetServerSerializationBuf &RecvSerializeBuf = *CNetServerSerializationBuf::Alloc();
					RecvSerializeBuf.m_iWrite = 0;
					int HeaderSize = pSession->RecvIOData.RingBuffer.Peek((char*)RecvSerializeBuf.m_pSerializeBuffer, df_HEADER_SIZE);
					// Code Size 1 + PayloadLength 2
					RecvSerializeBuf.m_iWrite += df_HEADER_SIZE;

					BYTE Code;
					WORD PayloadLength;
					RecvSerializeBuf >> Code >> PayloadLength;

					if (Code != CNetServerSerializationBuf::m_byHeaderCode)
					{
						CNetServerSerializationBuf::Free(&RecvSerializeBuf);
						InterlockedDecrement(&pSession->IOCount);
						st_Error Err;
						Err.GetLastErr = 0;
						Err.LanClientErr = HEADER_CODE_ERR;
						OnError(&Err);
						break;
					}
					if (pSession->RecvIOData.RingBuffer.GetUseSize() < PayloadLength + df_HEADER_SIZE)
					{
						CNetServerSerializationBuf::Free(&RecvSerializeBuf);
						break;
					}
					pSession->RecvIOData.RingBuffer.RemoveData(df_HEADER_SIZE);

					retval = pSession->RecvIOData.RingBuffer.Dequeue(&RecvSerializeBuf.m_pSerializeBuffer[RecvSerializeBuf.m_iWrite], PayloadLength);
					RecvSerializeBuf.m_iWrite += retval;
					if (!RecvSerializeBuf.Decode())
					{
						CNetServerSerializationBuf::Free(&RecvSerializeBuf);
						InterlockedDecrement(&pSession->IOCount);
						st_Error Err;
						Err.GetLastErr = 0;
						Err.LanClientErr = CHECKSUM_ERR;
						OnError(&Err);
						break;
					}

					RingBufferRestSize -= (retval + sizeof(WORD));
					OnRecv(pSession->SessionID, &RecvSerializeBuf);
					CNetServerSerializationBuf::Free(&RecvSerializeBuf);
				}

				cPostRetval = RecvPost(pSession);
			}
			// send 파트
			else if (lpOverlapped == &pSession->SendIOData.Overlapped)
			{
				//int BufferCount = 0;
				//while (pSession->pSeirializeBufStore[BufferCount] != NULL)
				//{
				//		CNetServerSerializationBuf::Free(pSession->pSeirializeBufStore[BufferCount]);
				//		pSession->pSeirializeBufStore[BufferCount] = NULL;
				//		++BufferCount;
				//}
				int BufferCount = pSession->SendIOData.lBufferCount;
				for (int i = 0; i < BufferCount; ++i)
				{
					Begin("Free");
					CNetServerSerializationBuf::Free(pSession->pSeirializeBufStore[i]);
					End("Free");

					////////////////////////////////////////
					//pSession->pSeirializeBufStore[i] = NULL;
					////////////////////////////////////////
				}

				pSession->SendIOData.lBufferCount -= BufferCount;
				//InterlockedAdd(&g_ULLConuntOfDel, BufferCount);
				//InterlockedAdd(&pSession->Del, BufferCount); // 여기 다시 생각해 볼 것
				//pSession->Del += BufferCount;
				//InterlockedAdd(&pSession->SendIOData.lBufferCount, -BufferCount); // 여기 다시 생각해 볼 것

				OnSend();
				InterlockedExchange(&pSession->SendIOData.IOMode, NONSENDING); // 여기 다시 생각해 볼 것
				cPostRetval = SendPost(pSession);
			}
		}

		OnWorkerThreadEnd();

		if (cPostRetval == POST_RETVAL_ERR_SESSION_DELETED)
			continue;
		IOCount = InterlockedDecrement(&pSession->IOCount);
		if (IOCount == 0)
			ReleaseSession(pSession);
	}

	CNetServerSerializationBuf::ChunkFreeForcibly();
	return 0;
}

UINT __stdcall CLanClient::WorkerThread(LPVOID pNetServer)
{
	return ((CLanClient*)pNetServer)->Worker();
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CLanClient::RecvPost(Session *pSession)
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
			Error.LanClientErr = LANCLIENT_ERR::WSARECV_ERR;
			OnError(&Error);
			return POST_RETVAL_ERR;
		}
	}
	return POST_RETVAL_COMPLETE;
}

bool CLanClient::SendPacket(UINT64 SessionID, CNetServerSerializationBuf *pSerializeBuf)
{
	WORD StackIndex = (WORD)(SessionID >> SESSION_INDEX_SHIFT);

	if (pSerializeBuf == NULL)
	{
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::SERIALIZEBUF_NULL_ERR;
		OnError(&Error);
		return false;
	}

	if (m_pSessionArray.SessionID != SessionID)
	{
		Begin("Free");
		CNetServerSerializationBuf::Free(pSerializeBuf);
		End("Free");

		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::INCORRECT_SESSION_ERR;
		OnError(&Error);
		return false;
	}

	if (InterlockedIncrement(&m_pSessionArray.IOCount) == 1)
	{
		CNetServerSerializationBuf::Free(pSerializeBuf);
		InterlockedDecrement(&m_pSessionArray.IOCount);
		if (m_pSessionArray.IsUseSession)
			ReleaseSession(&m_pSessionArray);

		return false;
	}

	if (m_pSessionArray.IsUseSession == FALSE)
	{
		Begin("Free");
		CNetServerSerializationBuf::Free(pSerializeBuf);
		End("Free");

		// 이후 컨텐츠가 들어간다면 발생할 가능성이 존재함
		//////////////////////////////////////////////////////
		st_Error Error;
		Error.GetLastErr = WSAGetLastError();
		Error.LanClientErr = LANCLIENT_ERR::SESSION_NULL_ERR;
		OnError(&Error);
		//////////////////////////////////////////////////////
		return false;
	}

	//InterlockedIncrement(&m_pSessionArray[StackIndex].New);
	//pSerializeBuf->WritePtrSetHeader();
	pSerializeBuf->m_iWriteLast = pSerializeBuf->m_iWrite;
	pSerializeBuf->m_iWrite = 0;
	pSerializeBuf->Encode();

	m_pSessionArray.SendIOData.SendQ.Enqueue(pSerializeBuf);
	SendPost(&m_pSessionArray);

	if (InterlockedDecrement(&m_pSessionArray.IOCount) == 0)
	{
		if (m_pSessionArray.IsUseSession)
			ReleaseSession(&m_pSessionArray);

		return false;
	}

	return true;
}

// 해당 함수가 정상완료 및 pSession이 해제되지 않았으면 true 그 외에는 false를 반환함
char CLanClient::SendPost(Session *pSession)
{
	while (1)
	{
		if (InterlockedCompareExchange(&pSession->SendIOData.IOMode, SENDING, NONSENDING))
			return true;

		ULONGLONG IOCount;
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
			IOCount = InterlockedDecrement(&pSession->IOCount);
			if (IOCount == 0)
			{
				ReleaseSession(pSession);
				return POST_RETVAL_ERR_SESSION_DELETED;
			}
			InterlockedExchange(&pSession->SendIOData.IOMode, NONSENDING);
			return POST_RETVAL_ERR;
		}

		WSABUF wsaSendBuf[ONE_SEND_WSABUF_MAX];
		for (int i = 0; i < UseSize; ++i)
		{
			pSession->SendIOData.SendQ.Dequeue(&pSession->pSeirializeBufStore[i]);
			wsaSendBuf[i].buf = pSession->pSeirializeBufStore[i]->GetBufferPtr();
			wsaSendBuf[i].len = pSession->pSeirializeBufStore[i]->GetAllUseSize();
		}
		pSession->SendIOData.lBufferCount += UseSize;
		//InterlockedAdd(&pSession->SendIOData.lBufferCount, UseSize);

		InterlockedIncrement(&pSession->IOCount);
		int retval = WSASend(pSession->sock, wsaSendBuf, UseSize, NULL, 0, &pSession->SendIOData.Overlapped, 0);
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

				// 현재 IOCount를 줄여서 이전값이 1일경우 해당 세션을 삭제함
				IOCount = InterlockedDecrement(&pSession->IOCount);
				if (IOCount == 0)
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
