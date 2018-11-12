#include "PreCompile.h"

#include "MatchmakingServer.h"
#include "MatchmakingLanClient.h"
#include "LanServerSerializeBuf.h"
#include "Log.h"

#include "Protocol/CommonProtocol.h"


CMatchmakingLanClient::CMatchmakingLanClient()
{

}

CMatchmakingLanClient::~CMatchmakingLanClient()
{

}

bool CMatchmakingLanClient::MatchmakingLanClientStart(CMatchmakingServer *pMatchmakingNetServer, int ServerNo, const WCHAR *szMatchmakingLanServerOptionFile)
{
	m_pMatchmakingNetServer = pMatchmakingNetServer;
	m_iServerNo = ServerNo;
	m_bConnectMasterServer = false;

	if(!Start(szMatchmakingLanServerOptionFile))
		return false;
	if (!MatchmakingLanClientOptionParsing(szMatchmakingLanServerOptionFile))
		return false;

	return true;
}

void CMatchmakingLanClient::MatchmakingLanClientStop()
{
	Stop();
}

void CMatchmakingLanClient::OnConnectionComplete()
{
	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();

	WORD Type = en_PACKET_MAT_MAS_REQ_SERVER_ON;
	SendBuf << Type << m_iServerNo;
	SendBuf.WriteBuffer(m_MasterToken, sizeof(m_MasterToken));

	SendPacket(&SendBuf);
}

void CMatchmakingLanClient::OnRecv(CSerializationBuf *OutReadBuf)
{
	WORD Type;

	*OutReadBuf >> Type;
	
	switch (Type)
	{
	case en_PACKET_MAT_MAS_RES_SERVER_ON:
		m_bConnectMasterServer = true;
		break;
	case en_PACKET_MAT_MAS_RES_GAME_ROOM:
		m_pMatchmakingNetServer->SendToClient_GameRoom(OutReadBuf);
		break;
	default:
		break;
	}
}

void CMatchmakingLanClient::OnSend()
{

}

void CMatchmakingLanClient::OnWorkerThreadBegin()
{

}

void CMatchmakingLanClient::OnWorkerThreadEnd()
{

}

void CMatchmakingLanClient::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

void CMatchmakingLanClient::SendToMaster_GameRoom(UINT64 ClientKey, UINT64 AccountNo)
{
	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	
	WORD Type = en_PACKET_MAT_MAS_REQ_GAME_ROOM;
	SendBuf << Type << ClientKey << AccountNo;

	SendPacket(&SendBuf);
}

void CMatchmakingLanClient::SendToMaster_GameRoomEnterSuccess(WORD BattleServerNo, int RoomNo, UINT64 ClientKey)
{
	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	
	WORD Type = en_PACKET_MAT_MAS_REQ_ROOM_ENTER_SUCCESS;
	SendBuf << Type << BattleServerNo << RoomNo << ClientKey;

	SendPacket(&SendBuf);
}

void CMatchmakingLanClient::SendToMaster_GameRoomEnterFail(UINT64 ClientKey)
{
	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();

	WORD Type = en_PACKET_MAT_MAS_REQ_ROOM_ENTER_FAIL;
	SendBuf << Type << ClientKey;

	SendPacket(&SendBuf);
}

bool CMatchmakingLanClient::MatchmakingLanClientOptionParsing(const WCHAR *szOptionFileName)
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

	WCHAR MasterToken[32];
	
	if (!parser.GetValue_String(pBuff, L"MATCHMAKING_LAN_CLIENT", L"MASTER_TOKEN", MasterToken))
		return false;

	int Length = WideCharToMultiByte(CP_UTF8, 0, MasterToken, lstrlenW(MasterToken), NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, MasterToken, Length, m_MasterToken, Length, NULL, NULL);

	return true;
}
