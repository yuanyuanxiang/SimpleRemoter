// RegisterOperation.cpp: implementation of the RegisterOperation class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "RegisterOperation.h"
#include "Common.h"
#include <IOSTREAM>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

enum MYKEY {
    MHKEY_CLASSES_ROOT,
    MHKEY_CURRENT_USER,
    MHKEY_LOCAL_MACHINE,
    MHKEY_USERS,
    MHKEY_CURRENT_CONFIG
};


enum KEYVALUE {
    MREG_SZ,
    MREG_DWORD,
    MREG_BINARY,
    MREG_EXPAND_SZ
};

struct REGMSG {
    int count;         //���ָ���
    DWORD size;             //���ִ�С
    DWORD valsize;     //ֵ��С

};
RegisterOperation::RegisterOperation(char bToken)
{
    switch(bToken) {
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
    HKEY		hKey;			//ע����ؾ��
    /*��ע��� User  kdjfjkf\kdjfkdjf\  */
    if(RegOpenKeyEx(MKEY,KeyPath,0,KEY_ALL_ACCESS,&hKey)==ERROR_SUCCESS) { //��
        DWORD dwIndex=0,NameCount,NameMaxLen;
        DWORD KeySize,KeyCount,KeyMaxLen,MaxDateLen;
        //�����ö����
        if(RegQueryInfoKey(hKey,NULL,NULL,NULL,&KeyCount,  //14
                           &KeyMaxLen,NULL,&NameCount,&NameMaxLen,&MaxDateLen,NULL,NULL)!=ERROR_SUCCESS) {
            return NULL;
        }
        //һ�㱣����ʩ
        KeySize=KeyMaxLen+1;
        if(KeyCount>0&&KeySize>1) {
            int Size=sizeof(REGMSG)+1;

            DWORD DataSize=KeyCount*KeySize+Size+1;    //[TOKEN_REG_PATH][2 11 ccccc\0][11][11]
            szBuffer=(char*)LocalAlloc(LPTR, DataSize);
            if (szBuffer == NULL) {
                return NULL;
            }
            ZeroMemory(szBuffer,DataSize);
            szBuffer[0]=TOKEN_REG_PATH;           //����ͷ
            REGMSG msg;                     //����ͷ
            msg.size=KeySize;
            msg.count=KeyCount;
            memcpy(szBuffer+1,(void*)&msg,Size);

            char * szTemp=new char[KeySize];
            for(dwIndex=0; dwIndex<KeyCount; dwIndex++) {	//ö����
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
    char	*szValueName;		//��ֵ����
    LPBYTE	 szValueDate;		//��ֵ����

    char *szBuffer=NULL;
    HKEY  hKey;			//ע����ؾ��
    if(RegOpenKeyEx(MKEY,KeyPath,0,KEY_ALL_ACCESS,&hKey)==ERROR_SUCCESS) { //��
        DWORD dwIndex=0,NameSize,NameCount,NameMaxLen,Type;
        DWORD KeyCount,KeyMaxLen,DataSize,MaxDateLen;
        //�����ö����
        if(RegQueryInfoKey(hKey,NULL,NULL,NULL,
                           &KeyCount,&KeyMaxLen,NULL,&NameCount,&NameMaxLen,&MaxDateLen,NULL,NULL)!=ERROR_SUCCESS) {
            return NULL;
        }
        if(NameCount>0&&MaxDateLen>0) {
            DataSize=MaxDateLen+1;
            NameSize=NameMaxLen+100;
            REGMSG  msg;
            msg.count=NameCount;        //�ܸ���
            msg.size=NameSize;          //���ִ�С
            msg.valsize=DataSize;       //���ݴ�С
            int msgsize=sizeof(REGMSG);
            // ͷ                   ���            ����                ����
            DWORD size=sizeof(REGMSG)+
                       sizeof(BYTE)*NameCount+ NameSize*NameCount+DataSize*NameCount+10;
            szBuffer = (char*)LocalAlloc(LPTR, size);
            if (szBuffer==NULL) {
                return NULL;
            }
            ZeroMemory(szBuffer,size);
            szBuffer[0]=TOKEN_REG_KEY;         //����ͷ
            memcpy(szBuffer+1,(void*)&msg,msgsize);     //����ͷ

            szValueName=(char *)malloc(NameSize);
            szValueDate=(LPBYTE)malloc(DataSize);
            if (szValueName==NULL||szValueDate == NULL) {
                return NULL;
            }
            char *szTemp=szBuffer+msgsize+1;
            for(dwIndex=0; dwIndex<NameCount; dwIndex++) {	//ö�ټ�ֵ
                ZeroMemory(szValueName,NameSize);
                ZeroMemory(szValueDate,DataSize);

                DataSize=MaxDateLen+1;
                NameSize=NameMaxLen+100;

                RegEnumValue(hKey,dwIndex,szValueName,&NameSize,
                             NULL,&Type,szValueDate,&DataSize);//��ȡ��ֵ

                if(Type==REG_SZ) {
                    szTemp[0]=MREG_SZ;
                }
                if(Type==REG_DWORD) {
                    szTemp[0]=MREG_DWORD;
                }
                if(Type==REG_BINARY) {
                    szTemp[0]=MREG_BINARY;
                }
                if(Type==REG_EXPAND_SZ) {
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
