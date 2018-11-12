#include "PreCompile.h"
#include "MatchmakingServer.h"
#include "MatchmakingLanClient.h"
#include "MatchmakingMonitoringClient.h"

#include "NetServerSerializeBuffer.h"
#include "Log.h"
#include "DBConnector.h"
#include "CToHTTP.h"

#include "Protocol/CommonProtocol.h"

#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;

void CMatchmakingServer::LoginProc(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf)
{
	CNetServerSerializationBuf &SendBuf = *CNetServerSerializationBuf::Alloc();

	WORD Type = en_PACKET_CS_MATCH_RES_LOGIN;
	BYTE Status;

	if (OutReadBuf->GetUseSize() != 78)
	{
		// 패킷의 사이즈가 프로토콜과 맞지 않음 (기타 오류로 포함시킴)
		Status = 4;
		SendBuf << Type << Status;
		SendPacket(ReceivedSessionID, &SendBuf);
		return;
	}

	char Sessionkey[64];
	UINT Ver_Code;
	INT64 AccountNo;
	CCToHTTP CToHTTP(5000, L"127.0.0.1");
	
	*OutReadBuf >> AccountNo;
	OutReadBuf->ReadBuffer(Sessionkey, sizeof(Sessionkey));
	*OutReadBuf >> Ver_Code;

	if (m_uiServerVersionCode != Ver_Code)
	{
		// 버전 오류
		Status = 5;
		SendBuf << Type << Status;
		SendPacket(ReceivedSessionID, &SendBuf);
		return;
	}

	char HTTPData[HTTP_SIZE_MAX];
	char HTTPBody[200];
	char tempBody[200];

	if (!CToHTTP.InitSocket())
	{
		printf("Init Socket Error\n");
		return;
	}
	sprintf_s(tempBody, "\"accountno\":\"%lld\"", AccountNo);
	// select_account.php 에 요청하여 유저의 계정 정보를 얻어옴
	WORD httpSize = CToHTTP.CreateHTTPData(HTTPData,
		L"http://127.0.0.1/shDBhttpAPI/select_account.php", tempBody);
	CToHTTP.ConnectToHTTP();
	short CompleteCode = CToHTTP.GetHTTPBody(HTTPBody, HTTPData, httpSize);
	// 요청한 http 가 실패하였다면
	if (CompleteCode != HTTP_COMPLETECODE)
	{
		// HTTP 오류 (기타 오류로 포함시킴)
		Status = 4;
		SendBuf << Type << Status;
		SendPacket(ReceivedSessionID, &SendBuf);
		return;
	}
	
	GenericDocument<UTF8<>> Doc;

	Doc.Parse(HTTPBody);
	GenericValue<UTF8<>> &ResultObject = Doc["result"];
	LONGLONG ResultCode = _atoi64(ResultObject.GetString());
	if (ResultCode == -10)
	{
		// 회원가입이 되어있지 않은 유저임
		Status = 3;
		SendBuf << Type << Status;
		SendPacket(ReceivedSessionID, &SendBuf);
		return;
	}
	else if (ResultCode != 1)
	{
		// SelectAccount 에서 회원가입이 되어있지 않은 오류 외의 오류
		Status = 4;
		SendBuf << Type << Status;
		SendPacket(ReceivedSessionID, &SendBuf);
		return;
	}

	GenericValue<UTF8<>> &SessionkeyObject = Doc["sessionkey"];
	if(!memcmp(Sessionkey, SessionkeyObject.GetString(), sizeof(Sessionkey)))
	{
		// 세션키 오류
		Status = 2;
		SendBuf << Type << Status;
		SendPacket(ReceivedSessionID, &SendBuf);
		return;
	}

	// m_UserMap 임계영역 시작
	EnterCriticalSection(&m_csUserMapLock);

	unordered_map<UINT64, st_USER*>::iterator UserMapIter = m_UserMap.find(ReceivedSessionID);
	if (UserMapIter == m_UserMap.end())
	{
		g_Dump.Crash();
	}
	st_USER *UserInfo = UserMapIter->second;

	// m_UserMap 임계영역 끝
	LeaveCriticalSection(&m_csUserMapLock);

	UINT64 ServerVersionCode = m_uiServerVersionCode;
	UserInfo->iAccountNo = AccountNo;
	UserInfo->uiClientKey = (ServerVersionCode << 32) + InterlockedIncrement(&m_uiUserNoCount);
	UserInfo->byStatus = en_STATUS_LOGIN_COMPLETE;

	// m_SessionIDMap 임계영역 시작
	EnterCriticalSection(&m_csSessionMapLock);

	m_SessionIDMap.insert({ UserInfo->uiClientKey, ReceivedSessionID });

	// m_SessionIDMap 임계영역 끝
	LeaveCriticalSection(&m_csSessionMapLock);

	InterlockedIncrement(&m_uiNumOfLoginCompleteUser);
}
