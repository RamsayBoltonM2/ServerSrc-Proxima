#ifndef __INC_METIN_II_GAME_EXCHANGE_H__
#define __INC_METIN_II_GAME_EXCHANGE_H__

class CGrid;

enum EExchangeValues
{
#ifdef WJ_NEW_EXCHANGE_WINDOW
	EXCHANGE_ITEM_MAX_NUM = 24,
#else
	EXCHANGE_ITEM_MAX_NUM = 12,
#endif
	EXCHANGE_MAX_DISTANCE	= 1000
};

class CExchange
{
	public:
		CExchange(LPCHARACTER pOwner);
		~CExchange();

#ifdef WJ_NEW_EXCHANGE_WINDOW
		void		Accept(bool bIsAccept = true);
#else
		bool		Accept(bool bIsAccept = true);	
#endif
		void		Cancel();

		bool		AddGold(long lGold);
#ifdef WJ_CHEQUE_SYSTEM
		bool		AddCheque(long lCheque);
#endif
		bool		AddItem(TItemPos item_pos, BYTE display_pos);
		bool		RemoveItem(BYTE pos);

		LPCHARACTER	GetOwner()	{ return m_pOwner;	}
		CExchange *	GetCompany()	{ return m_pCompany;	}

		bool		GetAcceptStatus() { return m_bAccept; }

		void		SetCompany(CExchange * pExchange)	{ m_pCompany = pExchange; }
		
#ifdef WJ_NEW_EXCHANGE_WINDOW
		void		SendInfo(bool isError, const char * format, ...);

		int			CountExchangingItems();

		//Getters
		int64_t		GetExchangingGold() const { return m_lGold; }
#ifdef WJ_CHEQUE_SYSTEM
		int64_t		GetExchangingCheque() const { return m_lCheque; }
#endif
		int			GetLastCriticalUpdatePulse() const { return m_lLastCriticalUpdatePulse; };
		LPCHARACTER	GetOwner() const { return m_pOwner; }
		CExchange *	GetCompany() const { return m_pCompany; }
		bool		GetAcceptStatus() const { return m_bAccept; }
		LPITEM		GetItemByPosition(int i) const { return m_apItems[i]; }
		int			GetPositionByItem(LPITEM item) const;
#endif

	private:
#ifdef WJ_NEW_EXCHANGE_WINDOW
		bool		Done(DWORD tradeID, bool firstPlayer);
		bool		SanityCheck();
		bool		PerformTrade();
#else
		bool		Done();
		bool		Check(int * piItemCount);,
#endif
		bool		CheckSpace();

	private:
		CExchange *	m_pCompany;	// 상대방의 CExchange 포인터

		LPCHARACTER	m_pOwner;

		TItemPos		m_aItemPos[EXCHANGE_ITEM_MAX_NUM];
		LPITEM		m_apItems[EXCHANGE_ITEM_MAX_NUM];
		BYTE		m_abItemDisplayPos[EXCHANGE_ITEM_MAX_NUM];

		bool 		m_bAccept;
		long		m_lGold;
#ifdef WJ_CHEQUE_SYSTEM
		long		m_lCheque;
#endif

#ifdef WJ_NEW_EXCHANGE_WINDOW
		int			m_lLastCriticalUpdatePulse;
#endif

		CGrid *		m_pGrid;

};

#endif
