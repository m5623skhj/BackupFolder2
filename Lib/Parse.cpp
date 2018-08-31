#include "PreComfile.h"
#include "Parse.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


Parser::Parser()
{

}

Parser::~Parser()
{
	
}

bool Parser::GetNextWord(const WCHAR *pKeyName, bool *bIsStop)
{
	bool bIsCorrectWord = false;
	if (**m_ppBufferPointer == '\0' ||
		**m_ppBufferPointer == ':')
	{
		KeyError(pKeyName);
		*bIsStop = true;
		return false;
	}
	bIsCorrectWord = GetStringWord(pKeyName);
	SkipToToken();

	return bIsCorrectWord;
}

void Parser::SkipToToken()
{
	// 찾던 문자와 다른 문자일 경우 다음 토큰이 나올 때 까지 전부 버림
	// 찾던 문자가 들어왔다면 바로 다음에 가리키는 문자가 토큰이므로 바로 처리됨
	while (1) 
	{
		if (IsToken())
		{
			*m_ppBufferPointer = *m_ppBufferPointer + 1;
			break;
		}
		else if (**m_ppBufferPointer == '/')
		{
			JumpComment(); // 단일 '/'인 경우 버퍼만 하나 뒤로 밀어줌
			break;
		}
		*m_ppBufferPointer = *m_ppBufferPointer + 1;
	}
}

void Parser::JumpComment()
{
	*m_ppBufferPointer = *m_ppBufferPointer + 1;
	switch (**m_ppBufferPointer)
	{
	case '/':
		while (**m_ppBufferPointer != 0x0a) //  라인 피드가 나올 때 까지 버퍼를 옮김
		{
			*m_ppBufferPointer = *m_ppBufferPointer + 1;
		}
		break;
	case '*':
		while (**m_ppBufferPointer != '*' || //  끝까지 이 형식이 안나오면 잘 못 쓴거라고 판단
			   *(*m_ppBufferPointer + 1) != '/')   //  */ 가 나올 때 까지 버퍼를 옮김
		{
			*m_ppBufferPointer = *m_ppBufferPointer + 1;
		}
		*m_ppBufferPointer = *m_ppBufferPointer + 1; //  이 부분에서 / 를 포인터가 가르키고 있음
		break;
	default:
		break;
	}

	*m_ppBufferPointer = *m_ppBufferPointer + 1; //  현재 가리키고 있는 포인터가 대상 문자이므로, 다음을 가리키게 함
}

bool Parser::JumpScope(bool *bIsStop)
{
	while (1)
	{
		if (**m_ppBufferPointer == ':')
		{
			*m_ppBufferPointer = *m_ppBufferPointer + 1;
			if (GetStringWord(m_pKeyScope))
			{
				return true;
			}
		}
		if (**m_ppBufferPointer == '\0')
		{
			ScopeError();
			*bIsStop = true;
			return false;
		}
		*m_ppBufferPointer = *m_ppBufferPointer + 1;
	}
}

bool Parser::GetStringWord(const WCHAR *pKeyName)
{
	while (1)
	{
		//////////////////////////////////
		//대조 문자열이 짧을 때 파일 문자열의 일부와 같은게 존재한다면?
		//ex) asdf, asdfzxcv
		//같은 문자열이 아님에도 여기까지만 검사 한 후에
		//스킵커맨드로 들어가게 됨
		//버그 발생
		// -> 널문자 만날 때 pBufferPointer의 다음 문자가 토큰인지 확인하는 방법으로 처리함
		if (**m_ppBufferPointer == *pKeyName) //  두개의 포인터를 옮기면서 각 버퍼 포인터가 가르키는 것을 대조함
		{
			*m_ppBufferPointer = *m_ppBufferPointer + 1;
			pKeyName++;
			if (*pKeyName == '\0')
			{
				if(IsToken())
					break;
				else
				{
					return false;
				}
			}
		}
		//버그 발생
		// -> 널문자 만날 때 pBufferPointer의 다음 문자가 토큰인지 확인하는 방법으로 처리함
		//////////////////////////////////
		else
		{
			return false;
		}
	}

	return true;
}

bool Parser::IsToken()
{
	if (**m_ppBufferPointer == ','  || **m_ppBufferPointer == '\"' ||
		**m_ppBufferPointer == 0x20 || **m_ppBufferPointer == 0x08 ||
		**m_ppBufferPointer == 0x09 || **m_ppBufferPointer == 0x0a ||
		**m_ppBufferPointer == 0x0d)
		return true;
	else
		return false;
}

bool Parser::FindWord(const WCHAR *pKeyName)
{
	bool bIsStop = false;
	while (!JumpScope(&bIsStop))// 해당 스코프가 나올 때 까지 검색
	{
		if (bIsStop)
			return false;
	}
	while (!GetNextWord(pKeyName, &bIsStop))// 키 네임과 같은 이름 검색
	{
		if (bIsStop)
			return false;
	}
	while (!GetNextWord(L"=", &bIsStop)); // '=' 검색

	return true;
}

void Parser::TokenError(int iCnt, const WCHAR *pKeyName)
{
	wprintf_s(L"해당 값 앞에 추가적인 토큰이 존재합니다 : %ls\n", pKeyName);
}

void Parser::KeyError(const WCHAR *pKeyName)
{
	wprintf_s(L"스코프에 진입하였으나, 해당 키값이 존재하지 않습니다 : %ls\n", pKeyName);
}

void Parser::ScopeError()
{
	wprintf_s(L"해당 스코프가 존재하지 않습니다 : %ls\n", m_pKeyScope);
}

bool Parser::GetValue_Int(WCHAR *pBufferPointer, const WCHAR *pKeyName, const WCHAR *pKeyScope, int *OutValue)
{
	m_ppBufferPointer = &pBufferPointer;
	m_pKeyScope = const_cast<WCHAR*>(pKeyScope);
	WCHAR TempBuffer[10];
	int iCnt = 0;
	int i = 0;
	memset(TempBuffer, 0, sizeof(TempBuffer));

	if (!FindWord(pKeyName))
		return false;
	while (!IsToken()) // 토큰이 나올 때 까지 검색
	{
		TempBuffer[iCnt] = **m_ppBufferPointer;
		iCnt++;
		*m_ppBufferPointer = *m_ppBufferPointer + 1;
	}
	if (iCnt <= 0)
	{
		TokenError(iCnt, pKeyName);
		return false;
	}

	TempBuffer[iCnt] = '\0';

	*OutValue = _wtoi(TempBuffer);
	return true;
}

bool Parser::GetValue_Double(WCHAR *pBufferPointer, const WCHAR *pKeyName, const WCHAR *pKeyScope, double *OutValue)
{
	m_ppBufferPointer = &pBufferPointer;
	m_pKeyScope = const_cast<WCHAR*>(pKeyScope);
	WCHAR TempBuffer[20];
	int iCnt = 0;
	int i = 0;
	memset(TempBuffer, 0, sizeof(TempBuffer));

	if (!FindWord(pKeyName))
		return false;
	while (!IsToken()) // 토큰이 나올 때 까지 검색
	{
		TempBuffer[iCnt] = **m_ppBufferPointer;
		iCnt++;
		*m_ppBufferPointer = *m_ppBufferPointer + 1;
	}
	if (iCnt <= 0)
	{
		TokenError(iCnt, pKeyName);
		return false;
	}

	TempBuffer[iCnt] = '\0';

	*OutValue = _wtof(TempBuffer);
	return true;
}

bool Parser::GetValue_Char(WCHAR *pBufferPointer, const WCHAR *pKeyName, const WCHAR *pKeyScope, WCHAR *OutValue)
{
	m_ppBufferPointer = &pBufferPointer;
	m_pKeyScope = const_cast<WCHAR*>(pKeyScope);
	WCHAR TempBuffer;
	int iCnt = 0;
	int i = 0;
	TempBuffer = '0';

	if (!FindWord(pKeyName))
		return false;
	if (!IsToken()) // 토큰이 나올 때 까지 검색
	{
		TempBuffer = **m_ppBufferPointer;
		*m_ppBufferPointer = *m_ppBufferPointer + 1;
	}
	else
	{
		TokenError(iCnt, pKeyName);
		return false;
	}

	*OutValue = TempBuffer;
	return true;
}

bool Parser::GetValue_String(WCHAR *pBufferPointer, const WCHAR *pKeyName, const WCHAR *pKeyScope, WCHAR *OutValue)
{
	m_ppBufferPointer = &pBufferPointer;
	m_pKeyScope = const_cast<WCHAR*>(pKeyScope);
	WCHAR TempBuffer[100];
	int iCnt = 0;
	int i = 0;
	memset(TempBuffer, 0, sizeof(TempBuffer));

	if (!FindWord(pKeyName))
		return false;
	if (**m_ppBufferPointer != '\"') // 전열이 '"'인지 검색
	{
		printf_s("문자열 오류 : 해당 문자열을 찾았으나, 전열의 기호의 표시가 잘못되었습니다.\n");
		return false;
	}
	*m_ppBufferPointer = *m_ppBufferPointer + 1;
	while (**m_ppBufferPointer != '\"') // 후열에서 '"'이 나올 때 까지 검색
	{
		TempBuffer[iCnt] = **m_ppBufferPointer;
		iCnt++;
		*m_ppBufferPointer = *m_ppBufferPointer + 1;
		if (**m_ppBufferPointer == '\0')
		{
			printf_s("문자열 오류 : 해당 문자열을 찾았으나, 후열의 기호의 표시가 잘못되었습니다.\n");
			return false;
		}
	}

	wcscpy_s(OutValue, sizeof(TempBuffer), TempBuffer);
	return true;
}
