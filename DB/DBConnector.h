#pragma once

#include "include/mysql.h"
#include <codecvt>
// Query 함수 내에서 연결관련 에러 체크 
#include "include/errmsg.h"
#include "LockFreeStack.h"


/////////////////////////////////////////////////////////
// MySQL DB 연결 클래스
//
// 단순하게 MySQL Connector 를 통한 DB 연결만 관리한다.
//
// 스레드에 안전하지 않으므로 주의 해야 함.
// 여러 스레드에서 동시에 이를 사용한다면 개판이 됨.
//
/////////////////////////////////////////////////////////


#define dfQUERY_TIME_MAX				1000

#define dfRETRY_MAX						5

class CTLSDBConnector;
class CDBConnector
{
public:

	enum en_DB_CONNECTOR
	{
		eQUERY_MAX_LEN = 2048

	};

	CDBConnector(const char *szDBIP, const char *szUser, const char *szPassword, const char *szDBName, int iDBPort);
	CDBConnector(const WCHAR *szDBIP, const WCHAR *szUser, const WCHAR *szPassword, const WCHAR *szDBName, int iDBPort);
	virtual		~CDBConnector();

	//////////////////////////////////////////////////////////////////////
	// MySQL DB 연결
	//////////////////////////////////////////////////////////////////////
	bool		Connect();

	//////////////////////////////////////////////////////////////////////
	// MySQL DB 끊기
	//////////////////////////////////////////////////////////////////////
	bool		Disconnect();


	//////////////////////////////////////////////////////////////////////
	// 쿼리 날리고 결과셋 임시 보관
	//
	//////////////////////////////////////////////////////////////////////
	bool		Query(const WCHAR *szStringFormat, ...);
	bool		QueryCompleteString(const WCHAR *szCompleteString);

	// DBWriter 스레드의 Save 쿼리 전용
	// 결과셋을 저장하지 않음.
	bool		Query_Save(const WCHAR *szStringFormat, ...);
	bool		Query_SaveCompleteString(const WCHAR *szCompleteString);

	//////////////////////////////////////////////////////////////////////
	// 쿼리를 날린 뒤에 결과 뽑아오기.
	//
	// 결과가 없다면 NULL 리턴.
	//////////////////////////////////////////////////////////////////////
	MYSQL_ROW	FetchRow();

	//////////////////////////////////////////////////////////////////////
	// 한 쿼리에 대한 결과 모두 사용 후 정리.
	//////////////////////////////////////////////////////////////////////
	void		FreeResult();


	//////////////////////////////////////////////////////////////////////
	// Error 얻기.한 쿼리에 대한 결과 모두 사용 후 정리.
	//////////////////////////////////////////////////////////////////////
	int			GetLastError() { return m_iLastError; };
	WCHAR		*GetLastErrorMsg() { return m_szLastErrorMsg; }


private:

	//////////////////////////////////////////////////////////////////////
	// mysql 의 LastError 를 맴버변수로 저장한다.
	//////////////////////////////////////////////////////////////////////
	void												SaveLastError();

private:



	//-------------------------------------------------------------
	// MySQL 연결객체 본체
	//-------------------------------------------------------------
	MYSQL		m_MySQL;

	//-------------------------------------------------------------
	// MySQL 연결객체 포인터. 위 변수의 포인터임. 
	// 이 포인터의 null 여부로 연결상태 확인.
	//-------------------------------------------------------------
	MYSQL		*m_pMySQL;

	//-------------------------------------------------------------
	// 쿼리를 날린 뒤 Result 저장소.
	//
	//-------------------------------------------------------------
	MYSQL_RES	*m_pSqlResult;

	char		m_szDBIP[16];
	char		m_szDBUser[64];
	char		m_szDBPassword[64];
	char		m_szDBName[64];
	int			m_iDBPort;


	WCHAR		m_szQuery[eQUERY_MAX_LEN];
	char		m_szQueryUTF8[eQUERY_MAX_LEN];

	UINT		m_iLastError;
	WCHAR		m_szLastErrorMsg[128];

};

class CTLSDBConnector
{
public:

	CTLSDBConnector(const WCHAR *szDBIP, const WCHAR *szUser, const WCHAR *szPassword, const WCHAR *szDBName, int iDBPort);
	virtual		~CTLSDBConnector();

	//////////////////////////////////////////////////////////////////////
	// DB Connector 객체 포인터 얻기
	//////////////////////////////////////////////////////////////////////
	CDBConnector*	GetDBConnector();

private:

	int										m_iTLSIndex;							
	int										m_iDBPort;
	CLockFreeStack<CDBConnector*>			*m_pDBConnectorStack;

	WCHAR									m_szDBIP[16];
	WCHAR									m_szDBUser[64];
	WCHAR									m_szDBPassword[64];
	WCHAR									m_szDBName[64];
};
