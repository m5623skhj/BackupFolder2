#include "PreCompile.h"
#include "EchoServer.h"
#include "EchoPlayer.h"
#include "NetServerSerializeBuffer.h"
#include "Parse.h"
#include "ServerCommon.h"
#include "Log.h"
#include "MMOMonitoringLanClient.h"

CEchoServer::CEchoServer() : m_bIsServerRunning(false)
{
	if (!ParsingEchoOption())
		g_Dump.Crash();

	InitMMOServer(m_uiNumOfPlayer);
	
	m_pEchoPlayerArray = new CEchoPlayer[m_uiNumOfPlayer];
	for (UINT i = 0; i < m_uiNumOfPlayer; ++i)
	{
		LinkSession(&m_pEchoPlayerArray[i], i);
	}
}

CEchoServer::~CEchoServer()
{
	if (!m_bIsServerRunning)
	{
		Stop();
		delete[] m_pEchoPlayerArray;
	}
}

bool CEchoServer::EchoServerStart(const WCHAR *szMMOServerOptionFileName, const WCHAR *szMonitoringClientFileName)
{
	//m_pMonitoringClient = new CMMOMonitoringLanClient();

	if (!Start(szMMOServerOptionFileName))
		return false;

	//m_pMonitoringClient->MonitoringLanClientStart(szMonitoringClientFileName, GetNumOfAllSession(),
	//	GetNumOfAuthSession(), GetNumOfGameSession(), GetAuthFPSPtr(), GetGameFPSPtr(), GetNumOfWaitRoom(),
	//	GetNumOfPlayRoom());

	m_bIsServerRunning = true;

	return true;
}

void CEchoServer::EchoServerStop()
{
	Stop();
	delete[] m_pEchoPlayerArray;

	m_bIsServerRunning = false;
}

void CEchoServer::OnAuth_Update()
{

}

void CEchoServer::OnGame_Update()
{

}

void CEchoServer::OnError(st_Error *pError)
{
	if (pError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n%d\n", pError->GetLastErr, pError->ServerErr, pError->Line);
		printf_s("==============================================================\n");
	}
}

bool CEchoServer::ParsingEchoOption()
{
	_wsetlocale(LC_ALL, L"Korean");

	CParser parser;
	WCHAR cBuffer[BUFFER_MAX];

	FILE *fp;
	_wfopen_s(&fp, L"EchoServerOption.txt", L"rt, ccs=UNICODE");

	int iJumpBOM = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int iFileSize = ftell(fp);
	fseek(fp, iJumpBOM, SEEK_SET);
	int FileSize = (int)fread_s(cBuffer, BUFFER_MAX, sizeof(WCHAR), BUFFER_MAX / 2, fp);
	int iAmend = iFileSize - FileSize; // 개행 문자와 파일 사이즈에 대한 보정값
	fclose(fp);

	cBuffer[iFileSize - iAmend] = '\0';
	WCHAR *pBuff = cBuffer;

	if (!parser.GetValue_Int(pBuff, L"ECHO", L"NUM_OF_PLAYER", (int*)&m_uiNumOfPlayer))
		return false;

	return true;
}

UINT CEchoServer::GetNumOfUsingSerializeBuf()
{
	return CNetServerSerializationBuf::GetUsingSerializeBufNodeCount();
}
