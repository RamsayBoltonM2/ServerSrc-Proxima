#include "stdafx.h"
#ifdef WJ_CHEQUE_SYSTEM
#include "ChequeLog.h"
#include "ClientManager.h"
#include "Peer.h"

CChequeLog::CChequeLog()
{
}

CChequeLog::~CChequeLog()
{
}

void CChequeLog::Save()
{
	CPeer* peer = CClientManager::instance().GetAnyPeer();
	if (!peer)
		return;
	for (BYTE bType = 0; bType < CHEQUE_LOG_TYPE_MAX_NUM; bType ++)
	{
		decltype(m_ChequeLogContainer[bType].begin()) it;
		for (it = m_ChequeLogContainer[bType].begin(); it != m_ChequeLogContainer[bType].end(); ++it)
		{
			//bType;
			TPacketChequeLog p;
			p.type = bType;
			p.vnum = it->first;
			p.cheque = it->second;
			peer->EncodeHeader(HEADER_DG_MONEY_LOG, 0, sizeof(p));
			peer->Encode(&p, sizeof(p));
		}
		m_ChequeLogContainer[bType].clear();
	}
}

void CChequeLog::AddLog(BYTE bType, DWORD dwVnum, int iCheque)
{
	m_ChequeLogContainer[bType][dwVnum] += iCheque;
}
#endif