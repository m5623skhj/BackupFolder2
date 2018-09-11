#include "PreComfile.h"
#include "DBConnector.h"
#include <stdarg.h>
#include <strsafe.h>
#include "Log.h"

//////////////////////////////////////////////////////////////////////////////////
// DB Connector
//////////////////////////////////////////////////////////////////////////////////
CDBConnector::CDBConnector(const WCHAR *szDBIP, const WCHAR *szUser, const WCHAR *szPassword, const WCHAR *szDBName, int iDBPort)
	: m_iLastError(0), m_iDBPort(iDBPort)
{
	int Length = WideCharToMultiByte(CP_UTF8, 0, szDBIP, lstrlenW(szDBIP), NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, szDBIP, Length, m_szDBIP, Length, NULL, NULL);

	Length = WideCharToMultiByte(CP_UTF8, 0, szUser, Length, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, szUser, Length, m_szDBUser, Length, NULL, NULL);

	Length = WideCharToMultiByte(CP_UTF8, 0, szPassword, Length, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, szPassword, Length, m_szDBPassword, Length, NULL, NULL);

	Length = WideCharToMultiByte(CP_UTF8, 0, szDBName, Length, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, szDBName, Length, m_szDBName, Length, NULL, NULL);

	if (mysql_init(&m_MySQL) == NULL)
	{
		wprintf(L"Mysql Init Err\n");
		g_Dump.Crash();
	}
}

CDBConnector::CDBConnector(const char *szDBIP, const char *szUser, const char *szPassword, const char *szDBName, int iDBPort)
	: m_iLastError(0), m_iDBPort(iDBPort)
{
	memcpy(m_szDBIP, szDBIP, sizeof(m_szDBIP));
	memcpy(m_szDBUser, szUser, sizeof(m_szDBUser));
	memcpy(m_szDBPassword, szPassword, sizeof(m_szDBPassword));
	memcpy(m_szDBName, szDBName, sizeof(m_szDBName));

	if (mysql_init(&m_MySQL) == NULL)
	{
		wprintf(L"Mysql Init Err\n");
		g_Dump.Crash();
	}
}

CDBConnector::~CDBConnector()
{
	if(m_pMySQL != NULL)
		Disconnect();
}

bool CDBConnector::Connect()
{
	m_pMySQL = mysql_real_connect(&m_MySQL, m_szDBIP, (const char*)m_szDBUser, (const char*)m_szDBPassword,
		(const char*)m_szDBName, m_iDBPort, NULL, 0);
	if (m_pMySQL == NULL)
	{
		if (m_iLastError == CR_SOCKET_CREATE_ERROR ||
			m_iLastError == CR_CONNECTION_ERROR ||
			m_iLastError == CR_CONN_HOST_ERROR ||
			m_iLastError == CR_SERVER_GONE_ERROR ||
			m_iLastError == CR_TCP_CONNECTION ||
			m_iLastError == CR_SERVER_HANDSHAKE_ERR ||
			m_iLastError == CR_SERVER_LOST ||
			m_iLastError == CR_INVALID_CONN_HANDLE)
		{
			SaveLastError();
			return false;
		}

		int NumOfReConnect = 0;
		while (!mysql_real_connect(&m_MySQL, NULL, (const char*)m_szDBUser, (const char*)m_szDBPassword,
			(const char*)m_szDBName, m_iDBPort, NULL, 0))
		{
			++NumOfReConnect;

			if (NumOfReConnect >= dfRETRY_MAX)
			{
				SaveLastError();
				return false;
			}
			else if (m_iLastError == CR_SOCKET_CREATE_ERROR ||
				m_iLastError == CR_CONNECTION_ERROR ||
				m_iLastError == CR_CONN_HOST_ERROR ||
				m_iLastError == CR_SERVER_GONE_ERROR ||
				m_iLastError == CR_TCP_CONNECTION ||
				m_iLastError == CR_SERVER_HANDSHAKE_ERR ||
				m_iLastError == CR_SERVER_LOST ||
				m_iLastError == CR_INVALID_CONN_HANDLE)
			{
				SaveLastError();
				return false;
			}
		}
	}

	return true;
}

bool CDBConnector::Disconnect()
{
	mysql_close(m_pMySQL);
	m_pMySQL = NULL;
	return true;
}

bool CDBConnector::Query(const WCHAR *szStringFormat, ...)
{
	WCHAR szLogBuf[512];

	va_list ArgList;
	va_start(ArgList, szStringFormat);

	StringCchVPrintfW(szLogBuf, 512, szStringFormat, ArgList);
	int WideCharLength = lstrlenW(szLogBuf);

	int Length = WideCharToMultiByte(CP_UTF8, 0, szLogBuf, WideCharLength, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, szLogBuf, WideCharLength, m_szQueryUTF8, Length, NULL, NULL);

	DWORD QuerySendBeginTime = GetTickCount();
	int MySQLErr = mysql_query(m_pMySQL, m_szQueryUTF8);
	DWORD QuerySendEndTime = GetTickCount();
	DWORD QuerySendTime = QuerySendEndTime - QuerySendBeginTime;
	if (QuerySendTime > dfQUERY_TIME_MAX)
		_LOG(LOG_LEVEL::LOG_DEBUG, L"Query Send Is Too Long Type", L"Send Time : %d\n", QuerySendTime);

	if (MySQLErr != 0)
	{
		int NumOfReSend = 0;
		
		while (MySQLErr != 0)
		{
			QuerySendBeginTime = GetTickCount();
			MySQLErr = mysql_query(m_pMySQL, m_szQueryUTF8);
			QuerySendEndTime = GetTickCount();
			QuerySendTime = QuerySendEndTime - QuerySendBeginTime;

			if (MySQLErr == 0)
				break;
			++NumOfReSend;
			if (NumOfReSend >= dfRETRY_MAX)
			{
				SaveLastError();
				return false;
			}
		}
	}
	m_pSqlResult = mysql_store_result(m_pMySQL);
	if (m_pSqlResult == NULL)
	{
		SaveLastError();
		return false;
	}

	return true;
}

bool CDBConnector::QueryCompleteString(const WCHAR *szCompleteString)
{
	int WideCharLength = lstrlenW(szCompleteString);

	int Length = WideCharToMultiByte(CP_UTF8, 0, szCompleteString, WideCharLength, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, szCompleteString, WideCharLength, m_szQueryUTF8, Length, NULL, NULL);

	DWORD QuerySendBeginTime = GetTickCount();
	int MySQLErr = mysql_query(m_pMySQL, m_szQueryUTF8);
	DWORD QuerySendEndTime = GetTickCount();
	DWORD QuerySendTime = QuerySendEndTime - QuerySendBeginTime;
	if (QuerySendTime > dfQUERY_TIME_MAX)
		_LOG(LOG_LEVEL::LOG_DEBUG, L"Query Send Is Too Long Type", L"Send Time : %d\n", QuerySendTime);

	if (MySQLErr != 0)
	{
		int NumOfReSend = 0;
		while (1)
		{
			QuerySendBeginTime = GetTickCount();
			MySQLErr = mysql_query(m_pMySQL, m_szQueryUTF8);
			QuerySendEndTime = GetTickCount();
			QuerySendTime = QuerySendEndTime - QuerySendBeginTime;
			if (QuerySendTime > dfQUERY_TIME_MAX)
				_LOG(LOG_LEVEL::LOG_DEBUG, L"Query Send Is Too Long Type", L"Send Time : %d\n", QuerySendTime);

			if (MySQLErr == 0)
				break;
			++NumOfReSend;
			if (NumOfReSend >= dfRETRY_MAX)
			{
				SaveLastError();
				return false;
			}
		}
	}
	m_pSqlResult = mysql_store_result(m_pMySQL);
	if (m_pSqlResult == NULL)
	{
		SaveLastError();
		return false;
	}

	return true;
}

bool CDBConnector::Query_Save(const WCHAR *szStringFormat, ...)
{
	WCHAR szLogBuf[512];

	va_list ArgList;
	va_start(ArgList, szStringFormat);

	StringCchVPrintfW(szLogBuf, 512, szStringFormat, ArgList);
	int WideCharLength = lstrlenW(szStringFormat);

	int Length = WideCharToMultiByte(CP_UTF8, 0, szStringFormat, WideCharLength, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, szStringFormat, WideCharLength, m_szQueryUTF8, Length, NULL, NULL);

	DWORD QuerySendBeginTime = GetTickCount();
	int MySQLErr = mysql_query(m_pMySQL, m_szQueryUTF8);
	DWORD QuerySendEndTime = GetTickCount();
	DWORD QuerySendTime = QuerySendEndTime - QuerySendBeginTime;
	if (QuerySendTime > dfQUERY_TIME_MAX)
		_LOG(LOG_LEVEL::LOG_DEBUG, L"Query Send Is Too Long Type", L"Send Time : %d\n", QuerySendTime);

	if (MySQLErr != 0)
	{
		int NumOfReSend = 0;
		while (MySQLErr != 0)
		{
			QuerySendBeginTime = GetTickCount();
			MySQLErr = mysql_query(m_pMySQL, m_szQueryUTF8);
			QuerySendEndTime = GetTickCount();
			QuerySendTime = QuerySendEndTime - QuerySendBeginTime;
			if (QuerySendTime > dfQUERY_TIME_MAX)
				_LOG(LOG_LEVEL::LOG_DEBUG, L"Query Send Is Too Long Type", L"Send Time : %d\n", QuerySendTime);

			if (MySQLErr == 0)
				break;
			++NumOfReSend;
			if (NumOfReSend >= dfRETRY_MAX)
			{
				SaveLastError();
				return false;
			}
		}
	}

	return true;
}

bool CDBConnector::Query_SaveCompleteString(const WCHAR *szCompleteString)
{
	int WideCharLength = lstrlenW(szCompleteString);

	int Length = WideCharToMultiByte(CP_UTF8, 0, szCompleteString, WideCharLength, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, szCompleteString, WideCharLength, m_szQueryUTF8, Length, NULL, NULL);

	DWORD QuerySendBeginTime = GetTickCount();
	int MySQLErr = mysql_query(m_pMySQL, m_szQueryUTF8);
	DWORD QuerySendEndTime = GetTickCount();
	DWORD QuerySendTime = QuerySendEndTime - QuerySendBeginTime;
	if (QuerySendTime > dfQUERY_TIME_MAX)
		_LOG(LOG_LEVEL::LOG_DEBUG, L"Query Send Is Too Long Type", L"Send Time : %d\n", QuerySendTime);

	if (mysql_query(m_pMySQL, m_szQueryUTF8) != 0)
	{
		int NumOfReSend = 0;
		while (MySQLErr != 0)
		{
			QuerySendBeginTime = GetTickCount();
			MySQLErr = mysql_query(m_pMySQL, m_szQueryUTF8);
			QuerySendEndTime = GetTickCount();
			QuerySendTime = QuerySendEndTime - QuerySendBeginTime;
			if (QuerySendTime > dfQUERY_TIME_MAX)
				_LOG(LOG_LEVEL::LOG_DEBUG, L"Query Send Is Too Long Type", L"Send Time : %d\n", QuerySendTime);

			if (MySQLErr == 0)
				break;
			++NumOfReSend;
			if (NumOfReSend >= dfRETRY_MAX)
			{
				SaveLastError();
				return false;
			}
		}
	}

	return true;
}

MYSQL_ROW CDBConnector::FetchRow()
{
	return mysql_fetch_row(m_pSqlResult);
}

void CDBConnector::FreeResult()
{
	mysql_free_result(m_pSqlResult);
}

void CDBConnector::SaveLastError()
{
	m_iLastError = mysql_errno(&m_MySQL);
}

//////////////////////////////////////////////////////////////////////////////////
// TLS DB Connector
//////////////////////////////////////////////////////////////////////////////////
CTLSDBConnector::CTLSDBConnector(const WCHAR *szDBIP, const WCHAR *szUser, const WCHAR *szPassword, const WCHAR *szDBName, int iDBPort) :
	m_iDBPort(0)
{
	//int Length = WideCharToMultiByte(CP_UTF8, 0, szDBIP, lstrlenW(szDBIP), NULL, 0, NULL, NULL);
	//WideCharToMultiByte(CP_UTF8, 0, szDBIP, Length, m_szDBIP, Length, NULL, NULL);
	//Length = WideCharToMultiByte(CP_UTF8, 0, szUser, Length, NULL, 0, NULL, NULL);
	//WideCharToMultiByte(CP_UTF8, 0, szUser, Length, m_szDBUser, Length, NULL, NULL);
	//Length = WideCharToMultiByte(CP_UTF8, 0, szPassword, Length, NULL, 0, NULL, NULL);
	//WideCharToMultiByte(CP_UTF8, 0, szPassword, Length, m_szDBPassword, Length, NULL, NULL);
	//Length = WideCharToMultiByte(CP_UTF8, 0, szDBName, Length, NULL, 0, NULL, NULL);
	//WideCharToMultiByte(CP_UTF8, 0, szDBName, Length, m_szDBName, Length, NULL, NULL);

	memcpy(m_szDBIP, szDBIP, sizeof(m_szDBIP));
	memcpy(m_szDBUser, szUser, sizeof(m_szDBUser));
	memcpy(m_szDBPassword, szPassword, sizeof(m_szDBPassword));
	memcpy(m_szDBName, szDBName, sizeof(m_szDBName));

	m_pDBConnectorStack = new CLockFreeStack<CDBConnector*>();

	m_iTLSIndex = TlsAlloc();
	if (m_iTLSIndex == TLS_OUT_OF_INDEXES)
	{
		wprintf(L"TLS Out Of Index Err\n");
		g_Dump.Crash();
	}
}

CTLSDBConnector::~CTLSDBConnector()
{
	TlsFree(m_iTLSIndex);
	int StackSize = m_pDBConnectorStack->GetRestStackSize();
	CDBConnector *pConnector;

	for (int i = 0; i < StackSize; ++i)
	{
		m_pDBConnectorStack->Pop(&pConnector);
		pConnector->Disconnect();
		delete pConnector;
	}

	delete m_pDBConnectorStack;
}

//bool CTLSDBConnector::Connect()
//{
//	return m_pDBConnector->Connect();
//}
//bool CTLSDBConnector::Disconnect()
//{
//	return m_pDBConnector->Disconnect();
//}
//bool CTLSDBConnector::Query(WCHAR *szStringFormat, ...)
//{
//	WCHAR szCompleteString[512];
//
//	va_list ArgList;
//	va_start(ArgList, szStringFormat);
//
//	StringCchVPrintfW(szCompleteString, 512, szStringFormat, ArgList);
//
//	return m_pDBConnector->QueryCompleteString(szCompleteString);
//}
//bool CTLSDBConnector::Query_Save(WCHAR *szStringFormat, ...)
//{
//	WCHAR szCompleteString[512];
//
//	va_list ArgList;
//	va_start(ArgList, szStringFormat);
//
//	StringCchVPrintfW(szCompleteString, 512, szStringFormat, ArgList);
//
//	return m_pDBConnector->Query_SaveCompleteString(szCompleteString);
//}
//MYSQL_ROW CTLSDBConnector::FetchRow()
//{
//	return m_pDBConnector->FetchRow();
//}
//void CTLSDBConnector::FreeResult()
//{
//	m_pDBConnector->FreeResult();
//}

CDBConnector* CTLSDBConnector::GetDBConnector()
{
	CDBConnector *pConnector = (CDBConnector*)TlsGetValue(m_iTLSIndex);
	if (pConnector == NULL)
	{
		pConnector = new CDBConnector(m_szDBIP, m_szDBUser, m_szDBPassword, m_szDBName, m_iDBPort);
		TlsSetValue(m_iTLSIndex, pConnector);
		m_pDBConnectorStack->Push(pConnector);
	}

	return pConnector;
}