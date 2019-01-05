#pragma once

#include <pdh.h>
#include <pdhmsg.h>

#pragma comment(lib,"PDH.lib")

#define MAX_RAW_VALUES          20

//process 项目下的% Processor Time计数器是关于进程CPU使率的
const char szCounterName[] = "\\Processor(_Total)\\% Processor Time";

typedef struct _tag_PDHCounterStruct {
	HCOUNTER hCounter;   
	int nNextIndex;         
	int nOldestIndex;     
	int nRawCount;          
	PDH_RAW_COUNTER a_RawValue[MAX_RAW_VALUES]; 
} PDHCOUNTERSTRUCT, *PPDHCOUNTERSTRUCT;


class CCpuUsage  
{
public:
	CCpuUsage();
	virtual ~CCpuUsage();
	BOOL	Init();
	int     GetUsage();

protected:

	PPDHCOUNTERSTRUCT			m_pCounterStruct;
	HQUERY						m_hQuery;
};
