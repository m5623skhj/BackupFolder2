#pragma once
#include <Windows.h>
#include <locale.h>
// 	_wsetlocale(LC_ALL, L"Korean");

#define BUFFER_MAX 1024 * 10 

class Parser
{
private :
	WCHAR **m_ppBufferPointer;
	WCHAR *m_pKeyScope;

	// 다음 문자 + 다음 토큰까지를 검사함
	//bool GetNextWord(char **pBufferPointer, const char *pKeyName);
	bool GetNextWord(const WCHAR *pKeyName, bool *bIsStop);

	// 토큰 중 하나를 만나게 되면 멈추고, 
	// 토큰이 아니면 계속 포인터를 옮김
	void SkipToToken();

	// 주석 처리된 부분이 끝날 때 까지 포인터를 옮김
	void JumpComment();

	// 인자로 전달된 스코프를 찾을 때 까지 검색함
	bool JumpScope(bool *bIsStop);

	// 해당 문자를 찾는 반복문
	bool FindWord(const WCHAR *pKeyName);

	// 문자열을 대조해서 같으면 true 틀리면 false를 반환함 
	bool GetStringWord(const WCHAR *pKeyName);

	// 해당 문자가 토큰인지를 확인하여 토큰이면 true, 토큰이 아니면 false를 반환함
	bool IsToken();

	// 문자열을 찾았으나 해당 문자열 앞에 추가적인 토큰이 존재함
	void TokenError(int iCnt, const WCHAR *pKeyName);

	// 스코프에 해당 문자열이 존재하지 않음
	void KeyError(const WCHAR *pKeyName);

	// 해당 스코프가 존재하지 않음
	void ScopeError();
public :
	Parser();
	~Parser();

	bool GetValue_Int(WCHAR *pBufferPointer, const WCHAR *KeyName, const WCHAR *KeyScope, int *OutValue);
	bool GetValue_Double(WCHAR *pBufferPointer, const WCHAR *KeyName, const WCHAR *KeyScope, double *OutValue);
	bool GetValue_Char(WCHAR *pBufferPointer, const WCHAR *KeyName, const WCHAR *KeyScope, WCHAR *OutValue);
	bool GetValue_String(WCHAR *pBufferPointer, const WCHAR *KeyName, const WCHAR *KeyScope, WCHAR *OutValue);
};