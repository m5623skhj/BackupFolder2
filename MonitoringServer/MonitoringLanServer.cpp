#include "PreCompile.h"
#include "MonitoringLanServer.h"
#include "MonitoringNetServer.h"
#include "LanServerSerializeBuf.h"
#include "DBConnector.h"
#include "Log.h"
#include "MonitorProtocol.h"

CMonitoringLanServer::CMonitoringLanServer()
{
	for (int i = 0; i < dfNUM_OF_DATATYPE; ++i)
		InitializeCriticalSection(&m_csUnitData[i]);
}

CMonitoringLanServer::~CMonitoringLanServer()
{
	for (int i = 0; i < dfNUM_OF_DATATYPE; ++i)
		DeleteCriticalSection(&m_csUnitData[i]);
}

bool CMonitoringLanServer::MonitoringLanServerStart(const WCHAR *szLanServerOptionFileName, const WCHAR *szNetServerOptionFileName, const WCHAR *szMonitoringServerOption)
{
	m_pMonitoringNetServer = new CMonitoringNetServer();
	if (!Start(szLanServerOptionFileName))
		return false;
	if (!m_pMonitoringNetServer->MonitoringNetServerStart(szNetServerOptionFileName, szMonitoringServerOption))
		return false;

	m_hDBSendThreadHandle = (HANDLE)_beginthreadex(NULL, 0, DBSendThread, this, 0, NULL);
	if (m_hDBSendThreadHandle == NULL)
		return false;
	m_hSendEventHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (m_hSendEventHandle == NULL)
		return false;

	return true;
}

void CMonitoringLanServer::MonitoringLanServerStop()
{
	delete m_pMonitoringNetServer;
	
	SetEvent(m_hSendEventHandle);
	WaitForSingleObject(m_hDBSendThreadHandle, INFINITE);
	
	CloseHandle(m_hSendEventHandle);
	CloseHandle(m_hDBSendThreadHandle);

	Stop();
}

void CMonitoringLanServer::OnClientJoin(UINT64 OutClientID)
{

}

void CMonitoringLanServer::OnClientLeave(UINT64 ClientID)
{

}

bool CMonitoringLanServer::OnConnectionRequest()
{
	return true;
}

void CMonitoringLanServer::OnRecv(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf)
{
	WORD Type;
	*OutReadBuf >> Type;
	if (Type == en_PACKET_SS_MONITOR_DATA_UPDATE)
	{
		CDBConnector &DBConnector = *CDBConnector::GetDBConnector();

		st_MonitorToolUpdateData SendMonitorData;
		*OutReadBuf >> SendMonitorData.byDataType >> SendMonitorData.iDataValue >> SendMonitorData.iTimeStamp;
		for (int i = 0; i < dfNUM_OF_SERVER; ++i)
		{
			if (m_ServerLoginInfo[i].uiSessionID == ReceivedSessionID)
			{
				SendMonitorData.byServerNo = i + 1;
				break;
			}
		}

		BYTE UnitIndex = SendMonitorData.byDataType - 1;

		// m_csUnitData 임계영역 시작
		EnterCriticalSection(&m_csUnitData[UnitIndex]);

		m_UnitValue[UnitIndex].iLast = SendMonitorData.iDataValue;
		if (m_UnitValue[UnitIndex].iMax < SendMonitorData.iDataValue)
			m_UnitValue[UnitIndex].iMax = SendMonitorData.iDataValue;
		if(m_UnitValue[UnitIndex].iMin > SendMonitorData.iDataValue)
			m_UnitValue[UnitIndex].iMin = SendMonitorData.iDataValue;
		m_UnitValue[UnitIndex].iTotal += SendMonitorData.iDataValue;
		++m_UnitValue[UnitIndex].iNumOfRecv;
		
		// m_csUnitData 임계영역 끝
		LeaveCriticalSection(&m_csUnitData[UnitIndex]);

		m_pMonitoringNetServer->SendMonitoringInfoToNetClient(SendMonitorData);
	}
	else if (Type == en_PACKET_SS_MONITOR_LOGIN)
	{
		int ServerNo;
		*OutReadBuf >> ServerNo;

		if (ServerNo == dfMONITOR_SERVER_TYPE_LOGIN)
		{
			m_ServerLoginInfo[dfLOGIN_SERVER].bIsLoginServer = TRUE;
			m_ServerLoginInfo[dfLOGIN_SERVER].uiSessionID = ReceivedSessionID;
		}
		else if (ServerNo == dfMONITOR_SERVER_TYPE_GAME)
		{
			m_ServerLoginInfo[dfGAME_SERVER].bIsLoginServer = TRUE;
			m_ServerLoginInfo[dfGAME_SERVER].uiSessionID = ReceivedSessionID;
		}
		else if (ServerNo == dfMONITOR_SERVER_TYPE_CHAT)
		{
			m_ServerLoginInfo[dfCHAT_SERVER].bIsLoginServer = TRUE;
			m_ServerLoginInfo[dfCHAT_SERVER].uiSessionID = ReceivedSessionID;
		}
	}
}

void CMonitoringLanServer::OnSend(UINT64 ClientID, int sendsize)
{

}

void CMonitoringLanServer::OnWorkerThreadBegin()
{

}

void CMonitoringLanServer::OnWorkerThreadEnd()
{

}

void CMonitoringLanServer::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

UINT __stdcall CMonitoringLanServer::DBSendThread(LPVOID pMonitoringServer)
{
	return ((CMonitoringLanServer*)pMonitoringServer)->DBSender();
}

UINT CMonitoringLanServer::DBSender()
{
	CDBConnector::InitTLSDBConnet(L"127.0.0.1", L"root", L"zxcv123", L"logdb", dfDB_PORT);
	CDBConnector &DBConnector = *CDBConnector::GetDBConnector();

	while (1)
	{
		if (WaitForSingleObject(m_hSendEventHandle, 10000) == WAIT_TIMEOUT)
		{
			int QueryNumber = 0;
			for (; QueryNumber < dfMONITOR_DATA_TYPE_CHAT_ROOM; ++QueryNumber)
			{
				// m_csUnitData 임계영역 시작
				EnterCriticalSection(&m_csUnitData[QueryNumber]);
			
				if (m_UnitValue[QueryNumber].iNumOfRecv == 0)
				{
					LeaveCriticalSection(&m_csUnitData[QueryNumber]);
					continue;
				}

				DBConnector.SendQuery_Save(
L"INSERT INTO monitorlog (logtime, serverno, servername, type, value, min, max, avr) VALUES (NOW(), 3, \"ChattingServer\", %d, %d, %d, %d, %d)",
QueryNumber, m_UnitValue[QueryNumber].iLast, m_UnitValue[QueryNumber].iMin, m_UnitValue[QueryNumber].iMax, m_UnitValue[QueryNumber].iTotal / m_UnitValue[QueryNumber].iNumOfRecv);

				m_UnitValue[QueryNumber].iLast = 0;
				m_UnitValue[QueryNumber].iMax = 0;
				m_UnitValue[QueryNumber].iMin = 99999;
				m_UnitValue[QueryNumber].iTotal = 0;
				m_UnitValue[QueryNumber].iNumOfRecv = 0;

				// m_csUnitData 임계영역 끝
				LeaveCriticalSection(&m_csUnitData[QueryNumber]);
			}
		}
		else
			break;
	}

	return 0;
}
