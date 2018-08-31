#include "PreComfile.h"
#include "Profiler.h"
#include "Echo.h"
#include "LanServerSerializeBuf.h"
#include "Log.h"

CEcho::CEcho(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle, UINT MaxClient)
{
	_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR", L"Start\n");
	SetLogLevel(LOG_LEVEL::LOG_DEBUG);
	Start(IP, PORT, NumOfWorkerThread, IsNagle, MaxClient);
}

CEcho::~CEcho()
{
	_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR", L"End\n%d");
	Stop();
	WritingProfile();
}

void CEcho::OnClientJoin(UINT64 OutClientID)
{
	__int64 Payload = 0x7fffffffffffffff;
	// 직렬화 버퍼를 동적 할당해서 해당 포인터를
	// SendPacket 에 넣어 놓으면 Send 가 완료 되었을 시
	// 삭제됨

	//m_UserMap.insert({OutClientID, 0});
	Begin("Alloc");
	CSerializationBuf *SendBuf = CSerializationBuf::Alloc();
	End("Alloc");
	//InterlockedIncrement(&g_ULLConuntOfNew);
	WORD StackIndex = (WORD)(OutClientID >> SESSION_INDEX_SHIFT);

	*SendBuf << Payload;
	
	SendBuf->AddRefCount(SendBuf);
	SendPacket(OutClientID, SendBuf);
	CSerializationBuf::Free(SendBuf);
}

void CEcho::OnClientLeave(UINT64 LeaveClientID)
{
	//if (m_UserMap.erase(LeaveClientID) != 1)
	//{
	//	_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d", 0, 1001);
	//}
}

bool CEcho::OnConnectionRequest()
{
	return true;
}

void CEcho::OnRecv(UINT64 ReceivedSessionID, CSerializationBuf *ServerReceivedBuffer)
{
	//__int64 echo = 0;
	//*ServerReceivedBuffer >> echo;
	//CSerializationBuf SendBuf;
	//SendBuf << echo;
	//SendPacket(ReceivedSessionID, &SendBuf);

	__int64 echo = 0;
	*ServerReceivedBuffer >> echo;

	//if (m_UserMap.find(ReceivedSessionID) == m_UserMap.end())
	//{
	//	_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d", 0, 1002);
	//}
	// 직렬화 버퍼를 동적 할당해서 해당 포인터를
	// SendPacket 에 넣어 놓으면 Send 가 완료 되었을 시
	// 삭제됨
	Begin("Alloc");
	CSerializationBuf *SendBuf = CSerializationBuf::Alloc();
	End("Alloc");

	WORD StackIndex = (WORD)(ReceivedSessionID >> SESSION_INDEX_SHIFT);
	//InterlockedIncrement(&g_ULLConuntOfNew);
	*SendBuf << echo;

	SendBuf->AddRefCount(SendBuf);
	SendPacket(ReceivedSessionID, SendBuf);
	CSerializationBuf::Free(SendBuf);
}

void CEcho::OnSend()
{
	
}

void CEcho::OnWorkerThreadBegin()
{

}

void CEcho::OnWorkerThreadEnd()
{

}

void CEcho::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d", OutError->GetLastErr, OutError->LanServerErr);
		printf_s("==============================================================\n");
	}
}