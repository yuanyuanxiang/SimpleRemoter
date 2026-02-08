#pragma once


//////////////////////////////////////////////////////////////////////////
#include <unordered_map>
#include <fstream>
#include <set>
#include "context.h"

enum {
    MAP_NOTE,
    MAP_LOCATION,
    MAP_LEVEL,
    MAP_AUTH,
};

#pragma pack(push, 1)
class _ClientValue
{
public:
    char Note[64];
    char Location[64];
    char Level;
    char IP[46];
    char InstallTime[20];
    char LastLoginTime[20];
    char OsName[32];
    char Authorized;
    uint64_t ID;
    char ComputerName[64];
    char ProgramPath[256];
    char Reserved[448];

    _ClientValue()
    {
        memset(this, 0, sizeof(_ClientValue));
    }
};
#pragma pack(pop)

typedef uint64_t ClientKey;

typedef _ClientValue ClientValue;

typedef std::unordered_map<ClientKey, ClientValue> ClientMap;

class _ClientList
{
public:
    virtual ~_ClientList() {}
    virtual bool Exists(ClientKey key) = 0;
    virtual CString GetClientMapData(ClientKey key, int typ) = 0;
    virtual int GetClientMapInteger(ClientKey key, int typ) = 0;
    virtual void SetClientMapInteger(ClientKey key, int typ, int value) = 0;
    virtual void SetClientMapData(ClientKey key, int typ, const char* value) = 0;
    virtual void SaveClientMapData(context* ctx) = 0;
    virtual std::vector<std::pair<ClientKey, ClientValue>> GetAll() = 0;
    virtual void SaveToFile(const std::string& filename) = 0;
    virtual void LoadFromFile(const std::string& filename) = 0;
};

_ClientList* NewClientList();
