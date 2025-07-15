// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__46CA6496_AAD6_4658_B6E9_D7AEB26CDCD5__INCLUDED_)
#define AFX_STDAFX_H__46CA6496_AAD6_4658_B6E9_D7AEB26CDCD5__INCLUDED_

// �Ƿ�ʹ��ZLIB
#define USING_ZLIB 0

#if !USING_ZLIB
// �Ƿ�ʹ��LZ4
#define USING_LZ4 0

#if !USING_LZ4
#define USING_ZSTD 1
#define USING_CTX 1
#endif

#endif

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef _DEBUG
// ����ڴ�й©���谲װVLD��������ע�ʹ���
#include "vld.h"
#define USING_SAFETHRED 0
#else
#define USING_SAFETHRED 1
#endif

// Insert your headers here
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>

// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__46CA6496_AAD6_4658_B6E9_D7AEB26CDCD5__INCLUDED_)

#include <assert.h>
#include <MMSystem.h>
#pragma comment(lib, "winmm.lib")

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) if(NULL !=(p)){ delete (p);(p) = NULL;}
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) if(NULL !=(p)){ delete[] (p);(p) = NULL;}
#endif

#ifndef SAFE_DELETE_AR
#define SAFE_DELETE_AR(p) if(NULL !=(p)){ delete[] (p);(p) = NULL;}
#endif

#include "common/logger.h"
#include "common/locker.h"
