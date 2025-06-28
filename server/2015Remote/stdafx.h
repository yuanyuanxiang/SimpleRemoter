
// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�

#pragma once

// �Ƿ�ʹ��LZ4
#define USING_LZ4 0
#if !USING_LZ4
#define USING_ZSTD 1
#define USING_CTX 0
#endif

#ifndef _SECURE_ATL
#define _SECURE_ATL 1
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // �� Windows ͷ���ų�����ʹ�õ�����
#endif

// �Ƴ��Ի�����MFC�ؼ���֧�֣���С��̬�������Ĵ�С
#define _AFX_NO_MFC_CONTROLS_IN_DIALOGS

// ����ڴ�й©���谲װVLD��������ע�ʹ���
// �����װ��VLD, �뽫��װ·����ӵ���������: ����Ϊ"VLDPATH", ·��Ϊ"D:\Program Files (x86)\Visual Leak Detector"
// �����ʵ�ʰ�װĿ¼��дVLDPATH. �����ֶ��༭ÿ����Ŀ�ļ���ͷ�ļ�Ŀ¼�Ϳ�Ŀ¼. �й�����VLD�����Ϣ��ο���������.
// VS2017��ǰ�汾��VLD: https://kinddragon.github.io/vld
// VS2019ʹ�õ�VLD��֧��������VS�汾, �Ƽ���: https://github.com/oneiric/vld/releases/tag/v2.7.0
// ���Ҫ���ܿض˳���ŵ�����������������, ��ʹ��Releaseģʽ���ɵĳ���, �Խ����VLD������; ��������Ҫ��VLD����ļ�һͬ����.
// ����VLD��ʾ����й¶������׷�ٲ��˺������ö�ջ�������������ʹ�÷��ŷ������Զ����ء������������Գ����ʱ�������
// ȷ����ĵ��Թ��ߣ��� Visual Studio �� WinDbg�������˷��ŷ�������
// ���ŷ��������Զ�����ȱʧ�ķ����ļ������� dbghelp.pdb�������仺�浽���ط���·����
// ���÷��ŷ��������� Visual Studio Ϊ�������� Visual Studio �У��� ���� > ѡ�� > ���š�
// ��ѡ Microsoft Symbol Servers. ָ�����Ż���Ŀ¼������ "C:\Symbols"��
// ����ʱ��ȱʧ�ķ��ţ��� dbghelp.pdb�����Զ����ص�����Ŀ¼��
#include "vld.h"

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // ĳЩ CString ���캯��������ʽ��

// �ر� MFC ��ĳЩ�����������ɷ��ĺ��Եľ�����Ϣ������
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC ��������ͱ�׼���
#include <afxext.h>         // MFC ��չ

#include <afxdisp.h>        // MFC �Զ�����

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // MFC �� Internet Explorer 4 �����ؼ���֧��
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // MFC �� Windows �����ؼ���֧��
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxcontrolbars.h>     // �������Ϳؼ����� MFC ֧��


#define WM_USERTOONLINELIST			   WM_USER + 3000
#define WM_OPENSCREENSPYDIALOG		   WM_USER + 3001
#define WM_OPENFILEMANAGERDIALOG       WM_USER + 3002
#define WM_OPENTALKDIALOG              WM_USER+3003
#define WM_OPENSHELLDIALOG             WM_USER+3004
#define WM_OPENSYSTEMDIALOG            WM_USER+3005
#define WM_OPENAUDIODIALOG             WM_USER+3006
#define WM_OPENSERVICESDIALOG          WM_USER+3007
#define WM_OPENREGISTERDIALOG          WM_USER+3008
#define WM_OPENWEBCAMDIALOG            WM_USER+3009

#define WM_USEROFFLINEMSG				WM_USER+3010
#define WM_HANDLEMESSAGE				WM_USER+3011
#define WM_OPENKEYBOARDDIALOG			WM_USER+3012
#define WM_UPXTASKRESULT			    WM_USER+3013
#define WM_OPENPROXYDIALOG				WM_USER+3014
#define WM_OPENHIDESCREENDLG			WM_USER+3015
#define WM_OPENMACHINEMGRDLG			WM_USER+3016
#define WM_OPENCHATDIALOG				WM_USER+3017
#define WM_OPENDECRYPTDIALOG			WM_USER+3018
#define WM_OPENFILEMGRDIALOG			WM_USER+3019

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif


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

#include "common/locker.h"
#include "common/logger.h"
#include "common/commands.h"

#define SAFE_CANCELIO(p) if (INVALID_SOCKET != (p)){ CancelIo((HANDLE)(p)); closesocket((SOCKET)(p)); (p) = INVALID_SOCKET; }
