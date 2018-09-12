#include "PreCompile.h"
#include "LanEchoTest.h"
#include "LanServerSerializeBuf.h"

CLanEcho::CLanEcho()
	: EchoNumber(0), EchoTPS(0)
{
}

CLanEcho::~CLanEcho()
{
	Stop();
}

bool CLanEcho::LanClientStart(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle)
{
	return Start(IP, PORT, NumOfWorkerThread, IsNagle);
}

void CLanEcho::OnConnectionComplete()
{
	__int64 High = rand();
	__int64 Low = rand();
	EchoNumber += (High << 31);
	EchoNumber += Low;

	CSerializationBuf *SendBuf = CSerializationBuf::Alloc();
	
	*SendBuf << EchoNumber;
	
	SendBuf->AddRefCount(SendBuf);
	SendPacket(SendBuf);
	CSerializationBuf::Free(SendBuf);

	++EchoTPS;
}

void CLanEcho::OnRecv(CSerializationBuf *OutReadBuf)
{
	__int64 RecvNumber;
	*OutReadBuf >> RecvNumber;
	if (RecvNumber != EchoNumber)
	{
		printf("ERR\n");
		Sleep(INFINITE);
	}

	int High = rand();
	int Low = rand();
	EchoNumber += (High << 31);
	EchoNumber += Low;

	CSerializationBuf *SendBuf = CSerializationBuf::Alloc();

	*SendBuf << EchoNumber;

	SendBuf->AddRefCount(SendBuf);
	SendPacket(SendBuf);
	CSerializationBuf::Free(SendBuf);

	++EchoTPS;
}

void CLanEcho::OnSend()
{

}

void CLanEcho::OnWorkerThreadBegin()
{

}

void CLanEcho::OnWorkerThreadEnd()
{

}

void CLanEcho::OnError(st_Error *OutError)
{

}
