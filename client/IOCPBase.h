#include "common/commands.h"

class IOCPBase
{
public:
    virtual BOOL IsRunning() const = 0;
    virtual VOID RunEventLoop(const BOOL& bCondition) = 0;
	virtual CONNECT_ADDRESS* GetConnectionAddress() const = 0;
};

typedef BOOL(*TrailCheck)(void);
