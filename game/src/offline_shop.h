#pragma once

enum
{
	OFFLINE_SHOP_MAX_DISTANCE = 1500,
};

class COfflineShop
{
	public:
		typedef struct offline_shop_item
		{
			DWORD	id;
			DWORD	owner_id;
			BYTE	pos;
			BYTE	count;			
			DWORD		price;
#ifdef WJ_CHEQUE_SYSTEM
			DWORD		price2;
#endif
			DWORD		vnum;	
			long	alSockets[ITEM_SOCKET_MAX_NUM];
			TPlayerItemAttribute	aAttr[ITEM_ATTRIBUTE_MAX_NUM];
			BYTE	status;
			char szBuyerName[CHARACTER_NAME_MAX_LEN + 1];
#ifdef WJ_CHANGELOOK_SYSTEM
			DWORD	transmutation;
#endif	
			char szName[ITEM_NAME_MAX_LEN + 1];
			BYTE	refine_level;
			DWORD	shop_id;
			offline_shop_item()
			{
				id = 0;
				owner_id = 0;
				pos = 0;
				count = 0;
				price = 0;
#ifdef WJ_CHEQUE_SYSTEM
				price2 = 0;
#endif
				vnum = 0;
				memset(alSockets, 0, sizeof(alSockets));
				memset(aAttr, 0, sizeof(aAttr));		
				status = 0;
				szBuyerName[CHARACTER_NAME_MAX_LEN + 1] = 0;
#ifdef WJ_CHANGELOOK_SYSTEM
				transmutation = 0;
#endif
				szName[ITEM_NAME_MAX_LEN + 1] = 0;
				refine_level = 0;
				shop_id = 0;
			}
		} OFFLINE_SHOP_ITEM;
	
		COfflineShop();
		~COfflineShop();

		virtual void	SetOfflineShopNPC(LPCHARACTER npc);
		virtual bool	IsOfflineShopNPC(){ return m_pkOfflineShopNPC ? true : false; }
		LPCHARACTER		GetOfflineShopNPC() { return m_pkOfflineShopNPC; }
	
		void			CreateTable(DWORD dwOwnerPID);
		void			SetOfflineShopItems(DWORD dwOwnerPID, TOfflineShopItemTable * pTable, BYTE bItemCount);
		void			AddItem(LPCHARACTER ch, LPITEM pkItem, BYTE bPos, int iPrice, int iPrice2);
		void			RemoveItem(LPCHARACTER ch, BYTE bPos);
		
		virtual bool	AddGuest(LPCHARACTER ch, LPCHARACTER npc);
		
		void			RemoveGuest(LPCHARACTER ch);
		void			RemoveAllGuest();
		void			Destroy(LPCHARACTER npc);

		virtual int		Buy(LPCHARACTER ch, BYTE bPos);
		
		void			BroadcastUpdateItem(BYTE bPos, DWORD dwPID, bool bDestroy = false);
		void			BroadcastUpdatePrice(BYTE bPos, DWORD dwPrice);
		void			Refresh(LPCHARACTER ch);

		bool			RemoveItem(DWORD dwVID, BYTE bPos);
		BYTE			GetLeftItemCount();
		void			SetOfflineShopGold(long long val) { llMoney = val; }
		long long		GetOfflineShopGold() const { return llMoney; }
#ifdef WJ_CHEQUE_SYSTEM
		void			SetOfflineShopCheque(DWORD val) { dwCheque = val; }
		DWORD			GetOfflineShopCheque() const { return dwCheque; }
#endif
		void			SetOfflineShopMapIndex(long idx) { m_llMapIndex = idx; }
		long			GetOfflineShopMapIndex() const { return m_llMapIndex; }
		void			SetOfflineShopTime(int iTime) { m_iTime = iTime; }
		int				GetOfflineShopTime() const { return m_iTime; }
		void			SetOfflineShopBankValues(DWORD dwOwnerPID);
		
		std::string shopSign;
		const char *		GetShopSign() { return shopSign.c_str(); };
		void				SetShopSign(const char * c) { shopSign = c; };
		
		std::vector<OFFLINE_SHOP_ITEM>	GetItemVector() { return m_itemVector; }
		
		void				SetGuestMap(LPCHARACTER ch);
		void				RemoveGuestMap(LPCHARACTER ch);
		
	protected:
		void			Broadcast(const void * data, int bytes);

	private:
		// Grid
		CGrid *				m_pGrid;
		long long			llMoney;
#ifdef WJ_CHEQUE_SYSTEM
		DWORD				dwCheque;
#endif
		long				m_llMapIndex;
		int					m_iTime;

		// Guest Map
		typedef TR1_NS::unordered_map<LPCHARACTER, bool> GuestMapType;
		GuestMapType m_map_guest;
		// End Of Guest Map
		
		std::vector<OFFLINE_SHOP_ITEM>		m_itemVector;

		LPCHARACTER m_pkOfflineShopNPC;
		DWORD	m_dwDisplayedCount;
		std::vector<DWORD> m_Displayers;
};

