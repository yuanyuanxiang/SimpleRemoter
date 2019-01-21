// RegisterOperation.cpp: implementation of the RegisterOperation class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "RegisterOperation.h"
#include "Common.h"
#include <IOSTREAM>
using namespace std;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

enum MYKEY{
	MHKEY_CLASSES_ROOT,
	MHKEY_CURRENT_USER,
	MHKEY_LOCAL_MACHINE,
	MHKEY_USERS,
	MHKEY_CURRENT_CONFIG
};


enum KEYVALUE{
	MREG_SZ,
	MREG_DWORD,
	MREG_BINARY,
	MREG_EXPAND_SZ
};

struct REGMSG{
	int count;         //名字个数
	DWORD size;             //名字大小
	DWORD valsize;     //值大小

};
RegisterOperation::RegisterOperation(char bToken)
{
	switch(bToken){
	case MHKEY_CLASSES_ROOT:
		MKEY=HKEY_CLASSES_ROOT;
		break;
	case MHKEY_CURRENT_USER:
		MKEY=HKEY_CURRENT_USER;
		break;
	case MHKEY_LOCAL_MACHINE:
		MKEY=HKEY_LOCAL_MACHINE;
		break;
	case MHKEY_USERS:
		MKEY=HKEY_USERS;
		break;
	case MHKEY_CURRENT_CONFIG:
		MKEY=HKEY_CURRENT_CONFIG;
		break;
	default:
		MKEY=HKEY_LOCAL_MACHINE;
		break;
	}

	ZeroMemory(KeyPath,MAX_PATH);
}

RegisterOperation::~RegisterOperation()
{

}


char* RegisterOperation::FindPath()
{
	char *szBuffer=NULL;
	HKEY		hKey;			//注册表返回句柄
	/*打开注册表 User  kdjfjkf\kdjfkdjf\  */
	if(RegOpenKeyEx(MKEY,KeyPath,0,KEY_ALL_ACCESS,&hKey)==ERROR_SUCCESS)//打开
	{
		DWORD dwIndex=0,NameCount,NameMaxLen;
		DWORD KeySize,KeyCount,KeyMaxLen,MaxDateLen;
		//这就是枚举了
		if(RegQueryInfoKey(hKey,NULL,NULL,NULL,&KeyCount,  //14
			&KeyMaxLen,NULL,&NameCount,&NameMaxLen,&MaxDateLen,NULL,NULL)!=ERROR_SUCCESS)
		{  
			return NULL;
		}
		//一点保护措施
		KeySize=KeyMaxLen+1;
		if(KeyCount>0&&KeySize>1){
			int Size=sizeof(REGMSG)+1;

			DWORD DataSize=KeyCount*KeySize+Size+1;    //[TOKEN_REG_PATH][2 11 ccccc\0][11][11]
			szBuffer=(char*)LocalAlloc(LPTR, DataSize);
			ZeroMemory(szBuffer,DataSize);
			szBuffer[0]=TOKEN_REG_PATH;           //命令头
			REGMSG msg;                     //数据头
			msg.size=KeySize;
			msg.count=KeyCount;
			memcpy(szBuffer+1,(void*)&msg,Size);

			char * szTemp=new char[KeySize];
			for(dwIndex=0;dwIndex<KeyCount;dwIndex++)		//枚举项
			{
				ZeroMemory(szTemp,KeySize);
				DWORD i=KeySize;  //21
				RegEnumKeyEx(hKey,dwIndex,szTemp,&i,NULL,NULL,NULL,NULL);
				strcpy(szBuffer+dwIndex*KeySize+Size,szTemp);
			}
			delete[] szTemp;
			RegCloseKey(hKey);
		}
	}

	return szBuffer;
}


void RegisterOperation::SetPath(char *szPath)
{
	ZeroMemory(KeyPath,MAX_PATH);
	strcpy(KeyPath,szPath);
}

char* RegisterOperation::FindKey()
{
	char	*szValueName;		//键值名称
	LPBYTE	 szValueDate;		//键值数据

	char *szBuffer=NULL;
	HKEY  hKey;			//注册表返回句柄
	if(RegOpenKeyEx(MKEY,KeyPath,0,KEY_ALL_ACCESS,&hKey)==ERROR_SUCCESS)//打开
	{
		DWORD dwIndex=0,NameSize,NameCount,NameMaxLen,Type;
		DWORD KeyCount,KeyMaxLen,DataSize,MaxDateLen;
		//这就是枚举了
		if(RegQueryInfoKey(hKey,NULL,NULL,NULL,
			&KeyCount,&KeyMaxLen,NULL,&NameCount,&NameMaxLen,&MaxDateLen,NULL,NULL)!=ERROR_SUCCESS)
		{
			return NULL;
		}
		if(NameCount>0&&MaxDateLen>0)
		{
			DataSize=MaxDateLen+1;
			NameSize=NameMaxLen+100;
			REGMSG  msg;
			msg.count=NameCount;        //总个数
			msg.size=NameSize;          //名字大小
			msg.valsize=DataSize;       //数据大小
			int msgsize=sizeof(REGMSG);
			// 头                   标记            名字                数据
			DWORD size=sizeof(REGMSG)+ 
				sizeof(BYTE)*NameCount+ NameSize*NameCount+DataSize*NameCount+10;
			szBuffer = (char*)LocalAlloc(LPTR, size);
			ZeroMemory(szBuffer,size);
			szBuffer[0]=TOKEN_REG_KEY;         //命令头
			memcpy(szBuffer+1,(void*)&msg,msgsize);     //数据头

			szValueName=(char *)malloc(NameSize);
			szValueDate=(LPBYTE)malloc(DataSize);

			char *szTemp=szBuffer+msgsize+1;
			for(dwIndex=0;dwIndex<NameCount;dwIndex++)	//枚举键值
			{
				ZeroMemory(szValueName,NameSize);
				ZeroMemory(szValueDate,DataSize);

				DataSize=MaxDateLen+1;
				NameSize=NameMaxLen+100;

				RegEnumValue(hKey,dwIndex,szValueName,&NameSize,
					NULL,&Type,szValueDate,&DataSize);//读取键值

				if(Type==REG_SZ)
				{
					szTemp[0]=MREG_SZ;  
				}
				if(Type==REG_DWORD)
				{
					szTemp[0]=MREG_DWORD;  
				}
				if(Type==REG_BINARY)
				{
					szTemp[0]=MREG_BINARY;
				}
				if(Type==REG_EXPAND_SZ)
				{
					szTemp[0]=MREG_EXPAND_SZ;
				}
				szTemp+=sizeof(BYTE);
				strcpy(szTemp,szValueName);
				szTemp+=msg.size;
				memcpy(szTemp,szValueDate,msg.valsize);
				szTemp+=msg.valsize;
			}
			free(szValueName);
			free(szValueDate);
		}   
	}
	return szBuffer;
}
