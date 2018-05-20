// vim: ts=8 sw=4
#ifdef WJ_CHEQUE_SYSTEM
#ifndef __INC_CHEQUE_LOG
#define __INC_CHEQUE_LOG

#include <map>

class CChequeLog : public singleton<CChequeLog>
{
    public:
	CChequeLog();
	virtual ~CChequeLog();

	void Save();
	void AddLog(BYTE bType, DWORD dwVnum, int iCheque);

    private:
	std::map<DWORD, int> m_ChequeLogContainer[CHEQUE_LOG_TYPE_MAX_NUM];
};

#endif
#endif