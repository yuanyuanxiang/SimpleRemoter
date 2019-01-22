#include "stdafx.h"
#include "CpuUseage.h"

//系统性能计数器
CCpuUsage::CCpuUsage()
{
	m_hQuery = NULL;
	m_pCounterStruct = NULL;

}

CCpuUsage::~CCpuUsage()
{
	PdhCloseQuery(m_hQuery);   //关闭计数器
	if (m_pCounterStruct)
		delete m_pCounterStruct;
}


BOOL CCpuUsage::Init()
{
	if (ERROR_SUCCESS != PdhOpenQuery(NULL, 1, &m_hQuery))   //打开计数器
		return FALSE;

	m_pCounterStruct = (PPDHCOUNTERSTRUCT) new PDHCOUNTERSTRUCT;

	//统计感兴趣的系统信息时，必须先将对应的计数器添加进来
	PDH_STATUS pdh_status = PdhAddCounter(m_hQuery, (LPCSTR)szCounterName, 
		(DWORD) m_pCounterStruct, &(m_pCounterStruct->hCounter));
	if (ERROR_SUCCESS != pdh_status) 
	{
		return FALSE;
	}

	return TRUE;
}


int CCpuUsage::GetUsage()
{
	PDH_FMT_COUNTERVALUE pdhFormattedValue;

	PdhCollectQueryData(m_hQuery);

	//得到数据获得当前计数器的值
	if (ERROR_SUCCESS != PdhGetFormattedCounterValue(m_pCounterStruct->hCounter,
		PDH_FMT_LONG,
		NULL,
		&pdhFormattedValue )) 
	{
		return 0;
	}

	return pdhFormattedValue.longValue;
}
