#pragma once
#include "LanClient.h"

class CSerializationBuf;

class CLanEcho : public CLanClient
{
public:
	virtual void OnConnectionComplete();

	virtual void OnRecv(CSerializationBuf *OutReadBuf);
	virtual void OnSend();

	virtual void OnWorkerThreadBegin();
	virtual void OnWorkerThreadEnd();

	virtual void OnError(st_Error *OutError);

	CLanEcho();
	virtual ~CLanEcho();

	bool LanClientStart(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle);

	int GetEchoTPS() 
	{
		int TPS = EchoTPS;
		EchoTPS = 0;
		return TPS;
	}

private :
	__int64			EchoNumber;
	int				EchoTPS;
};