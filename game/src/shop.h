#ifndef __INC_METIN_II_GAME_SHOP_H__
#define __INC_METIN_II_GAME_SHOP_H__

enum
{
	SHOP_MAX_DISTANCE = 1000
};

class CGrid;

/* ---------------------------------------------------------------------------------- */
class CShop
{
	public:
		typedef struct shop_item
		{
			DWORD	vnum;		// ������ ��ȣ
			long	price;		// ����
#if defined(WJ_CHEQUE_SYSTEM)
			long	price2;
#endif
			BYTE	count;		// ������ ����

			LPITEM	pkItem;
			int		itemid;		// ������ �������̵�
#ifdef WJ_SHOPSOLD_SYSTEM
			BYTE	status;
			char	szBuyerName[CHARACTER_NAME_MAX_LEN + 1];
#endif

#ifdef WJ_EXTENDED_SHOP_SYSTEM
			long	alSockets[ITEM_SOCKET_MAX_NUM];
			TPlayerItemAttribute	aAttr[ITEM_ATTRIBUTE_MAX_NUM];
#endif

			shop_item()
			{
				vnum = 0;
				price = 0;
#if defined(WJ_CHEQUE_SYSTEM)
				price2 = 0;
#endif
				count = 0;
				itemid = 0;
				pkItem = NULL;
#ifdef WJ_SHOPSOLD_SYSTEM
				status = 0;
				szBuyerName[CHARACTER_NAME_MAX_LEN + 1] = 0;
#endif
#ifdef WJ_EXTENDED_SHOP_SYSTEM
				memset(alSockets, 0, sizeof(alSockets));
				memset(aAttr, 0, sizeof(aAttr));
#endif				
			}
		} SHOP_ITEM;

		CShop();
		virtual ~CShop(); // @fixme139 (+virtual)

#ifdef WJ_EXTENDED_SHOP_SYSTEM
		bool	Create(DWORD dwVnum, DWORD dwNPCVnum, TShopItemTable * pItemTable, short price_type, std::string shopname, DWORD dwSize);
#else
		bool	Create(DWORD dwVnum, DWORD dwNPCVnum, TShopItemTable * pItemTable);
#endif
		void	SetShopItems(TShopItemTable * pItemTable, BYTE bItemCount);

		virtual void	SetPCShop(LPCHARACTER ch);
		virtual bool	IsPCShop()	{ return m_pkPC ? true : false; }

		// �Խ�Ʈ �߰�/����
		virtual bool	AddGuest(LPCHARACTER ch,DWORD owner_vid, bool bOtherEmpire);
		void	RemoveGuest(LPCHARACTER ch);
#ifdef ENABLE_EXTENDED_RELOAD_COMMANDS
		void	RemoveAllGuests();
#endif
		// ���� ����
		virtual int	Buy(LPCHARACTER ch, BYTE pos);

		// �Խ�Ʈ���� ��Ŷ�� ����
		void	BroadcastUpdateItem(BYTE pos);

		// �Ǹ����� �������� ������ �˷��ش�.
		int		GetNumberByVnum(DWORD dwVnum);

		// �������� ������ ��ϵǾ� �ִ��� �˷��ش�.
		virtual bool	IsSellingItem(DWORD itemID);

		DWORD	GetVnum() { return m_dwVnum; }
		DWORD	GetNPCVnum() { return m_dwNPCVnum; }

	protected:
		void	Broadcast(const void * data, int bytes);

	protected:
		DWORD				m_dwVnum;
		DWORD				m_dwNPCVnum;
#ifdef WJ_EXTENDED_SHOP_SYSTEM
		short				m_sPriceType;
		std::string			m_szShopName;
		BYTE				m_bSize;
#endif

		CGrid *				m_pGrid;

		typedef TR1_NS::unordered_map<LPCHARACTER, bool> GuestMapType;
		GuestMapType m_map_guest;
		std::vector<SHOP_ITEM>		m_itemVector;	// �� �������� ����ϴ� ���ǵ�

		LPCHARACTER			m_pkPC;
};

#endif 
