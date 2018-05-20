#include "stdafx.h"
#include "../../libgame/include/grid.h"
#include "utils.h"
#include "desc.h"
#include "desc_client.h"
#include "char.h"
#include "item.h"
#include "item_manager.h"
#include "packet.h"
#include "log.h"
#include "db.h"
#include "locale_service.h"
#include "../../common/length.h"
#include "exchange.h"
#include "DragonSoul.h"
#ifdef WJ_GROWTH_PET_SYSTEM
#include "New_PetSystem.h"
#endif
#ifdef WJ_NEW_EXCHANGE_WINDOW
#include "buffer_manager.h"
#include <ctime>
#include "config.h"
#endif

#ifdef WJ_NEW_EXCHANGE_WINDOW
extern int passes_per_sec;
#endif

void exchange_packet(LPCHARACTER ch, BYTE sub_header, bool is_me, long long arg1, TItemPos arg2, DWORD arg3, void * pvData = NULL);

// 교환 패킷
void exchange_packet(LPCHARACTER ch, BYTE sub_header, bool is_me, long long arg1, TItemPos arg2, DWORD arg3, void * pvData)
{
	if (!ch->GetDesc())
		return;

	struct packet_exchange pack_exchg;
	pack_exchg.header 		= HEADER_GC_EXCHANGE;
	pack_exchg.sub_header 	= sub_header;
	pack_exchg.is_me		= is_me;
	pack_exchg.arg1			= arg1;
	pack_exchg.arg2			= arg2;
	pack_exchg.arg3			= arg3;

	if (sub_header == EXCHANGE_SUBHEADER_GC_ITEM_ADD && pvData)
	{
#ifdef WJ_ENABLE_TRADABLE_ICON
		pack_exchg.arg4 = TItemPos(((LPITEM) pvData)->GetWindow(), ((LPITEM) pvData)->GetCell());
#endif
		thecore_memcpy(&pack_exchg.alSockets, ((LPITEM) pvData)->GetSockets(), sizeof(pack_exchg.alSockets));
		thecore_memcpy(&pack_exchg.aAttr, ((LPITEM) pvData)->GetAttributes(), sizeof(pack_exchg.aAttr));
#ifdef WJ_CHANGELOOK_SYSTEM
		pack_exchg.dwTransmutation = ((LPITEM) pvData)->GetTransmutation();
#endif
	}
	else
	{
#ifdef WJ_ENABLE_TRADABLE_ICON
		pack_exchg.arg4 = TItemPos(RESERVED_WINDOW, 0);
#endif
		memset(&pack_exchg.alSockets, 0, sizeof(pack_exchg.alSockets));
		memset(&pack_exchg.aAttr, 0, sizeof(pack_exchg.aAttr));
#ifdef WJ_CHANGELOOK_SYSTEM
		pack_exchg.dwTransmutation = 0;
#endif
	}

	ch->GetDesc()->Packet(&pack_exchg, sizeof(pack_exchg));
}

// 교환을 시작
bool CHARACTER::ExchangeStart(LPCHARACTER victim)
{
	if (this == victim)	// 자기 자신과는 교환을 못한다.
		return false;

	if (IsObserverMode())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("관전 상태에서는 교환을 할 수 없습니다."));
		return false;
	}

	if (victim->IsNPC())
		return false;

	//PREVENT_TRADE_WINDOW
#ifdef WJ_OFFLINESHOP_SYSTEM
	if (IsOpenSafebox() || GetShopOwner() || GetMyShop() || IsCubeOpen() || GetOfflineShopOwner())
#else
	if (IsOpenSafebox() || GetShopOwner() || GetMyShop() || IsCubeOpen())
#endif
	{
		ChatPacket( CHAT_TYPE_INFO, LC_TEXT("다른 거래창이 열려있을경우 거래를 할수 없습니다." ) );
		return false;
	}
	
#ifdef WJ_ITEM_COMBINATION_SYSTEM
	if (IsCombOpen())
	{
		ChatPacket( CHAT_TYPE_INFO, LC_TEXT("다른 거래창이 열려있을경우 거래를 할수 없습니다." ) );
		return false;		
	}
#endif

#ifdef WJ_ACCE_SYSTEM
	if (IsAcceWindowOpen())
	{
		ChatPacket( CHAT_TYPE_INFO, LC_TEXT("다른 거래창이 열려있을경우 거래를 할수 없습니다." ) );
		return false;		
	}
#endif

#ifdef WJ_OFFLINESHOP_SYSTEM
	if (victim->IsOpenSafebox() || victim->GetShopOwner() || victim->GetMyShop() || victim->IsCubeOpen() || victim->GetOfflineShopOwner())
#else
	if (victim->IsOpenSafebox() || victim->GetShopOwner() || victim->GetMyShop() || victim->IsCubeOpen() )
#endif
	{
		ChatPacket( CHAT_TYPE_INFO, LC_TEXT("상대방이 다른 거래중이라 거래를 할수 없습니다." ) );
		return false;
	}
	
#ifdef WJ_ITEM_COMBINATION_SYSTEM
	if (victim->IsCombOpen())
	{
		ChatPacket( CHAT_TYPE_INFO, LC_TEXT("상대방이 다른 거래중이라 거래를 할수 없습니다." ) );
		return false;		
	}
#endif

#ifdef WJ_ACCE_SYSTEM
	if (victim->IsAcceWindowOpen())
	{
		ChatPacket( CHAT_TYPE_INFO, LC_TEXT("상대방이 다른 거래중이라 거래를 할수 없습니다." ) );
		return false;		
	}
#endif
	
#ifdef WJ_SECURITY_SYSTEM
	if (IsActivateSecurity() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot start exchange with security key activate"));
		return false;
	}
	
	if (victim->IsActivateSecurity() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot start exchange with victim security key activate"));
		return false;
	}	
#endif

#ifdef WJ_GROWTH_PET_SYSTEM
	if (GetNewPetSystem()->IsActivePet())
	{
		ChatPacket( CHAT_TYPE_INFO, LC_TEXT("PET_EXCHANGE_UNSUMMON"));
		return false;
	}
	
	if (victim->GetNewPetSystem()->IsActivePet())
	{
		ChatPacket( CHAT_TYPE_INFO, LC_TEXT("PET_EXCHANGE_UNSUMMON_VICITM"));
	}
#endif
	
	//END_PREVENT_TRADE_WINDOW
	int iDist = DISTANCE_APPROX(GetX() - victim->GetX(), GetY() - victim->GetY());

	// 거리 체크
	if (iDist >= EXCHANGE_MAX_DISTANCE)
		return false;

	if (GetExchange())
		return false;

	if (victim->GetExchange())
	{
		exchange_packet(this, EXCHANGE_SUBHEADER_GC_ALREADY, 0, 0, NPOS, 0);
		return false;
	}

	if (victim->IsBlockMode(BLOCK_EXCHANGE))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상대방이 교환 거부 상태입니다."));
		return false;
	}

	SetExchange(M2_NEW CExchange(this));
	victim->SetExchange(M2_NEW CExchange(victim));

	victim->GetExchange()->SetCompany(GetExchange());
	GetExchange()->SetCompany(victim->GetExchange());

	//
	SetExchangeTime();
	victim->SetExchangeTime();

	exchange_packet(victim, EXCHANGE_SUBHEADER_GC_START, 0, GetVID(), NPOS, 0);
	exchange_packet(this, EXCHANGE_SUBHEADER_GC_START, 0, victim->GetVID(), NPOS, 0);
	

	victim->GetExchange()->SendInfo(false, LC_TEXT("TRADE_STARTED"));
	GetExchange()->SendInfo(false, LC_TEXT("TRADE_STARTED"));

	return true;
}

CExchange::CExchange(LPCHARACTER pOwner)
{
	m_pCompany = NULL;

	m_bAccept = false;

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		m_apItems[i] = NULL;
		m_aItemPos[i] = NPOS;
		m_abItemDisplayPos[i] = 0;
	}

	m_lGold = 0;
#ifdef WJ_CHEQUE_SYSTEM
	m_lCheque = 0;
#endif

#ifdef WJ_NEW_EXCHANGE_WINDOW
	m_lLastCriticalUpdatePulse = 0;
#endif

	m_pOwner = pOwner;
	pOwner->SetExchange(this);

#ifdef WJ_NEW_EXCHANGE_WINDOW
	m_pGrid = M2_NEW CGrid(6, 4);
#else
	m_pGrid = M2_NEW CGrid(4, 3);
#endif
}

CExchange::~CExchange()
{
	M2_DELETE(m_pGrid);
}

bool CExchange::AddItem(TItemPos item_pos, BYTE display_pos)
{
	assert(m_pOwner != NULL && GetCompany());

	if (!item_pos.IsValidItemPosition())
		return false;

	// 장비는 교환할 수 없음
	if (item_pos.IsEquipPosition())
		return false;

	LPITEM item;

	if (!(item = m_pOwner->GetItem(item_pos)))
		return false;

	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE))
	{
		m_pOwner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템을 건네줄 수 없습니다."));
		return false;
	}

	if (true == item->isLocked())
	{
		return false;
	}

	// 이미 교환창에 추가된 아이템인가?
	if (item->IsExchanging())
	{
		sys_log(0, "EXCHANGE under exchanging");
		return false;
	}
	
#ifdef WJ_SOULBINDING_SYSTEM
	if (item->IsBind() || item->IsUntilBind())
	{
		m_pOwner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't exchange this item because is binded!"));
		return false;
	}
#endif

	if (!m_pGrid->IsEmpty(display_pos, 1, item->GetSize()))
	{
		sys_log(0, "EXCHANGE not empty item_pos %d %d %d", display_pos, 1, item->GetSize());
		return false;
	}
	
	if (m_pOwner->GetQuestItemPtr() == item)
	{
		sys_log(0, "EXCHANGE %s trying to cheat by using a current quest item in trade", m_pOwner->GetName());
		return false;
	}

	Accept(false);
	GetCompany()->Accept(false);

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (m_apItems[i])
			continue;

		m_apItems[i]		= item;
		m_aItemPos[i]		= item_pos;
		m_abItemDisplayPos[i]	= display_pos;
		m_pGrid->Put(display_pos, 1, item->GetSize());

		item->SetExchanging(true);

		exchange_packet(m_pOwner, 
				EXCHANGE_SUBHEADER_GC_ITEM_ADD,
				true,
				item->GetVnum(),
				TItemPos(RESERVED_WINDOW, display_pos),
				item->GetCount(),
				item);

		exchange_packet(GetCompany()->GetOwner(),
				EXCHANGE_SUBHEADER_GC_ITEM_ADD, 
				false, 
				item->GetVnum(),
				TItemPos(RESERVED_WINDOW, display_pos),
				item->GetCount(),
				item);


#ifdef WJ_NEW_EXCHANGE_WINDOW
		std::string link = item->GetHyperlink();

		if (item->GetCount() > 1) {
			SendInfo(false, LC_TEXT("YOU_ADDED_%s_%d_OF_EXCHANGE_ITEM"), link.c_str(), item->GetCount());
			GetCompany()->SendInfo(false, LC_TEXT("%s_ADDED_%s_%d_OF_EXCHANGE_ITEM"), GetOwner()->GetName(), link.c_str(), item->GetCount());
		}
		else 
		{
			SendInfo(false, LC_TEXT("YOU_ADDED_EXCHANGE_ITEM_%s"), link.c_str());
			GetCompany()->SendInfo(false, LC_TEXT("%s_ADDED_EXCHANGE_ITEM_%s"), GetOwner()->GetName(), link.c_str());
		}	
#endif
				
		sys_log(0, "EXCHANGE AddItem success %s pos(%d, %d) %d", item->GetName(), item_pos.window_type, item_pos.cell, display_pos);

		return true;
	}

	// 추가할 공간이 없음
	return false;
}

#ifdef WJ_NEW_EXCHANGE_WINDOW
bool CExchange::RemoveItem(BYTE pos)
{
	if (pos >= EXCHANGE_ITEM_MAX_NUM)
		return false;

	int k = -1;
	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (m_abItemDisplayPos[i] == pos) 
		{
			k = i;
			break;
		}
	}

	if (k == -1) //Not found
		return false;

	if (!m_apItems[k]) {
		sys_err("No item on position %d when trying to remove it!", k);
		return false;
	}

	TItemPos PosOfInventory = m_aItemPos[k];
	m_apItems[k]->SetExchanging(false);

	m_pGrid->Get(m_abItemDisplayPos[k], 1, m_apItems[k]->GetSize());

	exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_ITEM_DEL, true, pos, NPOS, 0);
	exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_ITEM_DEL, false, pos, PosOfInventory, 0);

	m_lLastCriticalUpdatePulse = thecore_pulse();
	Accept(false);
	GetCompany()->Accept(false);

	//Inform trade window
	std::string link = m_apItems[k]->GetHyperlink();	

	if (m_apItems[k]->GetCount() > 1)
	{
		SendInfo(false, LC_TEXT("YOU_REMOVED_%s_%d_OF_EXCHANGE_ITEM"), link.c_str(), m_apItems[k]->GetCount());
		GetCompany()->SendInfo(false, LC_TEXT("%s_REMOVED_%s_%d_OF_EXCHANGE_ITEM"), GetOwner()->GetName(), link.c_str(), m_apItems[k]->GetCount());
	}
	else
	{
		SendInfo(false, LC_TEXT("YOU_REMOVED_EXCHANGE_ITEM_%s"), link.c_str());
		GetCompany()->SendInfo(false, LC_TEXT("%s_REMOVED_EXCHANGE_ITEM_%s"), GetOwner()->GetName(), link.c_str());
	}

	//Mark as removed
	m_apItems[k]	    = NULL;
	m_aItemPos[k]	    = NPOS;
	m_abItemDisplayPos[k] = 0;
	return true;
}
#else
bool CExchange::RemoveItem(BYTE pos)
{
	if (pos >= EXCHANGE_ITEM_MAX_NUM)
		return false;

	if (!m_apItems[pos])
		return false;

	TItemPos PosOfInventory = m_aItemPos[pos];
	m_apItems[pos]->SetExchanging(false);

	m_pGrid->Get(m_abItemDisplayPos[pos], 1, m_apItems[pos]->GetSize());

	exchange_packet(GetOwner(),	EXCHANGE_SUBHEADER_GC_ITEM_DEL, true, pos, NPOS, 0);
	exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_ITEM_DEL, false, pos, PosOfInventory, 0);

	Accept(false);
	GetCompany()->Accept(false);

	m_apItems[pos]	    = NULL;
	m_aItemPos[pos]	    = NPOS;
	m_abItemDisplayPos[pos] = 0;
	return true;
}
#endif

bool CExchange::AddGold(long gold)
{
	if (gold <= 0)
		return false;

	if (GetOwner()->GetGold() < gold)
	{
		// 가지고 있는 돈이 부족.
		exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_LESS_GOLD, 0, 0, NPOS, 0);
		return false;
	}

	if ( m_lGold > 0 )
	{
		return false;
	}

	Accept(false);
	GetCompany()->Accept(false);

#ifdef WJ_NEW_EXCHANGE_WINDOW
	//Inform trade window
	std::string prettyGold = pretty_number(gold);

	if (m_lGold < gold) { //Before < new value: increased
		SendInfo(false, LC_TEXT("YOU_INCREASED_EXCHANGE_GOLD_TO_%s"), prettyGold.c_str());
		GetCompany()->SendInfo(false, LC_TEXT("%s_INCREASED_EXCHANGE_GOLD_TO_%s"), GetOwner()->GetName(), prettyGold.c_str());
	}
	else {
		m_lLastCriticalUpdatePulse = thecore_pulse();
		SendInfo(false, LC_TEXT("YOU_DECREASED_EXCHANGE_GOLD_TO_%s"), prettyGold.c_str());
		GetCompany()->SendInfo(false, LC_TEXT("%s_DECREASED_EXCHANGE_GOLD_TO_%s"), GetOwner()->GetName(), prettyGold.c_str());
	}
#endif
	
	m_lGold = gold;

	exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_GOLD_ADD, true, m_lGold, NPOS, 0);
	exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_GOLD_ADD, false, m_lGold, NPOS, 0);
	return true;
}

#ifdef WJ_CHEQUE_SYSTEM
bool CExchange::AddCheque(long cheque)
{
	if (cheque <= 0)
		return false;
	
	if (GetOwner()->GetCheque() < cheque)
	{
		exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_LESS_CHEQUE, 0, 0, NPOS, 0);
		return false;
	}

#ifdef WJ_NEW_EXCHANGE_WINDOW
	if (m_lCheque == cheque) //Nothing changed.
		return false;
#else
	if (m_lCheque > 0)
		return false;
#endif
	
	Accept(false);
	GetCompany()->Accept(false);
	
#ifdef WJ_NEW_EXCHANGE_WINDOW
	//Inform trade window
	std::string prettyCheque = pretty_number(cheque);

	if (m_lCheque < cheque) { //Before < new value: increased
		SendInfo(false, LC_TEXT("YOU_INCREASED_EXCHANGE_CHEQUE_TO_%s"), prettyCheque.c_str());
		GetCompany()->SendInfo(false, LC_TEXT("%s_INCREASED_EXCHANGE_CHEQUE_TO_%s"), GetOwner()->GetName(), prettyCheque.c_str());
	}
	else {
		m_lLastCriticalUpdatePulse = thecore_pulse();
		SendInfo(false, LC_TEXT("YOU_DECREASED_EXCHANGE_CHEQUE_TO_%s"), prettyCheque.c_str());
		GetCompany()->SendInfo(false, LC_TEXT("%s_DECREASED_EXCHANGE_CHEQUE_TO_%s"), GetOwner()->GetName(), prettyCheque.c_str());
	}
#endif
	
	m_lCheque = cheque;
	
	exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_CHEQUE_ADD, true, m_lCheque, NPOS, 0);
	exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_CHEQUE_ADD, false, m_lCheque, NPOS, 0);
	return true;
}
#endif

// 돈이 충분히 있는지, 교환하려는 아이템이 실제로 있는지 확인 한다.

bool CExchange::CheckSpace()
{
	static CGrid s_grid1(5, INVENTORY_MAX_NUM / 5 / 4);
	static CGrid s_grid2(5, INVENTORY_MAX_NUM / 5 / 4);
	CGrid * s_grid3 = M2_NEW CGrid(5,0);
	CGrid * s_grid4 = M2_NEW CGrid(5,0);
	
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	static CGrid s_grid5(5, SKILL_BOOK_INVENTORY_MAX_NUM / 5 / 4);
	static CGrid s_grid6(5, UPGRADE_ITEMS_INVENTORY_MAX_NUM / 5 / 4);
#endif
	
	s_grid1.Clear();
	s_grid2.Clear();

	LPCHARACTER	victim = GetCompany()->GetOwner();
	
#ifdef WJ_INVENTORY_EX_SYSTEM		
	int gridsize = (victim->GetInventoryEx()-90)/5;
	if (gridsize >= 9)
	{
		gridsize -= 9;
		s_grid4 = M2_NEW CGrid(5, gridsize);
		s_grid4->Clear();
	}
	else
	{
		s_grid3 = M2_NEW CGrid(5, gridsize);
		s_grid3->Clear();
	}
#endif

#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	s_grid5.Clear();
	s_grid6.Clear();
#endif
	
	LPITEM item;

	int i;

	const int perPageSlotCount = INVENTORY_MAX_NUM / 4;

#ifdef WJ_INVENTORY_EX_SYSTEM
	for (i = 0; i < victim->GetInventoryEx(); ++i) {
#else
	for (i = 0; i < INVENTORY_MAX_NUM; ++i) {
#endif
		if (!(item = victim->GetInventoryItem(i)))
			continue;

		BYTE itemSize = item->GetSize();

		if (i < perPageSlotCount) // Notice: This is adjusted for 4 Pages only!
			s_grid1.Put(i, 1, itemSize);
		else if (i < perPageSlotCount * 2)
			s_grid2.Put(i - perPageSlotCount, 1, itemSize);
#ifdef WJ_INVENTORY_EX_SYSTEM
		else if (i < perPageSlotCount * 3)
			s_grid3->Put(i - perPageSlotCount * 2, 1, itemSize);
		else
			s_grid4->Put(i - perPageSlotCount * 3, 1, itemSize);
#else
		else if (i < perPageSlotCount * 3)
			s_grid3.Put(i - perPageSlotCount * 2, 1, itemSize);
		else
			s_grid4.Put(i - perPageSlotCount * 3, 1, itemSize);
#endif		
	}
	
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	int x;
	int y;
	const int perPageSkillBookSlotCount = SKILL_BOOK_INVENTORY_MAX_NUM / 1;
	const int perPageUpgradeItemsSlotCount = UPGRADE_ITEMS_INVENTORY_MAX_NUM / 1;
	for (x = 0; x < SKILL_BOOK_INVENTORY_MAX_NUM; ++x) {
		if (!(item = victim->GetSkillBookInventoryItem(x)))
			continue;
		
		BYTE itemSize = item->GetSize();
		
		if (x < perPageSkillBookSlotCount)
			s_grid5.Put(x, 1, itemSize);
	}
	
	for (y = 0; y < UPGRADE_ITEMS_INVENTORY_MAX_NUM; ++y) {
		if (!(item = victim->GetUpgradeItemsInventoryItem(y)))
			continue;
		
		BYTE itemSize = item->GetSize();
		
		if (y < perPageUpgradeItemsSlotCount)
			s_grid6.Put(y, 1, itemSize);
	}
#endif

	static std::vector <WORD> s_vDSGrid(DRAGON_SOUL_INVENTORY_MAX_NUM);

	bool bDSInitialized = false;

	for (i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!(item = m_apItems[i]))
			continue;

		BYTE itemSize = item->GetSize();

		if (item->IsDragonSoul())
		{
			if (!victim->DragonSoul_IsQualified())
				return false;

			if (!bDSInitialized) {
				bDSInitialized = true;
				victim->CopyDragonSoulItemGrid(s_vDSGrid);
			}

			bool bExistEmptySpace = false;
			WORD wBasePos = DSManager::instance().GetBasePosition(item);
			if (wBasePos >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				return false;

			for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; i++)
			{
				WORD wPos = wBasePos + i;
				if (0 == s_vDSGrid[wBasePos])
				{
					bool bEmpty = true;
					for (int j = 1; j < item->GetSize(); j++)
					{
						if (s_vDSGrid[wPos + j * DRAGON_SOUL_BOX_COLUMN_NUM])
						{
							bEmpty = false;
							break;
						}
					}
					if (bEmpty)
					{
						for (int j = 0; j < item->GetSize(); j++)
						{
							s_vDSGrid[wPos + j * DRAGON_SOUL_BOX_COLUMN_NUM] = wPos + 1;
						}
						bExistEmptySpace = true;
						break;
					}
				}
				if (bExistEmptySpace)
					break;
			}
			if (!bExistEmptySpace)
				return false;
		}
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
		else if (item->IsSkillBook())
		{
			int iPos = s_grid5.FindBlank(1, itemSize);
			if (iPos >= 0) {
				s_grid5.Put(iPos, 1, itemSize);
				continue;
			}
			
			return false;
		}
		else if (item->IsUpgradeItem())
		{
			int iPos = s_grid6.FindBlank(1, itemSize);
			if (iPos >= 0) {
				s_grid6.Put(iPos, 1, itemSize);
				continue;
			}
			
			return false;
		}
#endif
		else
		{
			int iPos = s_grid1.FindBlank(1, itemSize);
			if (iPos >= 0) {
				s_grid1.Put(iPos, 1, itemSize);
				continue;
			}

			iPos = s_grid2.FindBlank(1, itemSize);
			if (iPos >= 0) {
				s_grid2.Put(iPos, 1, itemSize);
				continue;
			}

#ifdef WJ_INVENTORY_EX_SYSTEM
			if (s_grid3)
			{
				iPos = s_grid3->FindBlank(1, itemSize);
				if (iPos >= 0) {
					s_grid3->Put(iPos, 1, itemSize);
					continue;
				}
			}

			if (s_grid4)
			{
				iPos = s_grid4->FindBlank(1, itemSize);
				if (iPos >= 0) {
					s_grid4->Put(iPos, 1, itemSize);
					continue;
				}
			}
#else
			iPos = s_grid3.FindBlank(1, itemSize);
			if (iPos >= 0) {
				s_grid3.Put(iPos, 1, itemSize);
				continue;
			}

			iPos = s_grid4.FindBlank(1, itemSize);
			if (iPos >= 0) {
				s_grid4.Put(iPos, 1, itemSize);
				continue;
			}	
#endif

			return false;  // No space left in inventory
		}
	}

	return true;
}

#ifdef WJ_NEW_EXCHANGE_WINDOW
bool CExchange::Done(DWORD tradeID, bool firstPlayer)
{
	int		empty_pos, i;
	LPITEM	item;

	LPCHARACTER	victim = GetCompany()->GetOwner();

	if (m_lGold)
	{
		GetOwner()->PointChange(POINT_GOLD, -m_lGold);
		victim->PointChange(POINT_GOLD, m_lGold);
	}

#ifdef WJ_CHEQUE_SYSTEM
	if (m_lCheque)
	{
		GetOwner()->PointChange(POINT_CHEQUE, -m_lCheque);
		victim->PointChange(POINT_CHEQUE, m_lCheque);
	}
#endif

	for (i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!(item = m_apItems[i]))
			continue;

		if (item->IsDragonSoul())
			empty_pos = victim->GetEmptyDragonSoulInventory(item);
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
		else if (item->IsSkillBook())
			empty_pos = victim->GetEmptySkillBookInventory(item->GetSize());
		else if (item->IsUpgradeItem())
			empty_pos = victim->GetEmptyUpgradeItemsInventory(item->GetSize());
#endif
		else
			empty_pos = victim->GetEmptyInventory(item->GetSize());

		if (empty_pos < 0)
		{
			sys_err("Exchange::Done : Cannot find blank position in inventory %s <-> %s item %s", 
					m_pOwner->GetName(), victim->GetName(), item->GetName());
			continue;
		}

		assert(empty_pos >= 0);


		m_pOwner->SyncQuickslot(QUICKSLOT_TYPE_ITEM, (BYTE)item->GetCell(), 255);

		item->RemoveFromCharacter();
		if (item->IsDragonSoul())
			item->AddToCharacter(victim, TItemPos(DRAGON_SOUL_INVENTORY, empty_pos));
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
		else if (item->IsSkillBook())
			item->AddToCharacter(victim, TItemPos(INVENTORY, empty_pos));
		else if (item->IsUpgradeItem())
			item->AddToCharacter(victim, TItemPos(INVENTORY, empty_pos));
#endif
		else
			item->AddToCharacter(victim, TItemPos(INVENTORY, empty_pos));
		ITEM_MANAGER::instance().FlushDelayedSave(item);

		item->SetExchanging(false);

		LogManager::instance().ExchangeItemLog(tradeID, item, firstPlayer ? "A" : "B");
		DWORD itemVnum = item->GetVnum();
		if ((itemVnum >= 80003 && itemVnum <= 80007) || (itemVnum >= 80018 && itemVnum <= 80020))
		{
			LogManager::instance().GoldBarLog(victim->GetPlayerID(), item->GetID(), EXCHANGE_TAKE, "");
			LogManager::instance().GoldBarLog(GetOwner()->GetPlayerID(), item->GetID(), EXCHANGE_GIVE, "");
		}

		m_apItems[i] = NULL;
	}

	m_pGrid->Clear();
	return true;
}

void CExchange::Accept(bool bIsAccept /*= true*/)
{
	if (!GetCompany()) {
		sys_err("Invalid company");
		return;
	}

	// Player can't update 5 seconds after a trade decrease.
	if (bIsAccept && GetCompany()->GetLastCriticalUpdatePulse() != 0 && thecore_pulse() < GetCompany()->GetLastCriticalUpdatePulse() + PASSES_PER_SEC(5)) 
	{
		SendInfo(true, LC_TEXT("YOU_CANT_READY_UP_EXCHANGE"));
		return;
	}

	m_bAccept = !m_bAccept;

	if (!bIsAccept)
		m_bAccept = false;

	//Inform both players
	if (m_bAccept && bIsAccept) // No message on forced removal
	{
		SendInfo(false, LC_TEXT("YOU_READIED_UP_EXCHANGE"));
		GetCompany()->SendInfo(false, LC_TEXT("%s_READIED_UP_EXCHANGE"), GetOwner()->GetName());
	}

	if (!m_bAccept && bIsAccept) // No message on forced removal
	{
		SendInfo(false, LC_TEXT("YOU_REMOVED_READY_EXCHANGE"));
		GetCompany()->SendInfo(false, LC_TEXT("%s_REMOVED_READY_EXCHANGE"), GetOwner()->GetName());
	}

	if (!GetAcceptStatus() || !GetCompany()->GetAcceptStatus())
	{
		// Update the 'accepted' information
		exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_ACCEPT, true, m_bAccept, NPOS, 0);
		exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_ACCEPT, false, m_bAccept, NPOS, 0);
		return;
	}

	//Both accepted, run the trade!
	if (!PerformTrade()) 
	{
		//Something went wrong, unready both sides
		Accept(false);
		GetCompany()->Accept(false); 
	}
	else 
	{
		Cancel(); //All ok, end the trade.
	}
}

bool CExchange::PerformTrade()
{
	LPCHARACTER otherPlayer = GetCompany()->GetOwner();

	//PREVENT_PORTAL_AFTER_EXCHANGE
	GetOwner()->SetExchangeTime();
	otherPlayer->SetExchangeTime();
	//END_PREVENT_PORTAL_AFTER_EXCHANGE

	//Check the player's gold to make sure that the sums are correct
	if (GetOwner()->GetGold() < GetExchangingGold())
	{
		GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("YOU_DONT_HAVE_ENOUGH_GOLD_FOR_EXCHANGE"));
		GetCompany()->GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s_DOESNT_HAVE_ENOUGH_GOLD_FOR_EXCHANGE"), GetOwner()->GetName());
		return false;
	}

	if (otherPlayer->GetGold() < GetCompany()->GetExchangingGold())
	{
		GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s_DOESNT_HAVE_ENOUGH_GOLD_FOR_EXCHANGE"), otherPlayer->GetName());
		GetCompany()->GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("YOU_DONT_HAVE_ENOUGH_GOLD_FOR_EXCHANGE"));
		return false;
	}
	
#ifdef WJ_CHEQUE_SYSTEM
	if (GetOwner()->GetCheque() < GetExchangingCheque())
	{
		GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("YOU_DONT_HAVE_ENOUGH_CHEQUE_FOR_EXCHANGE"));
		GetCompany()->GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s_DOESNT_HAVE_ENOUGH_CHEQUE_FOR_EXCHANGE"), GetOwner()->GetName());
		return false;
	}

	if (otherPlayer->GetCheque() < GetCompany()->GetExchangingCheque())
	{
		GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s_DOESNT_HAVE_ENOUGH_CHEQUE_FOR_EXCHANGE"), otherPlayer->GetName());
		GetCompany()->GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("YOU_DONT_HAVE_ENOUGH_CHEQUE_FOR_EXCHANGE"));
		return false;
	}
#endif

	// Run a sanity check for items that no longer exist,
	// invalid positions, etc.
	if (!SanityCheck())
	{
		GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("EXCHANGE_SANITY_CHECK_FAILED"));
		GetCompany()->GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OTHER_EXCHANGE_SANITY_CHECK_FAILED"));
		return false;
	}

	if (!GetCompany()->SanityCheck())
	{
		GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OTHER_EXCHANGE_SANITY_CHECK_FAILED"));
		GetCompany()->GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("EXCHANGE_SANITY_CHECK_FAILED"));
		return false;
	}

	// Revise that each player can fit all the items from the trade
	// in their inventory.
	if (!CheckSpace())
	{
		GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("NOT_ENOUGH_SPACE_IN_INVENTORY"));
		GetCompany()->GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("EXCHANGE_NO_SPACE_LEFT_OTHER_PERSON"));
		return false;
	}

	if (!GetCompany()->CheckSpace())
	{
		GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("EXCHANGE_NO_SPACE_LEFT_OTHER_PERSON"));
		GetCompany()->GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("NOT_ENOUGH_SPACE_IN_INVENTORY"));
		return false;
	}

	if (db_clientdesc->GetSocket() == INVALID_SOCKET)
	{
		sys_err("Cannot use exchange while DB cache connection is dead.");
		otherPlayer->ChatPacket(CHAT_TYPE_INFO, "Unknown trade error.");
		GetOwner()->ChatPacket(CHAT_TYPE_INFO, "Unknown trade error.");
		Cancel();
		return false;
	}

	//If nothing was traded, cancel things out
	if (CountExchangingItems() + GetCompany()->CountExchangingItems() == 0 && GetExchangingGold() + GetCompany()->GetExchangingGold() == 0)
	{
		otherPlayer->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("TRADE_COMPLETED_WITHOUT_GOODS_EXCHANGE"));
		GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("TRADE_COMPLETED_WITHOUT_GOODS_EXCHANGE"));
		Cancel();
		return false;
	}

#ifdef WJ_CHEQUE_SYSTEM
	DWORD tradeID = LogManager::instance().ExchangeLog(EXCHANGE_TYPE_TRADE, GetOwner()->GetPlayerID(), otherPlayer->GetPlayerID(), GetOwner()->GetX(), GetOwner()->GetY(), m_lGold, GetCompany()->m_lGold, m_lCheque, GetCompany()->m_lCheque);
#else
	DWORD tradeID = LogManager::instance().ExchangeLog(EXCHANGE_TYPE_TRADE, GetOwner()->GetPlayerID(), otherPlayer->GetPlayerID(), GetOwner()->GetX(), GetOwner()->GetY(), m_lGold, GetCompany()->m_lGold);
#endif

	if (Done(tradeID, true))
	{
		if (m_lGold) // 돈이 있을 떄만 저장
			GetOwner()->Save();

		if (GetCompany()->Done(tradeID, false))
		{
			if (GetCompany()->m_lGold) // 돈이 있을 때만 저장
				otherPlayer->Save();

			// INTERNATIONAL_VERSION
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 님과의 교환이 성사 되었습니다."), otherPlayer->GetName());
			otherPlayer->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 님과의 교환이 성사 되었습니다."), GetOwner()->GetName());
			// END_OF_INTERNATIONAL_VERSION
		}
	}

	return true;
}

bool CExchange::SanityCheck()
{
	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!m_apItems[i])
			continue;

		if (!m_aItemPos[i].IsValidItemPosition())
			return false;

		if (m_apItems[i] != GetOwner()->GetItem(m_aItemPos[i]))
			return false;
	}

	return true;
}
#else
bool CExchange::Accept(bool bAccept)
{
	if (m_bAccept == bAccept)
		return true;

	m_bAccept = bAccept;

	// 둘 다 동의 했으므로 교환 성립
	if (m_bAccept && GetCompany()->m_bAccept)
	{
		int	iItemCount;

		LPCHARACTER victim = GetCompany()->GetOwner();

		//PREVENT_PORTAL_AFTER_EXCHANGE
		GetOwner()->SetExchangeTime();
		victim->SetExchangeTime();		
		//END_PREVENT_PORTAL_AFTER_EXCHANGE

		// exchange_check 에서는 교환할 아이템들이 제자리에 있나 확인하고,
		// 엘크도 충분히 있나 확인한다, 두번째 인자로 교환할 아이템 개수
		// 를 리턴한다.
		if (!Check(&iItemCount))
		{
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("돈이 부족하거나 아이템이 제자리에 없습니다."));
			victim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상대방의 돈이 부족하거나 아이템이 제자리에 없습니다."));
			goto EXCHANGE_END;
		}

		// 리턴 받은 아이템 개수로 상대방의 소지품에 남은 자리가 있나 확인한다.
		if (!CheckSpace())
		{
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상대방의 소지품에 빈 공간이 없습니다."));
			victim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지품에 빈 공간이 없습니다."));
			goto EXCHANGE_END;
		}

		// 상대방도 마찬가지로..
		if (!GetCompany()->Check(&iItemCount))
		{
			victim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("돈이 부족하거나 아이템이 제자리에 없습니다."));
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상대방의 돈이 부족하거나 아이템이 제자리에 없습니다."));
			goto EXCHANGE_END;
		}

		if (!GetCompany()->CheckSpace())
		{
			victim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상대방의 소지품에 빈 공간이 없습니다."));
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지품에 빈 공간이 없습니다."));
			goto EXCHANGE_END;
		}

		if (db_clientdesc->GetSocket() == INVALID_SOCKET)
		{
			sys_err("Cannot use exchange feature while DB cache connection is dead.");
			victim->ChatPacket(CHAT_TYPE_INFO, "Unknown error");
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, "Unknown error");
			goto EXCHANGE_END;
		}

		if (Done())
		{
#ifdef WJ_CHEQUE_SYSTEM
			if (m_lGold || m_lCheque)
#else
			if (m_lGold) // 돈이 있을 떄만 저장
#endif
				GetOwner()->Save();

			if (GetCompany()->Done())
			{
#ifdef WJ_CHEQUE_SYSTEM
				if (GetCompany()->m_lGold || GetCompany()->m_lCheque)
#else
				if (GetCompany()->m_lGold) // 돈이 있을 때만 저장
#endif
					victim->Save();

				// INTERNATIONAL_VERSION
				GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 님과의 교환이 성사 되었습니다."), victim->GetName());
				victim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 님과의 교환이 성사 되었습니다."), GetOwner()->GetName());
				// END_OF_INTERNATIONAL_VERSION
			}
		}

EXCHANGE_END:
		Cancel();
		return false;
	}
	else
	{
		// 아니면 accept에 대한 패킷을 보내자.
		exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_ACCEPT, true, m_bAccept, NPOS, 0);
		exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_ACCEPT, false, m_bAccept, NPOS, 0);
		return true;
	}
}

// 교환 끝 (아이템과 돈 등을 실제로 옮긴다)
bool CExchange::Done()
{
	int		empty_pos, i;
	LPITEM	item;

	LPCHARACTER	victim = GetCompany()->GetOwner();

	for (i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!(item = m_apItems[i]))
			continue;

		if (item->IsDragonSoul())
			empty_pos = victim->GetEmptyDragonSoulInventory(item);
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
		else if (item->IsSkillBook())
			empty_pos = victim->GetEmptySkillBookInventory(item->GetSize());
		else if (item->IsUpgradeItem())
			empty_pos = victim->GetEmptyUpgradeItemsInventory(item->GetSize());
#endif
		else
			empty_pos = victim->GetEmptyInventory(item->GetSize());

		if (empty_pos < 0)
		{
			sys_err("Exchange::Done : Cannot find blank position in inventory %s <-> %s item %s", 
					m_pOwner->GetName(), victim->GetName(), item->GetName());
			continue;
		}

		assert(empty_pos >= 0);

		if (item->GetVnum() == 90008 || item->GetVnum() == 90009) // VCARD
		{
			VCardUse(m_pOwner, victim, item);
			continue;
		}

		m_pOwner->SyncQuickslot(QUICKSLOT_TYPE_ITEM, item->GetCell(), 1000);

		item->RemoveFromCharacter();
		if (item->IsDragonSoul())
			item->AddToCharacter(victim, TItemPos(DRAGON_SOUL_INVENTORY, empty_pos));
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
		else if (item->IsSkillBook())
			item->AddToCharacter(victim, TItemPos(INVENTORY, empty_pos));
		else if (item->IsUpgradeItem())
			item->AddToCharacter(victim, TItemPos(INVENTORY, empty_pos));
#endif
		else
			item->AddToCharacter(victim, TItemPos(INVENTORY, empty_pos));
		ITEM_MANAGER::instance().FlushDelayedSave(item);

		item->SetExchanging(false);
		{
			char exchange_buf[51];

			snprintf(exchange_buf, sizeof(exchange_buf), "%s %u %u", item->GetName(), GetOwner()->GetPlayerID(), item->GetCount());
			LogManager::instance().ItemLog(victim, item, "EXCHANGE_TAKE", exchange_buf);

			snprintf(exchange_buf, sizeof(exchange_buf), "%s %u %u", item->GetName(), victim->GetPlayerID(), item->GetCount());
			LogManager::instance().ItemLog(GetOwner(), item, "EXCHANGE_GIVE", exchange_buf);

			if (item->GetVnum() >= 80003 && item->GetVnum() <= 80007)
			{
				LogManager::instance().GoldBarLog(victim->GetPlayerID(), item->GetID(), EXCHANGE_TAKE, "");
				LogManager::instance().GoldBarLog(GetOwner()->GetPlayerID(), item->GetID(), EXCHANGE_GIVE, "");
			}
		}

		m_apItems[i] = NULL;
	}

	if (m_lGold)
	{
		GetOwner()->PointChange(POINT_GOLD, -m_lGold, true);
		victim->PointChange(POINT_GOLD, m_lGold, true);

		if (m_lGold > 1000)
		{
			char exchange_buf[51];
			snprintf(exchange_buf, sizeof(exchange_buf), "%u %s", GetOwner()->GetPlayerID(), GetOwner()->GetName());
			LogManager::instance().CharLog(victim, m_lGold, "EXCHANGE_GOLD_TAKE", exchange_buf);

			snprintf(exchange_buf, sizeof(exchange_buf), "%u %s", victim->GetPlayerID(), victim->GetName());
			LogManager::instance().CharLog(GetOwner(), m_lGold, "EXCHANGE_GOLD_GIVE", exchange_buf);
		}
	}
	
#ifdef WJ_CHEQUE_SYSTEM
	if (m_lCheque)
	{
		GetOwner()->PointChange(POINT_CHEQUE, -m_lCheque, true);
		victim->PointChange(POINT_CHEQUE, m_lCheque, true);
		
		if (m_lCheque > 10)
		{
			char exchange_buf[51];
			snprintf(exchange_buf, sizeof(exchange_buf), "%u %s", GetOwner()->GetPlayerID(), GetOwner()->GetName());
			LogManager::instance().CharLog(victim, m_lCheque, "EXCHANGE_CHEQUE_TAKE", exchange_buf);
			
			snprintf(exchange_buf, sizeof(exchange_buf), "%u %s", victim->GetPlayerID(), victim->GetName());
			LogManager::instance().CharLog(GetOwner(), m_lCheque, "EXCHANGE_CHEQUE_GIVE", exchange_buf);
		}
	}
#endif

	m_pGrid->Clear();
	return true;
}

bool CExchange::Check(int * piItemCount)
{
	if (GetOwner()->GetGold() < m_lGold)
		return false;
	
#ifdef WJ_CHEQUE_SYSTEM
	if (GetOwner()->GetCheque() < m_lCheque)
		return false;
#endif

	int item_count = 0;

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!m_apItems[i])
			continue;

		if (!m_aItemPos[i].IsValidItemPosition())
			return false;

		if (m_apItems[i] != GetOwner()->GetItem(m_aItemPos[i]))
			return false;

		++item_count;
	}

	*piItemCount = item_count;
	return true;
}
#endif

// 교환 취소
void CExchange::Cancel()
{
	exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_END, 0, 0, NPOS, 0);
	GetOwner()->SetExchange(NULL);

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (m_apItems[i])
			m_apItems[i]->SetExchanging(false);
	}

	if (GetCompany())
	{
		GetCompany()->SetCompany(NULL);
		GetCompany()->Cancel();
	}

	M2_DELETE(this);
}

#ifdef WJ_NEW_EXCHANGE_WINDOW
void CExchange::SendInfo(bool isError, const char * format, ...)
{
	if (!GetOwner())
		return;

	LPDESC d = GetOwner()->GetDesc();
	if (!d)
		return;

	char rawbuf[512 + 1];
	va_list args;

	va_start(args, format);
	int len = vsnprintf(rawbuf, sizeof(rawbuf), format, args);
	va_end(args);

	TPacketGCExchageInfo pack;

	pack.bHeader = HEADER_GC_EXCHANGE_INFO;
	pack.wSize = sizeof(TPacketGCExchageInfo) + len;
	pack.bError = isError;
	pack.iUnixTime = std::time(nullptr);

	TEMP_BUFFER tmpbuf;
	tmpbuf.write(&pack, sizeof(pack));
	tmpbuf.write(rawbuf, len);

	d->Packet(tmpbuf.read_peek(), tmpbuf.size());
}

int CExchange::CountExchangingItems()
{
	int count = 0;
	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!m_apItems[i])
			continue;

		++count;
	}

	return count;
}

int CExchange::GetPositionByItem(LPITEM item) const
{
	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!m_apItems[i])
			continue;

		if (m_apItems[i] == item)
			return i;
	}

	return -1;
}
#endif