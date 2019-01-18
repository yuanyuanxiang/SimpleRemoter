// RegisterOperation.h: interface for the RegisterOperation class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REGISTEROPERATION_H__BB4F3ED1_FA98_4BA4_97D6_A78E683131CC__INCLUDED_)
#define AFX_REGISTEROPERATION_H__BB4F3ED1_FA98_4BA4_97D6_A78E683131CC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class RegisterOperation  
{
public:
	RegisterOperation(char bToken);
	virtual ~RegisterOperation();
	char* FindPath();
	HKEY MKEY;
	char KeyPath[MAX_PATH];
	void SetPath(char *szPath);
	char* FindKey();
};

#endif // !defined(AFX_REGISTEROPERATION_H__BB4F3ED1_FA98_4BA4_97D6_A78E683131CC__INCLUDED_)
