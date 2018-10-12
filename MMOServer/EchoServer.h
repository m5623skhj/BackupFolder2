#pragma once
#include "MMOServer.h"
#include "EchoPlayer.h"

class CMMOMonitoringLanClient;

class CEchoServer : public CMMOServer
{
public :
	CEchoServer();
	~CEchoServer();

	bool EchoServerStart(const WCHAR *szMMOServerOptionFileName, const WCHAR *szMonitoringClientFileName);
	void EchoServerStop();

	UINT GetNumOfUsingSerializeBuf();

private :
	// 인증 스레드의 한 프레임마다 호출됨
	virtual void OnAuth_Update();
	// 업데이트 스레드의 한 프레임마다 호출됨
	virtual void OnGame_Update();
	// 에러 상황 발생시 호출 됨
	virtual void OnError(st_Error *pError);

	bool ParsingEchoOption();

private :
	bool							m_bIsServerRunning;

	UINT							m_uiNumOfPlayer;
	CEchoPlayer						*m_pEchoPlayerArray;
	CMMOMonitoringLanClient			*m_pMonitoringClient;
};