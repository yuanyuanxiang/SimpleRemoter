/************************************************************************
* @file gh0st2Remote.h
* @brief 将FileManagerDlg、InputDlg、FileTransferModeDlg、TrueColorToolBar
*	还原到gh0st最初版本。
************************************************************************/

#pragma once

#include "IOCPServer.h"

#define CIOCPServer IOCPServer

#define ClientContext CONTEXT_OBJECT

typedef struct 
{
	DWORD	dwSizeHigh;
	DWORD	dwSizeLow;
}FILESIZE;

#define m_DeCompressionBuffer InDeCompressedBuffer

#define GetBufferLen GetBufferLength

#define m_Dialog hDlg

#define m_Socket sClientSocket

#define Send OnClientPreSending

#define MAKEINT64(low, high) ((unsigned __int64)(((DWORD)(low)) | ((unsigned __int64)((DWORD)(high))) << 32))

#define	MAX_WRITE_RETRY			15 // 重试写入文件次数
#define	MAX_SEND_BUFFER			1024 * 8 // 最大发送数据长度
#define MAX_RECV_BUFFER			1024 * 8 // 最大接收数据长度
