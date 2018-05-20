#include "stdafx.h"

#include <stack>

#include "utils.h"
#include "config.h"
#include "char.h"
#include "char_manager.h"
#include "item_manager.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "packet.h"
#include "affect.h"
#include "skill.h"
#include "start_position.h"
#include "mob_manager.h"
#include "db.h"
#include "log.h"
#include "vector.h"
#include "buffer_manager.h"
#include "questmanager.h"
#include "fishing.h"
#include "party.h"
#include "dungeon.h"
#include "refine.h"
#include "unique_item.h"
#include "war_map.h"
#include "xmas_event.h"
#include "marriage.h"
#include "monarch.h"
#include "polymorph.h"
#include "blend_item.h"
#include "castle.h"
#include "BattleArena.h"
#include "arena.h"
#include "dev_log.h"
#include "pcbang.h"
#include "threeway_war.h"

#include "safebox.h"
#include "shop.h"

#include "../../common/VnumHelper.h"
#include "DragonSoul.h"
#include "buff_on_attributes.h"
#include "belt_inventory_helper.h"
#include "../../common/defines.h"

//auction_temp
#ifdef __AUCTION__
#include "auction_manager.h"
#endif

#ifdef WJ_GROWTH_PET_SYSTEM
#include "New_PetSystem.h"
#endif
#ifdef WJ_EXTENDED_PET_SYSTEM
#include "PetSystem.h"
#endif
#ifdef WJ_MELEY_LAIR_DUNGEON
#include "MeleyLair.h"
#endif
const int ITEM_BROKEN_METIN_VNUM = 28960;

// CHANGE_ITEM_ATTRIBUTES
const DWORD CHARACTER::msc_dwDefaultChangeItemAttrCycle = 10;
const char CHARACTER::msc_szLastChangeItemAttrFlag[] = "Item.LastChangeItemAttr";
const char CHARACTER::msc_szChangeItemAttrCycleFlag[] = "change_itemattr_cycle";
// END_OF_CHANGE_ITEM_ATTRIBUTES
const BYTE g_aBuffOnAttrPoints[] = { POINT_ENERGY, POINT_COSTUME_ATTR_BONUS };

struct FFindStone
{
	std::map<DWORD, LPCHARACTER> m_mapStone;

	void operator()(LPENTITY pEnt)
	{
		if (pEnt->IsType(ENTITY_CHARACTER) == true)
		{
			LPCHARACTER pChar = (LPCHARACTER)pEnt;

			if (pChar->IsStone() == true)
			{
				m_mapStone[(DWORD)pChar->GetVID()] = pChar;
			}
		}
	}
};


//귀환부, 귀환기억부, 결혼반지
static bool IS_SUMMON_ITEM(int vnum)
{
	switch (vnum)
	{
		case 22000:
		case 22010:
		case 22011:
		case 22020:
		case ITEM_MARRIAGE_RING:
			return true;
	}

	return false;
}

static bool IS_MONKEY_DUNGEON(int map_index)
{
	switch (map_index)
	{
		case 5:
		case 25:
		case 45:
		case 108:
		case 109:
			return true;;
	}

	return false;
}

bool IS_SUMMONABLE_ZONE(int map_index)
{
	// 몽키던전
	if (IS_MONKEY_DUNGEON(map_index))
		return false;
	// 성
	if (IS_CASTLE_MAP(map_index))
		return false;

	switch (map_index)
	{
		case 66 : // 사귀타워
		case 71 : // 거미 던전 2층
		case 72 : // 천의 동굴
		case 73 : // 천의 동굴 2층
		case 193 : // 거미 던전 2-1층
#if 0
		case 184 : // 천의 동굴(신수)
		case 185 : // 천의 동굴 2층(신수)
		case 186 : // 천의 동굴(천조)
		case 187 : // 천의 동굴 2층(천조)
		case 188 : // 천의 동굴(진노)
		case 189 : // 천의 동굴 2층(진노)
#endif
//		case 206 : // 아귀동굴
		case 216 : // 아귀동굴
		case 217 : // 거미 던전 3층
		case 208 : // 천의 동굴 (용방)
			return false;
	}

	if (CBattleArena::IsBattleArenaMap(map_index)) return false;

	// 모든 private 맵으론 워프 불가능
	if (map_index > 10000) return false;

	return true;
}

bool IS_BOTARYABLE_ZONE(int nMapIndex)
{
	if (!g_bEnableBootaryCheck) return true;

	switch (nMapIndex)
	{
		case 1 :
		case 3 :
		case 21 :
		case 23 :
		case 41 :
		case 43 :
			return true;
	}

	return false;
}

// item socket 이 프로토타입과 같은지 체크 -- by mhh
static bool FN_check_item_socket(LPITEM item)
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		if (item->GetSocket(i) != item->GetProto()->alSockets[i])
			return false;
	}

	return true;
}

// item socket 복사 -- by mhh
static void FN_copy_item_socket(LPITEM dest, LPITEM src)
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		dest->SetSocket(i, src->GetSocket(i));
	}
}
static bool FN_check_item_sex(LPCHARACTER ch, LPITEM item)
{
	// 남자 금지
	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MALE))
	{
		if (SEX_MALE==GET_SEX(ch))
			return false;
	}
	// 여자금지
	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_FEMALE)) 
	{
		if (SEX_FEMALE==GET_SEX(ch))
			return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////
// ITEM HANDLING
/////////////////////////////////////////////////////////////////////////////
bool CHARACTER::CanHandleItem(bool bSkipCheckRefine, bool bSkipObserver)
{
	if (!bSkipObserver)
		if (m_bIsObserver)
			return false;

	if (GetMyShop())
		return false;

	if (!bSkipCheckRefine)
		if (m_bUnderRefine)
			return false;

	if (IsCubeOpen() || NULL != DragonSoul_RefineWindow_GetOpener())
		return false;

	if (IsWarping())
		return false;
	
#ifdef WJ_CHANGELOOK_SYSTEM
	if (m_bChangeLook)
		return false;
#endif

#ifdef WJ_ITEM_COMBINATION_SYSTEM
	if (IsCombOpen())
		return false;
#endif

#ifdef WJ_ACCE_SYSTEM
	if (IsAcceWindowOpen())
		return false;
#endif

	return true;
}

LPITEM CHARACTER::GetInventoryItem(WORD wCell) const
{
	return GetItem(TItemPos(INVENTORY, wCell));
}
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
LPITEM CHARACTER::GetSkillBookInventoryItem(WORD wCell) const
{
	return GetItem(TItemPos(INVENTORY, wCell));
}
LPITEM CHARACTER::GetUpgradeItemsInventoryItem(WORD wCell) const
{
	return GetItem(TItemPos(INVENTORY, wCell));
}
#endif
#ifdef ENABLE_NEWSTUFF
LPITEM CHARACTER::GetFullInventoryItem(WORD wCell) const
{
	return GetItem(TItemPos(EQUIPMENT, wCell));
}
#endif
LPITEM CHARACTER::GetItem(TItemPos Cell) const
{
	if (!IsValidItemPosition(Cell))
		return NULL;
	WORD wCell = Cell.cell;
	BYTE window_type = Cell.window_type;
	switch (window_type)
	{
	case INVENTORY:
	case EQUIPMENT:
		if (wCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
		{
			sys_err("CHARACTER::GetInventoryItem: invalid item cell %d", wCell);
			return NULL;
		}
		return m_pointsInstant.pItems[wCell];
	case DRAGON_SOUL_INVENTORY:
		if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			sys_err("CHARACTER::GetInventoryItem: invalid DS item cell %d", wCell);
			return NULL;
		}
		return m_pointsInstant.pDSItems[wCell];

	default:
		return NULL;
	}
	return NULL;
}

#ifdef WJ_HIGHLIGHT_SYSTEM
void CHARACTER::SetItem(TItemPos Cell, LPITEM pItem, bool isHighLight)
#else
void CHARACTER::SetItem(TItemPos Cell, LPITEM pItem)
#endif
{
	WORD wCell = Cell.cell;
	BYTE window_type = Cell.window_type;
	if ((unsigned long)((CItem*)pItem) == 0xff || (unsigned long)((CItem*)pItem) == 0xffffffff)
	{
		sys_err("!!! FATAL ERROR !!! item == 0xff (char: %s cell: %u)", GetName(), wCell);
		core_dump();
		return;
	}

	if (pItem && pItem->GetOwner())
	{
		assert(!"GetOwner exist");
		return;
	}
	// 기본 인벤토리
	switch(window_type)
	{
	case INVENTORY:
	case EQUIPMENT:
		{
			if (wCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
			{
				sys_err("CHARACTER::SetItem: invalid item cell %d", wCell);
				return;
			}

			LPITEM pOld = m_pointsInstant.pItems[wCell];

			if (pOld)
			{
#ifdef WJ_INVENTORY_EX_SYSTEM
				if (wCell < GetInventoryEx())
#else
				if (wCell < INVENTORY_MAX_NUM)
#endif
				{
					for (int i = 0; i < pOld->GetSize(); ++i)
					{
						int p = wCell + (i * 5);

#ifdef WJ_INVENTORY_EX_SYSTEM
						if (p >= GetInventoryEx())
#else
						if (p >= INVENTORY_MAX_NUM)
#endif
							continue;

						if (m_pointsInstant.pItems[p] && m_pointsInstant.pItems[p] != pOld)
							continue;

						m_pointsInstant.bItemGrid[p] = 0;
					}
				}
				else
					m_pointsInstant.bItemGrid[wCell] = 0;
			}

			if (pItem)
			{
#ifdef WJ_INVENTORY_EX_SYSTEM
				if (wCell < GetInventoryEx())
#else
				if (wCell < INVENTORY_MAX_NUM)
#endif
				{
					for (int i = 0; i < pItem->GetSize(); ++i)
					{
						int p = wCell + (i * 5);

#ifdef WJ_INVENTORY_EX_SYSTEM
						if (p >= GetInventoryEx())
#else
						if (p >= INVENTORY_MAX_NUM)
#endif
							continue;

						// wCell + 1 로 하는 것은 빈곳을 체크할 때 같은
						// 아이템은 예외처리하기 위함
						m_pointsInstant.bItemGrid[p] = wCell + 1;
					}
				}
				else
					m_pointsInstant.bItemGrid[wCell] = wCell + 1;
			}

			m_pointsInstant.pItems[wCell] = pItem;
		}
		break;
	// 용혼석 인벤토리
	case DRAGON_SOUL_INVENTORY:
		{
			LPITEM pOld = m_pointsInstant.pDSItems[wCell];

			if (pOld)
			{
				if (wCell < DRAGON_SOUL_INVENTORY_MAX_NUM)
				{
					for (int i = 0; i < pOld->GetSize(); ++i)
					{
						int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

						if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
							continue;

						if (m_pointsInstant.pDSItems[p] && m_pointsInstant.pDSItems[p] != pOld)
							continue;

						m_pointsInstant.wDSItemGrid[p] = 0;
					}
				}
				else
					m_pointsInstant.wDSItemGrid[wCell] = 0;
			}

			if (pItem)
			{
				if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				{
					sys_err("CHARACTER::SetItem: invalid DS item cell %d", wCell);
					return;
				}

				if (wCell < DRAGON_SOUL_INVENTORY_MAX_NUM)
				{
					for (int i = 0; i < pItem->GetSize(); ++i)
					{
						int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

						if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
							continue;

						// wCell + 1 로 하는 것은 빈곳을 체크할 때 같은
						// 아이템은 예외처리하기 위함
						m_pointsInstant.wDSItemGrid[p] = wCell + 1;
					}
				}
				else
					m_pointsInstant.wDSItemGrid[wCell] = wCell + 1;
			}

			m_pointsInstant.pDSItems[wCell] = pItem;
		}
		break;
		
	default:
		sys_err ("Invalid Inventory type %d", window_type);
		return;
	}

	if (GetDesc())
	{
		// 확장 아이템: 서버에서 아이템 플래그 정보를 보낸다
		if (pItem)
		{
			TPacketGCItemSet pack;
			pack.header = HEADER_GC_ITEM_SET;
			pack.Cell = Cell;

			pack.count = pItem->GetCount();
#ifdef WJ_SOULBINDING_SYSTEM
			pack.bind = pItem->GetBind();
#endif
#ifdef WJ_CHANGELOOK_SYSTEM
			pack.transmutation = pItem->GetTransmutation();
#endif
			pack.vnum = pItem->GetVnum();
			pack.flags = pItem->GetFlag();
			pack.anti_flags	= pItem->GetAntiFlag();
#ifdef WJ_HIGHLIGHT_SYSTEM
			if (isHighLight)
				pack.highlight = true;
			else
				pack.highlight = (Cell.window_type == DRAGON_SOUL_INVENTORY);
#else
			pack.highlight = (Cell.window_type == DRAGON_SOUL_INVENTORY);
#endif

			thecore_memcpy(pack.alSockets, pItem->GetSockets(), sizeof(pack.alSockets));
			thecore_memcpy(pack.aAttr, pItem->GetAttributes(), sizeof(pack.aAttr));
			
			GetDesc()->Packet(&pack, sizeof(TPacketGCItemSet));
		}
		else
		{
			TPacketGCItemDelDeprecated pack;
			pack.header = HEADER_GC_ITEM_DEL;
			pack.Cell = Cell;
			pack.count = 0;
#ifdef WJ_SOULBINDING_SYSTEM
			pack.bind = 0;
#endif
#ifdef WJ_CHANGELOOK_SYSTEM
			pack.transmutation = 0;
#endif
			pack.vnum = 0;
			memset(pack.alSockets, 0, sizeof(pack.alSockets));
			memset(pack.aAttr, 0, sizeof(pack.aAttr));
			
			GetDesc()->Packet(&pack, sizeof(TPacketGCItemDelDeprecated));
		}
	}

	if (pItem)
	{
		pItem->SetCell(this, wCell);
		switch (window_type)
		{
		case INVENTORY:
		case EQUIPMENT:
			if ((wCell < INVENTORY_MAX_NUM) || (BELT_INVENTORY_SLOT_START <= wCell && BELT_INVENTORY_SLOT_END > wCell) 
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
				|| (SKILL_BOOK_INVENTORY_SLOT_START <= wCell && SKILL_BOOK_INVENTORY_SLOT_END > wCell) || (UPGRADE_ITEMS_INVENTORY_SLOT_START <= wCell && UPGRADE_ITEMS_INVENTORY_SLOT_END > wCell)
#endif
			)
				pItem->SetWindow(INVENTORY);
			else
				pItem->SetWindow(EQUIPMENT);
			break;
		case DRAGON_SOUL_INVENTORY:
			pItem->SetWindow(DRAGON_SOUL_INVENTORY);
			break;
		}
	}
}

LPITEM CHARACTER::GetWear(UINT bCell) const
{
	// > WEAR_MAX_NUM : 용혼석 슬롯들.
	if (bCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
	{
		sys_err("CHARACTER::GetWear: invalid wear cell %d", bCell);
		return NULL;
	}

	return m_pointsInstant.pItems[INVENTORY_MAX_NUM + bCell];
}

void CHARACTER::SetWear(UINT bCell, LPITEM item)
{
	// > WEAR_MAX_NUM : 용혼석 슬롯들.
	if (bCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
	{
		sys_err("CHARACTER::SetItem: invalid item cell %d", bCell);
		return;
	}

#ifdef WJ_HIGHLIGHT_SYSTEM
	SetItem(TItemPos (INVENTORY, INVENTORY_MAX_NUM + bCell), item, false);
#else
	SetItem(TItemPos (INVENTORY, INVENTORY_MAX_NUM + bCell), item);
#endif

	if (!item && bCell == WEAR_WEAPON)
	{
		// 귀검 사용 시 벗는 것이라면 효과를 없애야 한다.
		if (IsAffectFlag(AFF_GWIGUM))
			RemoveAffect(SKILL_GWIGEOM);

		if (IsAffectFlag(AFF_GEOMGYEONG))
			RemoveAffect(SKILL_GEOMKYUNG);
	}
}

void CHARACTER::ClearItem()
{
	int		i;
	LPITEM	item;
	
	for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
	{
		if ((item = GetInventoryItem(i)))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);

			SyncQuickslot(QUICKSLOT_TYPE_ITEM, i, 1000);
		}
	}
	for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
	{
		if ((item = GetItem(TItemPos(DRAGON_SOUL_INVENTORY, i))))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);
		}
	}
}

bool CHARACTER::IsEmptyItemGrid(TItemPos Cell, BYTE bSize, int iExceptionCell) const
{
	switch (Cell.window_type)
	{
	case INVENTORY:
		{
			UINT bCell = Cell.cell;

			// bItemCell은 0이 false임을 나타내기 위해 + 1 해서 처리한다.
			// 따라서 iExceptionCell에 1을 더해 비교한다.
			++iExceptionCell;

			if (Cell.IsBeltInventoryPosition())
			{
				LPITEM beltItem = GetWear(WEAR_BELT);

				if (NULL == beltItem)
					return false;

				if (false == CBeltInventoryHelper::IsAvailableCell(bCell - BELT_INVENTORY_SLOT_START, beltItem->GetValue(0)))
					return false;

				if (m_pointsInstant.bItemGrid[bCell])
				{
					if (m_pointsInstant.bItemGrid[bCell] == (UINT)iExceptionCell)
						return true;

					return false;
				}

				if (bSize == 1)
					return true;

			}
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
			else if (Cell.IsSkillBookInventoryPosition())
			{
				if (bCell < SKILL_BOOK_INVENTORY_SLOT_START)
					return false;
				
				if (bCell > SKILL_BOOK_INVENTORY_SLOT_END)
					return false;
				
				if (m_pointsInstant.bItemGrid[bCell] == (UINT)iExceptionCell)
				{
					if (bSize == 1)
						return true;

					int j = 1;
					BYTE bPage = bCell / (SKILL_BOOK_INVENTORY_MAX_NUM / 3);

					do
					{
						UINT p = bCell + (5 * j);

						if (p >= SKILL_BOOK_INVENTORY_MAX_NUM)
							return false;

						if (p / (SKILL_BOOK_INVENTORY_MAX_NUM / 3) != bPage)
							return false;

						if (m_pointsInstant.bItemGrid[p])
							if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
								return false;
					}
					while (++j < bSize);

					return true;
				}
			}
			else if (Cell.IsUpgradeItemsInventoryPosition())
			{
				if (bCell < UPGRADE_ITEMS_INVENTORY_SLOT_START)
					return false;
				
				if (bCell > UPGRADE_ITEMS_INVENTORY_SLOT_END)
					return false;
				
				if (m_pointsInstant.bItemGrid[bCell] == (UINT)iExceptionCell)
				{
					if (bSize == 1)
						return true;

					int j = 1;
					BYTE bPage = bCell / (UPGRADE_ITEMS_INVENTORY_MAX_NUM / 3);

					do
					{
						UINT p = bCell + (5 * j);

						if (p >= UPGRADE_ITEMS_INVENTORY_MAX_NUM)
							return false;

						if (p / (UPGRADE_ITEMS_INVENTORY_MAX_NUM / 3) != bPage)
							return false;

						if (m_pointsInstant.bItemGrid[p])
							if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
								return false;
					}
					while (++j < bSize);

					return true;
				}
			}
#endif
#ifdef WJ_INVENTORY_EX_SYSTEM
			else if (bCell >= GetInventoryEx())
#else
			else if (bCell >= INVENTORY_MAX_NUM)
#endif
				return false;

			if (m_pointsInstant.bItemGrid[bCell])
			{
				if (m_pointsInstant.bItemGrid[bCell] == (UINT)iExceptionCell)
				{
					if (bSize == 1)
						return true;

					int j = 1;
					BYTE bPage = bCell / (INVENTORY_MAX_NUM / 4);

					do
					{
						UINT p = bCell + (5 * j);

#ifdef WJ_INVENTORY_EX_SYSTEM
						if (p >= GetInventoryEx())
#else
						if (p >= INVENTORY_MAX_NUM)
#endif
							return false;

						if (p / (INVENTORY_MAX_NUM / 4) != bPage)
							return false;

						if (m_pointsInstant.bItemGrid[p])
							if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
								return false;
					}
					while (++j < bSize);

					return true;
				}
				else
					return false;
			}

			// 크기가 1이면 한칸을 차지하는 것이므로 그냥 리턴
			if (1 == bSize)
				return true;
			else
			{
				int j = 1;
				BYTE bPage = bCell / (INVENTORY_MAX_NUM / 4);

				do
				{
					UINT p = bCell + (5 * j);

#ifdef WJ_INVENTORY_EX_SYSTEM
					if (p >= GetInventoryEx())
#else
					if (p >= INVENTORY_MAX_NUM)
#endif
						return false;

					if (p / (INVENTORY_MAX_NUM / 4) != bPage)
						return false;

					if (m_pointsInstant.bItemGrid[p])
						if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
							return false;
				}
				while (++j < bSize);

				return true;
			}
		}
		break;
	case DRAGON_SOUL_INVENTORY:
		{
			WORD wCell = Cell.cell;
			if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				return false;

			// bItemCell은 0이 false임을 나타내기 위해 + 1 해서 처리한다.
			// 따라서 iExceptionCell에 1을 더해 비교한다.
			iExceptionCell++;

			if (m_pointsInstant.wDSItemGrid[wCell])
			{
				if (m_pointsInstant.wDSItemGrid[wCell] == iExceptionCell)
				{
					if (bSize == 1)
						return true;

					int j = 1;

					do
					{
						// @fixme BYTE -> int
						int p = wCell + (DRAGON_SOUL_BOX_COLUMN_NUM * j);

						if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
							return false;

						if (m_pointsInstant.wDSItemGrid[p])
							if (m_pointsInstant.wDSItemGrid[p] != iExceptionCell)
								return false;
					}
					while (++j < bSize);

					return true;
				}
				else
					return false;
			}

			// 크기가 1이면 한칸을 차지하는 것이므로 그냥 리턴
			if (1 == bSize)
				return true;
			else
			{
				int j = 1;

				do
				{
					// @fixme BYTE -> int
					int p = wCell + (DRAGON_SOUL_BOX_COLUMN_NUM * j);

					if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
						return false;

					if (m_pointsInstant.bItemGrid[p])
						if (m_pointsInstant.wDSItemGrid[p] != iExceptionCell)
							return false;
				}
				while (++j < bSize);

				return true;
			}
		}
	}
	
	return false;
}

int CHARACTER::GetEmptyInventory(BYTE size) const
{
#ifdef WJ_INVENTORY_EX_SYSTEM
	for ( int i = 0; i < GetInventoryEx(); ++i)
#else
	for ( int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		if (IsEmptyItemGrid(TItemPos (INVENTORY, i), size))
			return i;
		
	return -1;
}

int CHARACTER::GetEmptyDragonSoulInventory(LPITEM pItem) const
{
	if (NULL == pItem || !pItem->IsDragonSoul())
		return -1;
	if (!DragonSoul_IsQualified())
	{
		return -1;
	}
	BYTE bSize = pItem->GetSize();
	WORD wBaseCell = DSManager::instance().GetBasePosition(pItem);

	if (WORD_MAX == wBaseCell)
		return -1;

	for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; ++i)
		if (IsEmptyItemGrid(TItemPos(DRAGON_SOUL_INVENTORY, i + wBaseCell), bSize))
			return i + wBaseCell;

	return -1;
}

#ifdef WJ_SPLIT_INVENTORY_SYSTEM
int CHARACTER::GetEmptySkillBookInventory(BYTE size) const
{
	for (int i = SKILL_BOOK_INVENTORY_SLOT_START; i < SKILL_BOOK_INVENTORY_SLOT_END; ++i)
		if (IsEmptyItemGrid(TItemPos(INVENTORY, i), size))
			return i;
		
	return -1;
}

int CHARACTER::GetEmptyUpgradeItemsInventory(BYTE size) const
{
	for (int i = UPGRADE_ITEMS_INVENTORY_SLOT_START; i < UPGRADE_ITEMS_INVENTORY_SLOT_END; ++i)
		if (IsEmptyItemGrid(TItemPos(INVENTORY, i), size))
			return i;
		
	return -1;	
}
#endif

void CHARACTER::CopyDragonSoulItemGrid(std::vector<WORD>& vDragonSoulItemGrid) const
{
	vDragonSoulItemGrid.resize(DRAGON_SOUL_INVENTORY_MAX_NUM);

	std::copy(m_pointsInstant.wDSItemGrid, m_pointsInstant.wDSItemGrid + DRAGON_SOUL_INVENTORY_MAX_NUM, vDragonSoulItemGrid.begin());
}

int CHARACTER::CountEmptyInventory() const
{
	int	count = 0;
#ifdef WJ_INVENTORY_EX_SYSTEM
	for (int i = 0; i < GetInventoryEx(); ++i)
		if (GetInventoryItem(i))
			count += GetInventoryItem(i)->GetSize();

	return (GetInventoryEx() - count);
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
		if (GetInventoryItem(i))
			count += GetInventoryItem(i)->GetSize();

	return (INVENTORY_MAX_NUM - count);
#endif
}

void TransformRefineItem(LPITEM pkOldItem, LPITEM pkNewItem)
{
	// ACCESSORY_REFINE
	if (pkOldItem->IsAccessoryForSocket())
	{
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			pkNewItem->SetSocket(i, pkOldItem->GetSocket(i));
		}
		//pkNewItem->StartAccessorySocketExpireEvent();
	}
	// END_OF_ACCESSORY_REFINE
	else
	{
		// 여기서 깨진석이 자동적으로 청소 됨
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			if (!pkOldItem->GetSocket(i))
				break;
			else
				pkNewItem->SetSocket(i, 1);
		}

		// 소켓 설정
		int slot = 0;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			long socket = pkOldItem->GetSocket(i);

			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				pkNewItem->SetSocket(slot++, socket);
		}

	}

	// 매직 아이템 설정
	pkOldItem->CopyAttributeTo(pkNewItem);
	pkNewItem->SetTransmutation(pkOldItem->GetTransmutation());
}

void NotifyRefineSuccess(LPCHARACTER ch, LPITEM item, const char* way)
{
	if (NULL != ch && item != NULL)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "RefineSuceeded");
#ifdef ENABLE_SMITH_EFFECT
		ch->EffectPacket(SE_SUCCESS);
#endif

		LogManager::instance().RefineLog(ch->GetPlayerID(), item->GetName(), item->GetID(), item->GetRefineLevel(), 1, way);
	}
}

void NotifyRefineFail(LPCHARACTER ch, LPITEM item, const char* way, int success = 0)
{
	if (NULL != ch && NULL != item)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "RefineFailed");
#ifdef ENABLE_SMITH_EFFECT
		ch->EffectPacket(SE_FAIL);
#endif
		
		LogManager::instance().RefineLog(ch->GetPlayerID(), item->GetName(), item->GetID(), item->GetRefineLevel(), success, way);
	}
}

void CHARACTER::SetRefineNPC(LPCHARACTER ch)
{
	if ( ch != NULL )
	{
		m_dwRefineNPCVID = ch->GetVID();
	}
	else
	{
		m_dwRefineNPCVID = 0;
	}
}

bool CHARACTER::DoRefine(LPITEM item, bool bMoneyOnly)
{
	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}
	
	//개량 시간제한 : upgrade_refine_scroll.quest 에서 개량후 5분이내에 일반 개량을 
	//진행할수 없음
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			sys_log(0, "can't refine %d %s", GetPlayerID(), GetName());
			return false;
		}
	}

	const TRefineTable * prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());

	if (!prt)
		return false;

	DWORD result_vnum = item->GetRefinedVnum();

	// REFINE_COST
	int cost = ComputeRefineFee(prt->cost);

	int RefineChance = GetQuestFlag("main_quest_lv7.refine_chance");

	if (RefineChance > 0)
	{
		if (!item->CheckItemUseLevel(20) || item->GetType() != ITEM_WEAPON)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("무료 개량 기회는 20 이하의 무기만 가능합니다"));
			return false;
		}

		cost = 0;
		SetQuestFlag("main_quest_lv7.refine_chance", RefineChance - 1);
	}
	// END_OF_REFINE_COST

	if (result_vnum == 0)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("더 이상 개량할 수 없습니다."));
		return false;
	}

	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_TUNING)
		return false;

	TItemTable * pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
		sys_err("DoRefine NOT GET ITEM PROTO %d", item->GetRefinedVnum());
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템은 개량할 수 없습니다."));
		return false;
	}

	// REFINE_COST
	if (GetGold() < cost)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("개량을 하기 위한 돈이 부족합니다."));
		return false;
	}

	if (!bMoneyOnly && !RefineChance)
	{
		for (int i = 0; i < prt->material_count; ++i)
		{
			if (CountSpecifyItem(prt->materials[i].vnum) < prt->materials[i].count)
			{
				if (test_server)
				{
					ChatPacket(CHAT_TYPE_INFO, "Find %d, count %d, require %d", prt->materials[i].vnum, CountSpecifyItem(prt->materials[i].vnum), prt->materials[i].count);
				}
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("개량을 하기 위한 재료가 부족합니다."));
				return false;
			}
		}

		for (int i = 0; i < prt->material_count; ++i)
			RemoveSpecifyItem(prt->materials[i].vnum, prt->materials[i].count);
	}

	int prob = number(1, 100);

	if (IsRefineThroughGuild() || bMoneyOnly)
		prob -= 10;

	// END_OF_REFINE_COST

	if (prob <= prt->prob)
	{
		// 성공! 모든 아이템이 사라지고, 같은 속성의 다른 아이템 획득
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE SUCCESS", pkNewItem->GetName());

			UINT bCell = item->GetCell();

			// DETAIL_REFINE_LOG
			NotifyRefineSuccess(this, item, IsRefineThroughGuild() ? "GUILD" : "POWER");
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");
			// END_OF_DETAIL_REFINE_LOG

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell)); 
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

			sys_log(0, "Refine Success %d", cost);
			pkNewItem->AttrLog();
			//PointChange(POINT_GOLD, -cost);
			sys_log(0, "PayPee %d", cost);
			PayRefineFee(cost);
			sys_log(0, "PayPee End %d", cost);
		}
		else
		{
			// DETAIL_REFINE_LOG
			// 아이템 생성에 실패 -> 개량 실패로 간주
			sys_err("cannot create item %u", result_vnum);
			NotifyRefineFail(this, item, IsRefineThroughGuild() ? "GUILD" : "POWER");
			// END_OF_DETAIL_REFINE_LOG
		}
	}
	else
	{
		// 실패! 모든 아이템이 사라짐.
		DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -cost);
		NotifyRefineFail(this, item, IsRefineThroughGuild() ? "GUILD" : "POWER");
		item->AttrLog();
		ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

		//PointChange(POINT_GOLD, -cost);
		PayRefineFee(cost);
	}

	return true;
}

enum enum_RefineScrolls
{
	CHUKBOK_SCROLL = 0,
	HYUNIRON_CHN   = 1, // 중국에서만 사용
	YONGSIN_SCROLL = 2,
	MUSIN_SCROLL   = 3,
	YAGONG_SCROLL  = 4,
	MEMO_SCROLL	   = 5,
	BDRAGON_SCROLL	= 6,
};

bool CHARACTER::DoRefineWithScroll(LPITEM item)
{
	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	ClearRefineMode();

	//개량 시간제한 : upgrade_refine_scroll.quest 에서 개량후 5분이내에 일반 개량을 
	//진행할수 없음
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			sys_log(0, "can't refine %d %s", GetPlayerID(), GetName());
			return false;
		}
	}

	const TRefineTable * prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());

	if (!prt)
		return false;

	LPITEM pkItemScroll;

	// 개량서 체크
	if (m_iRefineAdditionalCell < 0)
		return false;

	pkItemScroll = GetInventoryItem(m_iRefineAdditionalCell);

	if (!pkItemScroll)
		return false;

	if (!(pkItemScroll->GetType() == ITEM_USE && pkItemScroll->GetSubType() == USE_TUNING))
		return false;

	if (pkItemScroll->GetVnum() == item->GetVnum())
		return false;

	DWORD result_vnum = item->GetRefinedVnum();
	DWORD result_fail_vnum = item->GetRefineFromVnum();

	if (result_vnum == 0)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("더 이상 개량할 수 없습니다."));
		return false;
	}

	// MUSIN_SCROLL
	if (pkItemScroll->GetValue(0) == MUSIN_SCROLL)
	{
		if (item->GetRefineLevel() >= 4)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 개량서로 더 이상 개량할 수 없습니다."));
			return false;
		}
	}
	// END_OF_MUSIC_SCROLL

	else if (pkItemScroll->GetValue(0) == MEMO_SCROLL)
	{
		if (item->GetRefineLevel() != pkItemScroll->GetValue(1))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 개량서로 개량할 수 없습니다."));
			return false;
		}
	}
	else if (pkItemScroll->GetValue(0) == BDRAGON_SCROLL)
	{
		if (item->GetType() != ITEM_METIN || item->GetRefineLevel() != 4)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템으로 개량할 수 없습니다."));
			return false;
		}
	}

	TItemTable * pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
		sys_err("DoRefineWithScroll NOT GET ITEM PROTO %d", item->GetRefinedVnum());
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템은 개량할 수 없습니다."));
		return false;
	}

	if (GetGold() < prt->cost)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("개량을 하기 위한 돈이 부족합니다."));
		return false;
	}

	for (int i = 0; i < prt->material_count; ++i)
	{
		if (CountSpecifyItem(prt->materials[i].vnum) < prt->materials[i].count)
		{
			if (test_server)
			{
				ChatPacket(CHAT_TYPE_INFO, "Find %d, count %d, require %d", prt->materials[i].vnum, CountSpecifyItem(prt->materials[i].vnum), prt->materials[i].count);
			}
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("개량을 하기 위한 재료가 부족합니다."));
			return false;
		}
	}

	for (int i = 0; i < prt->material_count; ++i)
		RemoveSpecifyItem(prt->materials[i].vnum, prt->materials[i].count);

	int prob = number(1, 100);
	int success_prob = prt->prob;
	bool bDestroyWhenFail = false;

	const char* szRefineType = "SCROLL";

	if (pkItemScroll->GetValue(0) == HYUNIRON_CHN || 
		pkItemScroll->GetValue(0) == YONGSIN_SCROLL || 
		pkItemScroll->GetValue(0) == YAGONG_SCROLL) // 현철, 용신의 축복서, 야공의 비전서  처리
	{
		const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };

		const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

		if (pkItemScroll->GetValue(0) == YONGSIN_SCROLL)
		{
			success_prob = hyuniron_prob[MINMAX(0, item->GetRefineLevel(), 8)];
		}
		else if (pkItemScroll->GetValue(0) == YAGONG_SCROLL)
		{
			success_prob = yagong_prob[MINMAX(0, item->GetRefineLevel(), 8)];
		}
		else if (pkItemScroll->GetValue(0) == HYUNIRON_CHN) {} // @fixme13
		else
		{
			sys_err("REFINE : Unknown refine scroll item. Value0: %d", pkItemScroll->GetValue(0));
		}

		if (test_server) 
		{
			ChatPacket(CHAT_TYPE_INFO, "[Only Test] Success_Prob %d, RefineLevel %d ", success_prob, item->GetRefineLevel());
		}
		if (pkItemScroll->GetValue(0) == HYUNIRON_CHN) // 현철은 아이템이 부서져야 한다.
			bDestroyWhenFail = true;

		// DETAIL_REFINE_LOG
		if (pkItemScroll->GetValue(0) == HYUNIRON_CHN)
		{
			szRefineType = "HYUNIRON";
		}
		else if (pkItemScroll->GetValue(0) == YONGSIN_SCROLL)
		{
			szRefineType = "GOD_SCROLL";
		}
		else if (pkItemScroll->GetValue(0) == YAGONG_SCROLL)
		{
			szRefineType = "YAGONG_SCROLL";
		}
		// END_OF_DETAIL_REFINE_LOG
	}

	// DETAIL_REFINE_LOG
	if (pkItemScroll->GetValue(0) == MUSIN_SCROLL) // 무신의 축복서는 100% 성공 (+4까지만)
	{
		success_prob = 100;

		szRefineType = "MUSIN_SCROLL";
	}
	// END_OF_DETAIL_REFINE_LOG
	else if (pkItemScroll->GetValue(0) == MEMO_SCROLL)
	{
		success_prob = 100;
		szRefineType = "MEMO_SCROLL";
	}
	else if (pkItemScroll->GetValue(0) == BDRAGON_SCROLL)
	{
		success_prob = 80;
		szRefineType = "BDRAGON_SCROLL";
	}

	pkItemScroll->SetCount(pkItemScroll->GetCount() - 1);

	if (prob <= success_prob)
	{
		// 성공! 모든 아이템이 사라지고, 같은 속성의 다른 아이템 획득
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE SUCCESS", pkNewItem->GetName());

			UINT bCell = item->GetCell();

			NotifyRefineSuccess(this, item, szRefineType);
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell)); 
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);
			pkNewItem->AttrLog();
			//PointChange(POINT_GOLD, -prt->cost);
			PayRefineFee(prt->cost);
		}
		else
		{
			// 아이템 생성에 실패 -> 개량 실패로 간주
			sys_err("cannot create item %u", result_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}
	}
	else if (!bDestroyWhenFail && result_fail_vnum)
	{
		// 실패! 모든 아이템이 사라지고, 같은 속성의 낮은 등급의 아이템 획득
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_fail_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE FAIL", pkNewItem->GetName());

			UINT bCell = item->GetCell();

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
			NotifyRefineFail(this, item, szRefineType, -1);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell)); 
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

			pkNewItem->AttrLog();

			//PointChange(POINT_GOLD, -prt->cost);
			PayRefineFee(prt->cost);
		}
		else
		{
			// 아이템 생성에 실패 -> 개량 실패로 간주
			sys_err("cannot create item %u", result_fail_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}
	}
	else
	{
		NotifyRefineFail(this, item, szRefineType); // 개량시 아이템 사라지지 않음
		
		PayRefineFee(prt->cost);
	}

	return true;
}

bool CHARACTER::RefineInformation(UINT bCell, BYTE bType, int iAdditionalCell)
{
#ifdef WJ_INVENTORY_EX_SYSTEM
	if (bCell > GetInventoryEx())
#else
	if (bCell > INVENTORY_MAX_NUM)
#endif
		return false;

	LPITEM item = GetInventoryItem(bCell);

	if (!item)
		return false;

	// REFINE_COST
	if (bType == REFINE_TYPE_MONEY_ONLY && !GetQuestFlag("deviltower_zone.can_refine"))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("사귀 타워 완료 보상은 한번까지 사용가능합니다."));
		return false;
	}
	// END_OF_REFINE_COST

	TPacketGCRefineInformation p;

	p.header = HEADER_GC_REFINE_INFORMATION;
	p.pos = bCell;
	p.src_vnum = item->GetVnum();
	p.result_vnum = item->GetRefinedVnum();
	p.type = bType;

	if (p.result_vnum == 0)
	{
		sys_err("RefineInformation p.result_vnum == 0");
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템은 개량할 수 없습니다."));
		return false;
	}

	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_TUNING)
	{
		if (bType == 0)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템은 이 방식으로는 개량할 수 없습니다."));
			return false;
		}
		else
		{
			LPITEM itemScroll = GetInventoryItem(iAdditionalCell);
			if (!itemScroll || item->GetVnum() == itemScroll->GetVnum())
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("같은 개량서를 합칠 수는 없습니다."));
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("축복의 서와 현철을 합칠 수 있습니다."));
				return false;
			}
		}
	}

	CRefineManager & rm = CRefineManager::instance();

	const TRefineTable* prt = rm.GetRefineRecipe(item->GetRefineSet());

	if (!prt)
	{
		sys_err("RefineInformation NOT GET REFINE SET %d", item->GetRefineSet());
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템은 개량할 수 없습니다."));
		return false;
	}

	// REFINE_COST
	
	//MAIN_QUEST_LV7
	if (GetQuestFlag("main_quest_lv7.refine_chance") > 0)
	{
		// 일본은 제외
		if (!item->CheckItemUseLevel(20) || item->GetType() != ITEM_WEAPON)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("무료 개량 기회는 20 이하의 무기만 가능합니다"));
			return false;
		}
		p.cost = 0;
	}
	else
		p.cost = ComputeRefineFee(prt->cost);
	
	//END_MAIN_QUEST_LV7
	p.prob = prt->prob;
	if (bType == REFINE_TYPE_MONEY_ONLY)
	{
		p.material_count = 0;
		memset(p.materials, 0, sizeof(p.materials));
	}
	else
	{
		p.material_count = prt->material_count;
		thecore_memcpy(&p.materials, prt->materials, sizeof(prt->materials));
	}
	// END_OF_REFINE_COST

	GetDesc()->Packet(&p, sizeof(TPacketGCRefineInformation));

	SetRefineMode(iAdditionalCell);
	return true;
}

bool CHARACTER::RefineItem(LPITEM pkItem, LPITEM pkTarget)
{
	if (!CanHandleItem())
		return false;

#ifdef WJ_SOULBINDING_SYSTEM
	if (pkTarget->IsBind())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't do this because item is binden!"));
		return false;
	}
	
	if (pkTarget->IsUntilBind())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't do this because item is binden!"));
		return false;
	}
#endif

	if (pkItem->GetSubType() == USE_TUNING)
	{
		// XXX 성능, 소켓 개량서는 사라졌습니다...
		// XXX 성능개량서는 축복의 서가 되었다!
		// MUSIN_SCROLL
		if (pkItem->GetValue(0) == MUSIN_SCROLL)
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_MUSIN, pkItem->GetCell());
		// END_OF_MUSIN_SCROLL
		else if (pkItem->GetValue(0) == HYUNIRON_CHN)
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_HYUNIRON, pkItem->GetCell());
		else if (pkItem->GetValue(0) == BDRAGON_SCROLL)
		{
			if (pkTarget->GetRefineSet() != 702) return false;
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_BDRAGON, pkItem->GetCell());
		}
		else
		{
			if (pkTarget->GetRefineSet() == 501) return false;
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_SCROLL, pkItem->GetCell());
		}
	}
	else if (pkItem->GetSubType() == USE_DETACHMENT && IS_SET(pkTarget->GetFlag(), ITEM_FLAG_REFINEABLE))
	{
		LogManager::instance().ItemLog(this, pkTarget, "USE_DETACHMENT", pkTarget->GetName());

		bool bHasMetinStone = false;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
		{
			long socket = pkTarget->GetSocket(i);
			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
			{
				bHasMetinStone = true;
				break;
			}
		}

		if (bHasMetinStone)
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			{
				long socket = pkTarget->GetSocket(i);
				if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				{
					AutoGiveItem(socket);
					//TItemTable* pTable = ITEM_MANAGER::instance().GetTable(pkTarget->GetSocket(i));
					//pkTarget->SetSocket(i, pTable->alValues[2]);
					// 깨진돌로 대체해준다
					pkTarget->SetSocket(i, ITEM_BROKEN_METIN_VNUM);
				}
			}
			pkItem->SetCount(pkItem->GetCount() - 1);
			return true;
		}
		else
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("빼낼 수 있는 메틴석이 없습니다."));
			return false;
		}
	}

	return false;
}

EVENTFUNC(kill_campfire_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == NULL )
	{
		sys_err( "kill_campfire_event> <Factor> Null pointer" );
		return 0;
	}

	LPCHARACTER	ch = info->ch;

	if (ch == NULL) { // <Factor>
		return 0;
	}
	ch->m_pkMiningEvent = NULL;
	M2_DESTROY_CHARACTER(ch);
	return 0;
}

bool CHARACTER::GiveRecallItem(LPITEM item)
{
	int idx = GetMapIndex();
	int iEmpireByMapIndex = -1;

	if (idx < 20)
		iEmpireByMapIndex = 1;
	else if (idx < 40)
		iEmpireByMapIndex = 2;
	else if (idx < 60)
		iEmpireByMapIndex = 3;
	else if (idx < 10000)
		iEmpireByMapIndex = 0;

	switch (idx)
	{
		case 66:
		case 216:
			iEmpireByMapIndex = -1;
			break;
	}

	if (iEmpireByMapIndex && GetEmpire() != iEmpireByMapIndex)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("기억해 둘 수 없는 위치 입니다."));
		return false;
	}

	int pos;

	if (item->GetCount() == 1)	// 아이템이 하나라면 그냥 셋팅.
	{
		item->SetSocket(0, GetX());
		item->SetSocket(1, GetY());
	}
	else if ((pos = GetEmptyInventory(item->GetSize())) != -1) // 그렇지 않다면 다른 인벤토리 슬롯을 찾는다.
	{
		LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), 1);

		if (NULL != item2)
		{
			item2->SetSocket(0, GetX());
			item2->SetSocket(1, GetY());
			item2->AddToCharacter(this, TItemPos(INVENTORY, pos));

			item->SetCount(item->GetCount() - 1);
		}
	}
	else
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지품에 빈 공간이 없습니다."));
		return false;
	}

	return true;
}

void CHARACTER::ProcessRecallItem(LPITEM item)
{
	int idx;

	if ((idx = SECTREE_MANAGER::instance().GetMapIndex(item->GetSocket(0), item->GetSocket(1))) == 0)
		return;

	int iEmpireByMapIndex = -1;

	if (idx < 20)
		iEmpireByMapIndex = 1;
	else if (idx < 40)
		iEmpireByMapIndex = 2;
	else if (idx < 60)
		iEmpireByMapIndex = 3;
	else if (idx < 10000)
		iEmpireByMapIndex = 0;

	switch (idx)
	{
		case 66:
		case 216:
			iEmpireByMapIndex = -1;
			break;
		// 악룡군도 일때
		case 301:
		case 302:
		case 303:
		case 304:
			if( GetLevel() < 90 )
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템의 레벨 제한보다 레벨이 낮습니다."));
				return;
			}
			else
				break;
	}

	if (iEmpireByMapIndex && GetEmpire() != iEmpireByMapIndex)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("기억된 위치가 타제국에 속해 있어서 귀환할 수 없습니다."));
		item->SetSocket(0, 0);
		item->SetSocket(1, 0);
	}
	else
	{
		sys_log(1, "Recall: %s %d %d -> %d %d", GetName(), GetX(), GetY(), item->GetSocket(0), item->GetSocket(1));
		WarpSet(item->GetSocket(0), item->GetSocket(1));
		item->SetCount(item->GetCount() - 1);
	}
}

void CHARACTER::__OpenPrivateShop()
{
	unsigned bodyPart = GetPart(PART_MAIN);
	switch (bodyPart)
	{
		case 0:
		case 1:
		case 2:
			ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShop");
			break;
		default:
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("갑옷을 벗어야 개인 상점을 열 수 있습니다."));
			break;
	}
}

#ifdef WJ_OFFLINESHOP_SYSTEM
void CHARACTER::__OpenOfflineShop()
{
	if (IsDead())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_YOU_DEAD"));
		return;
	}

	if (GetExchange())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_TRADE_WINDOW"));
		return;
	}

	if (GetMyShop())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_HAVE_SHOP"));
		return;
	}

	if (GetOfflineShop())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_SHOP_WINDOW"));
		return;
	}

	if (IsCubeOpen())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_CUBE_WINDOW"));
		return;
	}

	if (IsOpenSafebox())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_SAFEBOX_AKTIF"));
		return;
	}

	if (GetShop())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_DUKKAN_AKTIF"));
		return;
	}
	
	ChatPacket(CHAT_TYPE_COMMAND, "OpenOfflineShop");
}
#endif

// MYSHOP_PRICE_LIST
#ifdef WJ_CHEQUE_SYSTEM
void CHARACTER::SendMyShopPriceListCmd(DWORD dwItemVnum, TItemPriceType ItemPrice)
#else
void CHARACTER::SendMyShopPriceListCmd(DWORD dwItemVnum, DWORD dwItemPrice)
#endif
{
	char szLine[256];
#ifdef WJ_CHEQUE_SYSTEM
	snprintf(szLine, sizeof(szLine), "MyShopPriceListNew %u %u %u", dwItemVnum, ItemPrice.dwPrice, ItemPrice.byChequePrice);
#else
	snprintf(szLine, sizeof(szLine), "MyShopPriceList %u %u", dwItemVnum, dwItemPrice);
#endif
	ChatPacket(CHAT_TYPE_COMMAND, szLine);
	sys_log(0, szLine);
}

//
// DB 캐시로 부터 받은 리스트를 User 에게 전송하고 상점을 열라는 커맨드를 보낸다.
//
void CHARACTER::UseSilkBotaryReal(const TPacketMyshopPricelistHeader* p)
{
	const TItemPriceInfo* pInfo = (const TItemPriceInfo*)(p + 1);

	if (!p->byCount)
		// 가격 리스트가 없다. dummy 데이터를 넣은 커맨드를 보내준다.
#ifdef WJ_CHEQUE_SYSTEM
		SendMyShopPriceListCmd(1, TItemPriceType());
#else
		SendMyShopPriceListCmd(1, 0);
#endif
	else {
		for (int idx = 0; idx < p->byCount; idx++)
#ifdef WJ_CHEQUE_SYSTEM
			SendMyShopPriceListCmd(pInfo[ idx ].dwVnum, TItemPriceType(pInfo[ idx ].price.dwPrice, pInfo[ idx ].price.byChequePrice));
#else
			SendMyShopPriceListCmd(pInfo[ idx ].dwVnum, pInfo[ idx ].dwPrice);
#endif
	}

#ifdef WJ_OFFLINESHOP_SYSTEM
	__OpenOfflineShop();
#else
	__OpenPrivateShop();
#endif
}

//
// 이번 접속 후 처음 상점을 Open 하는 경우 리스트를 Load 하기 위해 DB 캐시에 가격정보 리스트 요청 패킷을 보낸다.
// 이후부터는 바로 상점을 열라는 응답을 보낸다.
//
void CHARACTER::UseSilkBotary(void)
{
	if (m_bNoOpenedShop)
	{
		DWORD dwPlayerID = GetPlayerID();
		db_clientdesc->DBPacket(HEADER_GD_MYSHOP_PRICELIST_REQ, GetDesc()->GetHandle(), &dwPlayerID, sizeof(DWORD));
		m_bNoOpenedShop = false;
	}
	else
	{
#ifdef WJ_OFFLINESHOP_SYSTEM
		__OpenOfflineShop();
#else
		__OpenPrivateShop();
#endif
	}
}
// END_OF_MYSHOP_PRICE_LIST


int CalculateConsume(LPCHARACTER ch)
{
	static const int WARP_NEED_LIFE_PERCENT	= 30;
	static const int WARP_MIN_LIFE_PERCENT	= 10;
	// CONSUME_LIFE_WHEN_USE_WARP_ITEM
	int consumeLife = 0;
	{
		// CheckNeedLifeForWarp
		const int curLife		= ch->GetHP();
		const int needPercent	= WARP_NEED_LIFE_PERCENT;
		const int needLife = ch->GetMaxHP() * needPercent / 100;
		if (curLife < needLife)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("남은 생명력 양이 모자라 사용할 수 없습니다."));
			return -1;
		}

		consumeLife = needLife;


		// CheckMinLifeForWarp: 독에 의해서 죽으면 안되므로 생명력 최소량는 남겨준다
		const int minPercent	= WARP_MIN_LIFE_PERCENT;
		const int minLife	= ch->GetMaxHP() * minPercent / 100;
		if (curLife - needLife < minLife)
			consumeLife = curLife - minLife;

		if (consumeLife < 0)
			consumeLife = 0;
	}
	// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM
	return consumeLife;
}

int CalculateConsumeSP(LPCHARACTER lpChar)
{
	static const int NEED_WARP_SP_PERCENT = 30;

	const int curSP = lpChar->GetSP();
	const int needSP = lpChar->GetMaxSP() * NEED_WARP_SP_PERCENT / 100;

	if (curSP < needSP)
	{
		lpChar->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("남은 정신력 양이 모자라 사용할 수 없습니다."));
		return -1;
	}

	return needSP;
}

bool CHARACTER::UseItemEx(LPITEM item, TItemPos DestCell)
{
	int iLimitRealtimeStartFirstUseFlagIndex = -1;
	// int iLimitTimerBasedOnWearFlagIndex = -1;

	WORD wDestCell = DestCell.cell;
	BYTE bDestInven = DestCell.window_type;
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		long limitValue = item->GetProto()->aLimits[i].lValue;

		switch (item->GetProto()->aLimits[i].bType)
		{
			case LIMIT_LEVEL:
				if (GetLevel() < limitValue)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템의 레벨 제한보다 레벨이 낮습니다."));
					return false;
				}
				break;

			case LIMIT_REAL_TIME_START_FIRST_USE:
				iLimitRealtimeStartFirstUseFlagIndex = i;
				break;

			case LIMIT_TIMER_BASED_ON_WEAR:
				// iLimitTimerBasedOnWearFlagIndex = i;
				break;
		}
	}

	if (test_server)
	{
		sys_log(0, "USE_ITEM %s, Inven %d, Cell %d, ItemType %d, SubType %d", item->GetName(), bDestInven, wDestCell, item->GetType(), item->GetSubType());
	}

	if ( CArenaManager::instance().IsLimitedItem( GetMapIndex(), item->GetVnum() ) == true )
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련 중에는 이용할 수 없는 물품입니다."));
		return false;
	}
	
	// @fixme3
	if (!IsLoadedAffect())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("AFFECTS_NOT_LOADED"));
		return false;
	}
	
	// @fixme BEGIN
	if (TItemPos(item->GetWindow(), item->GetCell()).IsBeltInventoryPosition())
	{
		LPITEM beltItem = GetWear(WEAR_BELT);

		if (NULL == beltItem)
		{
			ChatPacket(CHAT_TYPE_INFO, "<Belt> You can't use this item if you have no equipped belt.");
			return false;
		}

		if (false == CBeltInventoryHelper::IsAvailableCell(item->GetCell() - BELT_INVENTORY_SLOT_START, beltItem->GetValue(0)))
		{
			ChatPacket(CHAT_TYPE_INFO, "<Belt> You can't use this item if you don't upgrade your belt.");
			return false;
		}
	}
	// @fixme END

	// 아이템 최초 사용 이후부터는 사용하지 않아도 시간이 차감되는 방식 처리. 
	if (-1 != iLimitRealtimeStartFirstUseFlagIndex)
	{
		// 한 번이라도 사용한 아이템인지 여부는 Socket1을 보고 판단한다. (Socket1에 사용횟수 기록)
		if (0 == item->GetSocket(1))
		{
			// 사용가능시간은 Default 값으로 Limit Value 값을 사용하되, Socket0에 값이 있으면 그 값을 사용하도록 한다. (단위는 초)
			long duration = (0 != item->GetSocket(0)) ? item->GetSocket(0) : item->GetProto()->aLimits[iLimitRealtimeStartFirstUseFlagIndex].lValue;

			if (0 == duration)
				duration = 60 * 60 * 24 * 7;

			item->SetSocket(0, time(0) + duration);
			item->StartRealTimeExpireEvent();
		}	

		if (false == item->IsEquipped())
			item->SetSocket(1, item->GetSocket(1) + 1);		
	}
	
#ifdef WJ_GROWTH_PET_SYSTEM
	if (item->GetVnum() == 55001)
	{
		LPITEM item2;

		if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
			return false;

		if (item2->IsExchanging())
			return false;

		if (item2->GetVnum() > 55706 || item2->GetVnum() < 55701)
			return false;

		char szQuery1[1024];
		snprintf(szQuery1, sizeof(szQuery1), "SELECT duration,tduration FROM new_petsystem WHERE id = %lu ", item2->GetID());
		std::unique_ptr<SQLMsg> pmsg2(DBManager::instance().DirectQuery(szQuery1));
		if (pmsg2->Get()->uiNumRows > 0) 
		{
			MYSQL_ROW row = mysql_fetch_row(pmsg2->Get()->pSQLResult);
			int suankiDuration=time(0)-(atoi(row[1])-atoi(row[0]));
			if(suankiDuration < 1)
			{
				ChatPacket(CHAT_TYPE_INFO,LC_TEXT("PET_ALREADY_YOUNG"));
				return false;
			}
			
			int insertduration0 = time(0)+atoi(row[1]);
			int insertduration1 = time(0)+(atoi(row[2])/2);
			if (atoi(row[0]) > 0)
			{
				DBManager::instance().DirectQuery("UPDATE new_petsystem SET duration =%d WHERE id = %d", insertduration0, item2->GetID());
				ChatPacket(CHAT_TYPE_INFO,LC_TEXT("PSS_PROTEIN_D1"));
			}
			else
			{
				DBManager::instance().DirectQuery("UPDATE new_petsystem SET duration =%d WHERE id = %d", insertduration1, item2->GetID());
				ChatPacket(CHAT_TYPE_INFO,LC_TEXT("PSS_PROTEIN_D2"));
			}
			
			item->SetCount(item->GetCount() - 1);
			return true;
		}
		else
			return false;
	}

	if (item->GetVnum() >= 55701 && item->GetVnum() <= 55706)
	{
		LPITEM item2;
		if (item2 = GetItem(DestCell))
		{
			if (item2->GetVnum() == 55002)
			{
				if(item2->GetAttributeValue(0) > 0)
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("PET_BOX_IN_HAVE_PET"));
				else
				{
					if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
						return false;

					if (item2->IsExchanging())
						return false;

					if (GetNewPetSystem()->IsActivePet())
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("PET_FIRST_UNSUMMON_YOUR_PET"));
						return false;
					}
					
					if (item2->GetSocket(2) != 0)
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("PET_BOX_IN_HAVE_PET"));
						return false;
					}
					
					if (item->GetSocket(1) > GetLevel())
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("PET_LEVEL_MY_LEVEL"));
						return false;
					}
					
					for (int b = 0; b < 3; b++)
						item2->SetForceAttribute(b, 1, item->GetAttributeValue(b));
					item2->SetForceAttribute(4, 1, item->GetAttributeValue(4));
					item2->SetForceAttribute(6, 1, item->GetAttributeValue(6));
					item2->SetForceAttribute(7, 1, item->GetAttributeValue(7));
					item2->SetForceAttribute(8, 1, item->GetAttributeValue(8));
					item2->SetForceAttribute(9, 1, item->GetAttributeValue(9));
					item2->SetForceAttribute(10, 1, item->GetAttributeValue(10));
					item2->SetForceAttribute(11, 1, item->GetAttributeValue(11));
					item2->SetForceAttribute(12, 1, item->GetAttributeValue(12));
					DWORD vnum1 = item->GetVnum()-55700;
					item2->SetSocket(2, vnum1);
					item2->SetSocket(1, item->GetSocket(1));
					item2->SetSocket(3, item->GetSocket(3));
					item2->SetSocket(4, item->GetSocket(4));
					DBManager::instance().DirectQuery("UPDATE new_petsystem SET id =%d WHERE id = %d", item2->GetID(), item->GetID());
					ITEM_MANAGER::instance().RemoveItem(item);
					
					return true;
				}
			}
		}
	}

	if (item->GetVnum() == 55002 && item->GetAttributeValue(0) > 0)
	{
		int pos = GetEmptyInventory(item->GetSize());
		if(pos == -1)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("PET_NOT_ENOUGH_SPACE"));
			return false;
		}

		if (item->IsExchanging())
			return false;
		
		DWORD vnum2 = 55700+item->GetSocket(2);
		LPITEM item2 = AutoGiveItem(vnum2, 1);
		for (int b = 0; b < 3; b++)
			item2->SetForceAttribute(b, 1, item->GetAttributeValue(b));
		item2->SetForceAttribute(4, 1, item->GetAttributeValue(4));
		item2->SetForceAttribute(6, 1, item->GetAttributeValue(6));
		item2->SetForceAttribute(7, 1, item->GetAttributeValue(7));
		item2->SetForceAttribute(8, 1, item->GetAttributeValue(8));
		item2->SetForceAttribute(9, 1, item->GetAttributeValue(9));
		item2->SetForceAttribute(10, 1, item->GetAttributeValue(10));
		item2->SetForceAttribute(11, 1, item->GetAttributeValue(11));
		item2->SetForceAttribute(12, 1, item->GetAttributeValue(12));
		item2->SetSocket(1,item->GetSocket(1));
		item2->SetSocket(3,item->GetSocket(3));
		item2->SetSocket(4,item->GetSocket(4));
		DBManager::instance().DirectQuery("UPDATE new_petsystem SET id =%d WHERE id = %d", item2->GetID(), item->GetID());
		ITEM_MANAGER::instance().RemoveItem(item);
		
		return true;
	}
#endif

	switch (item->GetType())
	{
		case ITEM_HAIR:
			return ItemProcess_Hair(item, wDestCell);

		case ITEM_POLYMORPH:
			return ItemProcess_Polymorph(item);

		case ITEM_QUEST:
			if (GetArena() != NULL || IsObserverMode() == true)
			{
				if (item->GetVnum() == 50051 || item->GetVnum() == 50052 || item->GetVnum() == 50053)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련 중에는 이용할 수 없는 물품입니다."));
					return false;
				}
			}

			if (!IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE))
			{
				if (item->GetSIGVnum() == 0)
				{
					quest::CQuestManager::instance().UseItem(GetPlayerID(), item, false);
				}
				else
				{
					quest::CQuestManager::instance().SIGUse(GetPlayerID(), item->GetSIGVnum(), item, false);
				}
			}
			break;

		case ITEM_CAMPFIRE:
			{
				float fx, fy;
				GetDeltaByDegree(GetRotation(), 100.0f, &fx, &fy);

				LPSECTREE tree = SECTREE_MANAGER::instance().Get(GetMapIndex(), (long)(GetX()+fx), (long)(GetY()+fy));

				if (!tree)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("모닥불을 피울 수 없는 지점입니다."));
					return false;
				}

				if (tree->IsAttr((long)(GetX()+fx), (long)(GetY()+fy), ATTR_WATER))
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("물 속에 모닥불을 피울 수 없습니다."));
					return false;
				}

				LPCHARACTER campfire = CHARACTER_MANAGER::instance().SpawnMob(fishing::CAMPFIRE_MOB, GetMapIndex(), (long)(GetX()+fx), (long)(GetY()+fy), 0, false, number(0, 359));

				char_event_info* info = AllocEventInfo<char_event_info>();

				info->ch = campfire;

				campfire->m_pkMiningEvent = event_create(kill_campfire_event, info, PASSES_PER_SEC(40));

				item->SetCount(item->GetCount() - 1);
			}
			break;

		case ITEM_UNIQUE:
			{
				switch (item->GetSubType())
				{
#ifdef WJ_MOUNT_SYSTEM
					case USE_MOUNT:
					{
						if (Mount_map_unallowed_find(GetMapIndex()))
						{
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't mount in this map!"));
							return false;
						}
						
						if ((item->GetWearFlag() & WEARABLE_COSTUME_MOUNT))
						{
							if (!item->IsEquipped())
								EquipItem(item);
							else
								UnequipItem(item);
						}
						else
						{
							Mount(item->GetValue(4), false, item->GetValue(0) * 60, item->GetAttributeType(0), item->GetAttributeValue(0));
							item->SetCount(item->GetCount() - 1);
						}
					}
					break;
#endif

#ifdef WJ_EXTENDED_PET_SYSTEM
					case USE_PET:
					{
						if (Pet_map_unallowed_find(GetMapIndex()))
						{
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't pet in this map!"));
							return false;
						}
						
						if (!item->IsEquipped())
						{
							if (GetPetSystem() && GetPetSystem()->CountSummoned() > 0)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Already summoned pet!"));
								return false;
							}
							EquipItem(item);
						}
						else
						{
							UnequipItem(item);
						}
					}
					break;
#endif

					case USE_ABILITY_UP:
						{
							switch (item->GetValue(0))
							{
								case APPLY_MOV_SPEED:
									AddAffect(AFFECT_UNIQUE_ABILITY, POINT_MOV_SPEED, item->GetValue(2), AFF_MOV_SPEED_POTION, item->GetValue(1), 0, true, true);
									break;

								case APPLY_ATT_SPEED:
									AddAffect(AFFECT_UNIQUE_ABILITY, POINT_ATT_SPEED, item->GetValue(2), AFF_ATT_SPEED_POTION, item->GetValue(1), 0, true, true);
									break;

								case APPLY_STR:
									AddAffect(AFFECT_UNIQUE_ABILITY, POINT_ST, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
									break;

								case APPLY_DEX:
									AddAffect(AFFECT_UNIQUE_ABILITY, POINT_DX, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
									break;

								case APPLY_CON:
									AddAffect(AFFECT_UNIQUE_ABILITY, POINT_HT, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
									break;

								case APPLY_INT:
									AddAffect(AFFECT_UNIQUE_ABILITY, POINT_IQ, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
									break;

								case APPLY_CAST_SPEED:
									AddAffect(AFFECT_UNIQUE_ABILITY, POINT_CASTING_SPEED, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
									break;

								case APPLY_RESIST_MAGIC:
									AddAffect(AFFECT_UNIQUE_ABILITY, POINT_RESIST_MAGIC, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
									break;

								case APPLY_ATT_GRADE_BONUS:
									AddAffect(AFFECT_UNIQUE_ABILITY, POINT_ATT_GRADE_BONUS, 
											item->GetValue(2), 0, item->GetValue(1), 0, true, true);
									break;

								case APPLY_DEF_GRADE_BONUS:
									AddAffect(AFFECT_UNIQUE_ABILITY, POINT_DEF_GRADE_BONUS,
											item->GetValue(2), 0, item->GetValue(1), 0, true, true);
									break;
							}
						}

						if (GetDungeon())
							GetDungeon()->UsePotion(this);

						if (GetWarMap())
							GetWarMap()->UsePotion(this, item);

						item->SetCount(item->GetCount() - 1);
						break;

					default:
						{
							if (item->GetSubType() == USE_SPECIAL)
							{
								sys_log(0, "ITEM_UNIQUE: USE_SPECIAL %u", item->GetVnum());

								switch (item->GetVnum())
								{
									case 71049: // 비단보따리
										if (g_bEnableBootaryCheck)
										{
											if (IS_BOTARYABLE_ZONE(GetMapIndex()) == true)
											{
												UseSilkBotary();
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("개인 상점을 열 수 없는 지역입니다"));
											}
										}
										else
										{
											UseSilkBotary();
										}
										break;
								}
							}
							else
							{
								if (!item->IsEquipped())
									EquipItem(item);
								else
									UnequipItem(item);
								
#ifdef WJ_DRAGONBONE_SYSTEM
								if (item->GetVnum() == ITEM_DRAGONBONE_1 || item->GetVnum() == ITEM_DRAGONBONE_2)
									UpdatePacket();
#endif
							}
						}
						break;
				}
			}
			break;

		case ITEM_COSTUME:
		case ITEM_WEAPON:
		case ITEM_ARMOR:
		case ITEM_ROD:
		case ITEM_RING:		// 신규 반지 아이템
		case ITEM_BELT:		// 신규 벨트 아이템
			// MINING
		case ITEM_PICK:
			// END_OF_MINING
			if (!item->IsEquipped())
				EquipItem(item);
			else
				UnequipItem(item);
			break;
			// 착용하지 않은 용혼석은 사용할 수 없다.
			// 정상적인 클라라면, 용혼석에 관하여 item use 패킷을 보낼 수 없다.
			// 용혼석 착용은 item move 패킷으로 한다.
			// 착용한 용혼석은 추출한다.
		case ITEM_DS:
			{
				if (!item->IsEquipped())
					return false;
				return DSManager::instance().PullOut(this, NPOS, item);
			break;
			}
		case ITEM_SPECIAL_DS:
			if (!item->IsEquipped())
				EquipItem(item);
			else
				UnequipItem(item);
			break;
		
		case ITEM_FISH:
			{
				if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련 중에는 이용할 수 없는 물품입니다."));
					return false;
				}

				if (item->GetSubType() == FISH_ALIVE)
					fishing::UseFish(this, item);
			}
			break;

		case ITEM_TREASURE_BOX:
			{
				return false;
				//ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("열쇠로 잠겨 있어서 열리지 않는것 같다. 열쇠를 구해보자."));
			}
			break;

		case ITEM_TREASURE_KEY:
			{
				LPITEM item2;

				if (!GetItem(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme14
					return false;

				if (item2->GetType() != ITEM_TREASURE_BOX)
				{
					ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("열쇠로 여는 물건이 아닌것 같다."));
					return false;
				}

				if (item->GetValue(0) == item2->GetValue(0))
				{
					//ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("열쇠는 맞으나 아이템 주는 부분 구현이 안되었습니다."));
					DWORD dwBoxVnum = item2->GetVnum();
					std::vector <DWORD> dwVnums;
					std::vector <DWORD> dwCounts;
					std::vector <LPITEM> item_gets(0); // @fixme34
					int count = 0;

					if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
					{
						ITEM_MANAGER::instance().RemoveItem(item);
						ITEM_MANAGER::instance().RemoveItem(item2);
						
						for (int i = 0; i < count; i++){
							switch (dwVnums[i])
							{
								case CSpecialItemGroup::GOLD:
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("돈 %d 냥을 획득했습니다."), dwCounts[i]);
									break;
								case CSpecialItemGroup::EXP:
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 부터 신비한 빛이 나옵니다."));
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d의 경험치를 획득했습니다."), dwCounts[i]);
									break;
								case CSpecialItemGroup::MOB:
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 몬스터가 나타났습니다!"));
									break;
								case CSpecialItemGroup::SLOW:
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 나온 빨간 연기를 들이마시자 움직이는 속도가 느려졌습니다!"));
									break;
								case CSpecialItemGroup::DRAIN_HP:
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자가 갑자기 폭발하였습니다! 생명력이 감소했습니다."));
									break;
								case CSpecialItemGroup::POISON:
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 나온 녹색 연기를 들이마시자 독이 온몸으로 퍼집니다!"));
									break;
#ifdef WJ_WOLFMAN_CHARACTER
								case CSpecialItemGroup::BLEEDING:
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 나온 녹색 연기를 들이마시자 독이 온몸으로 퍼집니다!"));
									break;
#endif
								case CSpecialItemGroup::MOB_GROUP:
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 몬스터가 나타났습니다!"));
									break;
								default:
									if (item_gets[i])
									{
										if (dwCounts[i] > 1)
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 %s 가 %d 개 나왔습니다."), item_gets[i]->GetName(), dwCounts[i]);
										else
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 %s 가 나왔습니다."), item_gets[i]->GetName());

									}
							}
						}
					}
					else
					{
						ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("열쇠가 맞지 않는 것 같다."));
						return false;
					}
				}
				else
				{
					ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("열쇠가 맞지 않는 것 같다."));
					return false;
				}
			}
			break;

		case ITEM_GIFTBOX:
			{
#ifdef ENABLE_NEWSTUFF
				if (0 != g_BoxUseTimeLimitValue)
				{
					if (get_dword_time() < m_dwLastBoxUseTime+g_BoxUseTimeLimitValue)
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("EVENT_FLAG_WAIT"));
						return false;
					}
				}

				m_dwLastBoxUseTime = get_dword_time();
#endif
				
				DWORD dwBoxVnum = item->GetVnum();
				std::vector <DWORD> dwVnums;
				std::vector <DWORD> dwCounts;
				std::vector <LPITEM> item_gets(0); // @fixme34
				int count = 0;

				if( dwBoxVnum > 51500 && dwBoxVnum < 52000 )	// ¿eE￥¿ø¼®μe
				{
					if( !(this->DragonSoul_IsQualified()) )
					{
						ChatPacket(CHAT_TYPE_INFO,LC_TEXT("먼저 용혼석 퀘스트를 완료하셔야 합니다."));
						return false;
					}
				}

				if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
				{
					item->SetCount(item->GetCount()-1);

					for (int i = 0; i < count; i++){
						switch (dwVnums[i])
						{
						case CSpecialItemGroup::GOLD:
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("돈 %d 냥을 획득했습니다."), dwCounts[i]);
							break;
						case CSpecialItemGroup::EXP:
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 부터 신비한 빛이 나옵니다."));
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d의 경험치를 획득했습니다."), dwCounts[i]);
							break;
						case CSpecialItemGroup::MOB:
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 몬스터가 나타났습니다!"));
							break;
						case CSpecialItemGroup::SLOW:
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 나온 빨간 연기를 들이마시자 움직이는 속도가 느려졌습니다!"));
							break;
						case CSpecialItemGroup::DRAIN_HP:
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자가 갑자기 폭발하였습니다! 생명력이 감소했습니다."));
							break;
						case CSpecialItemGroup::POISON:
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 나온 녹색 연기를 들이마시자 독이 온몸으로 퍼집니다!"));
							break;
#ifdef WJ_WOLFMAN_CHARACTER
						case CSpecialItemGroup::BLEEDING:
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 나온 녹색 연기를 들이마시자 독이 온몸으로 퍼집니다!"));
							break;
#endif
						case CSpecialItemGroup::MOB_GROUP:
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 몬스터가 나타났습니다!"));
							break;
						default:
							if (item_gets[i])
							{
								if (dwCounts[i] > 1)
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 %s 가 %d 개 나왔습니다."), item_gets[i]->GetName(), dwCounts[i]);
								else
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 %s 가 나왔습니다."), item_gets[i]->GetName());
							}
						}
					}
				}
				else
				{
					ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("아무것도 얻을 수 없었습니다."));
					return false;
				}
			}
			break;

		case ITEM_SKILLFORGET:
			{
				if (!item->GetSocket(0))
				{
					// @fixme35
					if (item->GetCount() == 1)
						ITEM_MANAGER::instance().RemoveItem(item);
					else
						item->SetCount(item->GetCount() - 1);
					return false;
				}

				DWORD dwVnum = item->GetSocket(0);

				if (SkillLevelDown(dwVnum))
				{
					// @fixme35
					if (item->GetCount() == 1)
						ITEM_MANAGER::instance().RemoveItem(item);
					else
						item->SetCount(item->GetCount() - 1);
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("스킬 레벨을 내리는데 성공하였습니다."));
				}
				else
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("스킬 레벨을 내릴 수 없습니다."));
			}
			break;

		case ITEM_SKILLBOOK:
			{
				if (IsPolymorphed())
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변신중에는 책을 읽을수 없습니다."));
					return false;
				}

				DWORD dwVnum = 0;

				if (item->GetVnum() == 50300)
				{
					dwVnum = item->GetSocket(0);
				}
				else
				{
					// 새로운 수련서는 value 0 에 스킬 번호가 있으므로 그것을 사용.
					dwVnum = item->GetValue(0);
				}

				if (0 == dwVnum)
				{
					// @fixme35
					if (item->GetCount() == 1)
						ITEM_MANAGER::instance().RemoveItem(item);
					else
						item->SetCount(item->GetCount() - 1);
					return false;
				}

				if (true == LearnSkillByBook(dwVnum))
				{
					// @fixme35
					if (item->GetCount() == 1)
						ITEM_MANAGER::instance().RemoveItem(item);
					else
						item->SetCount(item->GetCount() - 1);

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);

					if (distribution_test_server)
						iReadDelay /= 3;

					SetSkillNextReadTime(dwVnum, get_global_time() + iReadDelay);
				}
			}
			break;

		case ITEM_USE:
			{
				if (item->GetVnum() > 50800 && item->GetVnum() <= 50820)
				{
					if (test_server)
						sys_log (0, "ADD addtional effect : vnum(%d) subtype(%d)", item->GetOriginalVnum(), item->GetSubType());

					int affect_type = AFFECT_EXP_BONUS_EURO_FREE;
					int apply_type = aApplyInfo[item->GetValue(0)].bPointType;
					int apply_value = item->GetValue(2);
					int apply_duration = item->GetValue(1);

					switch (item->GetSubType())
					{
						case USE_ABILITY_UP:
							if (FindAffect(affect_type, apply_type))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 효과가 걸려 있습니다."));
								return false;
							}

							{
								switch (item->GetValue(0))
								{
									case APPLY_MOV_SPEED:
										AddAffect(affect_type, apply_type, apply_value, AFF_MOV_SPEED_POTION, apply_duration, 0, true, true);
										break;

									case APPLY_ATT_SPEED:
										AddAffect(affect_type, apply_type, apply_value, AFF_ATT_SPEED_POTION, apply_duration, 0, true, true);
										break;

									case APPLY_STR:
									case APPLY_DEX:
									case APPLY_CON:
									case APPLY_INT:
									case APPLY_CAST_SPEED:
									case APPLY_RESIST_MAGIC:
									case APPLY_ATT_GRADE_BONUS:
									case APPLY_DEF_GRADE_BONUS:
										AddAffect(affect_type, apply_type, apply_value, 0, apply_duration, 0, true, true);
										break;
								}
							}

							if (GetDungeon())
								GetDungeon()->UsePotion(this);

							if (GetWarMap())
								GetWarMap()->UsePotion(this, item);

							item->SetCount(item->GetCount() - 1);
							break;
		
					case USE_AFFECT :
						{
							if (FindAffect(AFFECT_EXP_BONUS_EURO_FREE, aApplyInfo[item->GetValue(1)].bPointType))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 효과가 걸려 있습니다."));
							}
							else
							{
								// PC_BANG_ITEM_ADD
								if (item->IsPCBangItem() == true)
								{
									// PC방인지 체크해서 처리
									if (CPCBangManager::instance().IsPCBangIP(GetDesc()->GetHostName()) == false)
									{
										// PC방이 아님!
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템은 PC방에서만 사용할 수 있습니다."));
										return false;
									}
								}
								// END_PC_BANG_ITEM_ADD

								AddAffect(AFFECT_EXP_BONUS_EURO_FREE, aApplyInfo[item->GetValue(1)].bPointType, item->GetValue(2), 0, item->GetValue(3), 0, false, true);
								item->SetCount(item->GetCount() - 1);
							}
						}
						break;

					case USE_POTION_NODELAY:
						{
							if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
							{
								if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련장에서 사용하실 수 없습니다."));
									return false;
								}

								switch (item->GetVnum())
								{
									case 70020 :
									case 71018 :
									case 71019 :
									case 71020 :
										if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
										{
											if (m_nPotionLimit <= 0)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("사용 제한량을 초과하였습니다."));
												return false;
											}
										}
										break;

									default :
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련장에서 사용하실 수 없습니다."));
										return false;
										break;
								}
							}
						
							if (thecore_pulse() < LastPotionNoDelayUse + 8)
								return false;

							bool used = false;

							if (item->GetValue(0) != 0) // HP 절대값 회복
							{
								if (GetHP() < GetMaxHP())
								{
									PointChange(POINT_HP, item->GetValue(0) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
									EffectPacket(SE_HPUP_RED);
									used = TRUE;
								}
							}

							if (item->GetValue(1) != 0)	// SP 절대값 회복
							{
								if (GetSP() < GetMaxSP())
								{
									PointChange(POINT_SP, item->GetValue(1) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
									EffectPacket(SE_SPUP_BLUE);
									used = TRUE;
								}
							}

							if (item->GetValue(3) != 0) // HP % 회복
							{
								if (GetHP() < GetMaxHP())
								{
									PointChange(POINT_HP, item->GetValue(3) * GetMaxHP() / 100);
									EffectPacket(SE_HPUP_RED);
									used = TRUE;
								}
							}

							if (item->GetValue(4) != 0) // SP % 회복
							{
								if (GetSP() < GetMaxSP())
								{
									PointChange(POINT_SP, item->GetValue(4) * GetMaxSP() / 100);
									EffectPacket(SE_SPUP_BLUE);
									used = TRUE;
								}
							}

							if (used)
							{
								if (item->GetVnum() == 50085 || item->GetVnum() == 50086)
								{
									if (test_server)
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("월병 또는 종자 를 사용하였습니다"));
									SetUseSeedOrMoonBottleTime();
								}
								if (GetDungeon())
									GetDungeon()->UsePotion(this);

								if (GetWarMap())
									GetWarMap()->UsePotion(this, item);

								m_nPotionLimit--;

								//RESTRICT_USE_SEED_OR_MOONBOTTLE
								item->SetCount(item->GetCount() - 1);
								//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
							}
						}
						break;
					}

					return true;
				}


				if (item->GetVnum() >= 27863 && item->GetVnum() <= 27883)
				{
					if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련 중에는 이용할 수 없는 물품입니다."));
						return false;
					}
				}

				if (test_server)
				{
					 sys_log (0, "USE_ITEM %s Type %d SubType %d vnum %d", item->GetName(), item->GetType(), item->GetSubType(), item->GetOriginalVnum());
				}

				switch (item->GetSubType())
				{
					case USE_TIME_CHARGE_PER:
						{
							LPITEM pDestItem = GetItem(DestCell);
							if (NULL == pDestItem)
							{
								return false;
							}
							// 우선 용혼석에 관해서만 하도록 한다.
							if (pDestItem->IsDragonSoul())
							{
								int ret;
								char buf[128];
								if (item->GetVnum() == DRAGON_HEART_VNUM)
								{
									ret = pDestItem->GiveMoreTime_Per((float)item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
								}
								else
								{
									ret = pDestItem->GiveMoreTime_Per((float)item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
								}
								if (ret > 0)
								{
									if (item->GetVnum() == DRAGON_HEART_VNUM)
									{
										sprintf(buf, "Inc %ds by item{VN:%d SOC%d:%d}", ret, item->GetVnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
									}
									else
									{
										sprintf(buf, "Inc %ds by item{VN:%d VAL%d:%d}", ret, item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									}

									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d초 만큼 충전되었습니다."), ret);
									item->SetCount(item->GetCount() - 1);
									LogManager::instance().ItemLog(this, item, "DS_CHARGING_SUCCESS", buf);
									return true;
								}
								else
								{
									if (item->GetVnum() == DRAGON_HEART_VNUM)
									{
										sprintf(buf, "No change by item{VN:%d SOC%d:%d}", item->GetVnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
									}
									else
									{
										sprintf(buf, "No change by item{VN:%d VAL%d:%d}", item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									}

									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("충전할 수 없습니다."));
									LogManager::instance().ItemLog(this, item, "DS_CHARGING_FAILED", buf);
									return false;
								}
							}
							else
								return false;
						}
						break;
					case USE_TIME_CHARGE_FIX:
						{
							LPITEM pDestItem = GetItem(DestCell);
							if (NULL == pDestItem)
							{
								return false;
							}
							// 우선 용혼석에 관해서만 하도록 한다.
							if (pDestItem->IsDragonSoul())
							{
								int ret = pDestItem->GiveMoreTime_Fix(item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
								char buf[128];
								if (ret)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d초 만큼 충전되었습니다."), ret);
									sprintf(buf, "Increase %ds by item{VN:%d VAL%d:%d}", ret, item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									LogManager::instance().ItemLog(this, item, "DS_CHARGING_SUCCESS", buf);
									item->SetCount(item->GetCount() - 1);
									return true;
								}
								else
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("충전할 수 없습니다."));
									sprintf(buf, "No change by item{VN:%d VAL%d:%d}", item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									LogManager::instance().ItemLog(this, item, "DS_CHARGING_FAILED", buf);
									return false;
								}
							}
							else
								return false;
						}
						break;
					case USE_SPECIAL:
						
						switch (item->GetVnum())
						{
							//크리스마스 란주
							case ITEM_NOG_POCKET:
								{
									/*
									란주능력치 : item_proto value 의미
										이동속도  value 1
										공격력	  value 2
										경험치    value 3
										지속시간  value 0 (단위 초)

									*/
									if (FindAffect(AFFECT_NOG_ABILITY))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 효과가 걸려 있습니다."));
										return false;
									}
									long time = item->GetValue(0);
									long moveSpeedPer	= item->GetValue(1);
									long attPer	= item->GetValue(2);
									long expPer			= item->GetValue(3);
									AddAffect(AFFECT_NOG_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
									AddAffect(AFFECT_NOG_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
									AddAffect(AFFECT_NOG_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
									item->SetCount(item->GetCount() - 1);
								}
								break;
								
							//라마단용 사탕
							case ITEM_RAMADAN_CANDY:
								{
									/*
									사탕능력치 : item_proto value 의미
										이동속도  value 1
										공격력	  value 2
										경험치    value 3
										지속시간  value 0 (단위 초)

									*/
									if (FindAffect(AFFECT_RAMADAN_ABILITY)) // @fixme54
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 효과가 걸려 있습니다."));
										return false;
									}
									long time = item->GetValue(0);
									long moveSpeedPer	= item->GetValue(1);
									long attPer	= item->GetValue(2);
									long expPer			= item->GetValue(3);
									AddAffect(AFFECT_RAMADAN_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
									AddAffect(AFFECT_RAMADAN_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
									AddAffect(AFFECT_RAMADAN_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
									item->SetCount(item->GetCount() - 1);
								}
								break;
							case ITEM_MARRIAGE_RING:
								{
									marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(GetPlayerID());
									if (pMarriage)
									{
										if (pMarriage->ch1 != NULL)
										{
											if (CArenaManager::instance().IsArenaMap(pMarriage->ch1->GetMapIndex()) == true)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련 중에는 이용할 수 없는 물품입니다."));
												break;
											}
										}

										if (pMarriage->ch2 != NULL)
										{
											if (CArenaManager::instance().IsArenaMap(pMarriage->ch2->GetMapIndex()) == true)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련 중에는 이용할 수 없는 물품입니다."));
												break;
											}
										}

										int consumeSP = CalculateConsumeSP(this);

										if (consumeSP < 0)
											return false;

										PointChange(POINT_SP, -consumeSP, false);

										WarpToPID(pMarriage->GetOther(GetPlayerID()));
									}
									else
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("결혼 상태가 아니면 결혼반지를 사용할 수 없습니다."));
								}
								break;

								//기존 용기의 망토
							case UNIQUE_ITEM_CAPE_OF_COURAGE:
								//라마단 보상용 용기의 망토
							case 70057:
							case REWARD_BOX_UNIQUE_ITEM_CAPE_OF_COURAGE:
								AggregateMonster();
								item->SetCount(item->GetCount()-1);
								break;

							case UNIQUE_ITEM_WHITE_FLAG:
								ForgetMyAttacker();
								item->SetCount(item->GetCount()-1);
								break;

							case UNIQUE_ITEM_TREASURE_BOX:
								break;

							case 30093:
							case 30094:
							case 30095:
							case 30096:
								// 복주머니
								{
									const int MAX_BAG_INFO = 26;
									static struct LuckyBagInfo
									{
										DWORD count;
										int prob;
										DWORD vnum;
									} b1[MAX_BAG_INFO] =
									{
										{ 1000,	302,	1 },
										{ 10,	150,	27002 },
										{ 10,	75,	27003 },
										{ 10,	100,	27005 },
										{ 10,	50,	27006 },
										{ 10,	80,	27001 },
										{ 10,	50,	27002 },
										{ 10,	80,	27004 },
										{ 10,	50,	27005 },
										{ 1,	10,	50300 },
										{ 1,	6,	92 },
										{ 1,	2,	132 },
										{ 1,	6,	1052 },
										{ 1,	2,	1092 },
										{ 1,	6,	2082 },
										{ 1,	2,	2122 },
										{ 1,	6,	3082 },
										{ 1,	2,	3122 },
										{ 1,	6,	5052 },
										{ 1,	2,	5082 },
										{ 1,	6,	7082 },
										{ 1,	2,	7122 },
										{ 1,	1,	11282 },
										{ 1,	1,	11482 },
										{ 1,	1,	11682 },
										{ 1,	1,	11882 },
									};
	
									LuckyBagInfo * bi = NULL;
									bi = b1;

									int pct = number(1, 1000);

									int i;
									for (i=0;i<MAX_BAG_INFO;i++)
									{
										if (pct <= bi[i].prob)
											break;
										pct -= bi[i].prob;
									}
									if (i>=MAX_BAG_INFO)
										return false;

									if (bi[i].vnum == 50300)
									{
										// 스킬수련서는 특수하게 준다.
										GiveRandomSkillBook();
									}
									else if (bi[i].vnum == 1)
									{
										PointChange(POINT_GOLD, 1000, true);
									}
									else
									{
										AutoGiveItem(bi[i].vnum, bi[i].count);
									}
									// @fixme35
									if (item->GetCount() == 1)
										ITEM_MANAGER::instance().RemoveItem(item);
									else
										item->SetCount(item->GetCount() - 1);
								}
								break;

							case 50004: // 이벤트용 감지기
								{
									if (item->GetSocket(0))
									{
										item->SetSocket(0, item->GetSocket(0) + 1);
									}
									else
									{
										// 처음 사용시
										int iMapIndex = GetMapIndex();

										PIXEL_POSITION pos;

										if (SECTREE_MANAGER::instance().GetRandomLocation(iMapIndex, pos, 700))
										{
											item->SetSocket(0, 1);
											item->SetSocket(1, pos.x);
											item->SetSocket(2, pos.y);
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 곳에선 이벤트용 감지기가 동작하지 않는것 같습니다."));
											return false;
										}
									}

									int dist = 0;
									float distance = (DISTANCE_SQRT(GetX()-item->GetSocket(1), GetY()-item->GetSocket(2)));

									if (distance < 1000.0f)
									{
										// 발견!
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이벤트용 감지기가 신비로운 빛을 내며 사라집니다."));

										// 사용횟수에 따라 주는 아이템을 다르게 한다.
										struct TEventStoneInfo
										{
											DWORD dwVnum;
											int count;
											int prob;
										};
										const int EVENT_STONE_MAX_INFO = 15;
										TEventStoneInfo info_10[EVENT_STONE_MAX_INFO] =
										{ 
											{ 27001, 10,  8 },
											{ 27004, 10,  6 },
											{ 27002, 10, 12 },
											{ 27005, 10, 12 },
											{ 27100,  1,  9 },
											{ 27103,  1,  9 },
											{ 27101,  1, 10 },
											{ 27104,  1, 10 },
											{ 27999,  1, 12 },

											{ 25040,  1,  4 },

											{ 27410,  1,  0 },
											{ 27600,  1,  0 },
											{ 25100,  1,  0 },

											{ 50001,  1,  0 },
											{ 50003,  1,  1 },
										};
										TEventStoneInfo info_7[EVENT_STONE_MAX_INFO] =
										{ 
											{ 27001, 10,  1 },
											{ 27004, 10,  1 },
											{ 27004, 10,  9 },
											{ 27005, 10,  9 },
											{ 27100,  1,  5 },
											{ 27103,  1,  5 },
											{ 27101,  1, 10 },
											{ 27104,  1, 10 },
											{ 27999,  1, 14 },

											{ 25040,  1,  5 },

											{ 27410,  1,  5 },
											{ 27600,  1,  5 },
											{ 25100,  1,  5 },

											{ 50001,  1,  0 },
											{ 50003,  1,  5 },

										};
										TEventStoneInfo info_4[EVENT_STONE_MAX_INFO] =
										{ 
											{ 27001, 10,  0 },
											{ 27004, 10,  0 },
											{ 27002, 10,  0 },
											{ 27005, 10,  0 },
											{ 27100,  1,  0 },
											{ 27103,  1,  0 },
											{ 27101,  1,  0 },
											{ 27104,  1,  0 },
											{ 27999,  1, 25 },

											{ 25040,  1,  0 },

											{ 27410,  1,  0 },
											{ 27600,  1,  0 },
											{ 25100,  1, 15 },

											{ 50001,  1, 10 },
											{ 50003,  1, 50 },

										};

										{
											TEventStoneInfo* info;
											if (item->GetSocket(0) <= 4)
												info = info_4;
											else if (item->GetSocket(0) <= 7)
												info = info_7;
											else
												info = info_10;

											int prob = number(1, 100);

											for (int i = 0; i < EVENT_STONE_MAX_INFO; ++i)
											{
												if (!info[i].prob)
													continue;

												if (prob <= info[i].prob)
												{
													if (info[i].dwVnum == 50001)
													{
														DWORD * pdw = M2_NEW DWORD[2];

														pdw[0] = info[i].dwVnum;
														pdw[1] = info[i].count;

														// 추첨서는 소켓을 설정한다
														DBManager::instance().ReturnQuery(QID_LOTTO, GetPlayerID(), pdw,
																"INSERT INTO lotto_list VALUES(0, 'server%s', %u, NOW())", 
																get_table_postfix(), GetPlayerID());
													}
													else
														AutoGiveItem(info[i].dwVnum, info[i].count);

													break;
												}
												prob -= info[i].prob;
											}
										}

										char chatbuf[CHAT_MAX_LEN + 1];
										int len = snprintf(chatbuf, sizeof(chatbuf), "StoneDetect %u 0 0", (DWORD)GetVID());

										if (len < 0 || len >= (int) sizeof(chatbuf))
											len = sizeof(chatbuf) - 1;

										++len;  // \0 문자까지 보내기

										TPacketGCChat pack_chat;
										pack_chat.header	= HEADER_GC_CHAT;
										pack_chat.size		= sizeof(TPacketGCChat) + len;
										pack_chat.type		= CHAT_TYPE_COMMAND;
										pack_chat.id		= 0;
										pack_chat.bEmpire	= GetDesc()->GetEmpire();
										//pack_chat.id	= vid;

										TEMP_BUFFER buf;
										buf.write(&pack_chat, sizeof(TPacketGCChat));
										buf.write(chatbuf, len);

										PacketAround(buf.read_peek(), buf.size());

										// @fixme35
										if (item->GetCount() == 1)
											ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (DETECT_EVENT_STONE) 1");
										else
											item->SetCount(item->GetCount() - 1);
										return true;
									}
									else if (distance < 20000)
										dist = 1;
									else if (distance < 70000)
										dist = 2;
									else
										dist = 3;

									// 많이 사용했으면 사라진다.
									const int STONE_DETECT_MAX_TRY = 10;
									if (item->GetSocket(0) >= STONE_DETECT_MAX_TRY)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이벤트용 감지기가 흔적도 없이 사라집니다."));
										// @fixme35
										if (item->GetCount() == 1)
											ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (DETECT_EVENT_STONE) 0");
										else
											item->SetCount(item->GetCount() - 1);
										AutoGiveItem(27002);
										return true;
									}

									if (dist)
									{
										char chatbuf[CHAT_MAX_LEN + 1];
										int len = snprintf(chatbuf, sizeof(chatbuf),
												"StoneDetect %u %d %d",
											   	(DWORD)GetVID(), dist, (int)GetDegreeFromPositionXY(GetX(), item->GetSocket(2), item->GetSocket(1), GetY()));

										if (len < 0 || len >= (int) sizeof(chatbuf))
											len = sizeof(chatbuf) - 1;

										++len;  // \0 문자까지 보내기

										TPacketGCChat pack_chat;
										pack_chat.header	= HEADER_GC_CHAT;
										pack_chat.size		= sizeof(TPacketGCChat) + len;
										pack_chat.type		= CHAT_TYPE_COMMAND;
										pack_chat.id		= 0;
										pack_chat.bEmpire	= GetDesc()->GetEmpire();
										//pack_chat.id		= vid;

										TEMP_BUFFER buf;
										buf.write(&pack_chat, sizeof(TPacketGCChat));
										buf.write(chatbuf, len);

										PacketAround(buf.read_peek(), buf.size());
									}

								}
								break;

							case 27989: // 영석감지기
							case 76006: // 선물용 영석감지기
								{
									LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(GetMapIndex());

									if (pMap != NULL)
									{
										item->SetSocket(0, item->GetSocket(0) + 1);

										FFindStone f;

										// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
										pMap->for_each(f);

										if (f.m_mapStone.size() > 0)
										{
											std::map<DWORD, LPCHARACTER>::iterator stone = f.m_mapStone.begin();

											DWORD max = UINT_MAX;
											LPCHARACTER pTarget = stone->second;

											while (stone != f.m_mapStone.end())
											{
												DWORD dist = (DWORD)DISTANCE_SQRT(GetX()-stone->second->GetX(), GetY()-stone->second->GetY());

												if (dist != 0 && max > dist)
												{
													max = dist;
													pTarget = stone->second;
												}
												stone++;
											}

											if (pTarget != NULL)
											{
												int val = 3;

												if (max < 10000) val = 2;
												else if (max < 70000) val = 1;

												ChatPacket(CHAT_TYPE_COMMAND, "StoneDetect %u %d %d", (DWORD)GetVID(), val,
														(int)GetDegreeFromPositionXY(GetX(), pTarget->GetY(), pTarget->GetX(), GetY()));
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("감지기를 작용하였으나 감지되는 영석이 없습니다."));
											}
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("감지기를 작용하였으나 감지되는 영석이 없습니다."));
										}

										if (item->GetSocket(0) >= 6)
										{
											ChatPacket(CHAT_TYPE_COMMAND, "StoneDetect %u 0 0", (DWORD)GetVID());
											// @fixme35
											if (item->GetCount() == 1)
												ITEM_MANAGER::instance().RemoveItem(item);
											else
												item->SetCount(item->GetCount() - 1);
										}
									}
									break;
								}
								break;

							case 27996: // 독병
								item->SetCount(item->GetCount() - 1);
								AttackedByPoison(NULL); // @warme4
								break;
	
							case 27987: // 조개
								// 50  돌조각 47990
								// 30  꽝
								// 10  백진주 47992
								// 7   청진주 47993
								// 3   피진주 47994
								{
									item->SetCount(item->GetCount() - 1);

									int r = number(1, 100);

									if (r <= 50)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("조개에서 돌조각이 나왔습니다."));
										AutoGiveItem(27990);
									}
									else
									{
										const int prob_table_gb2312[] =
										{
											95, 97, 99
										};

										const int * prob_table = prob_table_gb2312;

										if (r <= prob_table[0])
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("조개가 흔적도 없이 사라집니다."));
										}
										else if (r <= prob_table[1])
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("조개에서 백진주가 나왔습니다."));
											AutoGiveItem(27992);
										}
										else if (r <= prob_table[2])
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("조개에서 청진주가 나왔습니다."));
											AutoGiveItem(27993);
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("조개에서 피진주가 나왔습니다."));
											AutoGiveItem(27994);
										}
									}
								}
								break;

							case 71013: // 축제용폭죽
								CreateFly(number(FLY_FIREWORK1, FLY_FIREWORK6), this);
								item->SetCount(item->GetCount() - 1);
								break;

							case 50100: // 폭죽
							case 50101:
							case 50102:
							case 50103:
							case 50104:
							case 50105:
							case 50106:
								CreateFly(item->GetVnum() - 50100 + FLY_FIREWORK1, this);
								item->SetCount(item->GetCount() - 1);
								break;
						
#ifdef WJ_SHOP_SEARCH_SYSTEM
							case 80022:
								ChatPacket(CHAT_TYPE_COMMAND, "OpenShopSearch 1");
								break;
								
							case 80023:
								ChatPacket(CHAT_TYPE_COMMAND, "OpenShopSearch 2");
								break;
#endif								

							case 50200: // 보따리
								if (g_bEnableBootaryCheck)
								{
									if (IS_BOTARYABLE_ZONE(GetMapIndex()) == true)
									{
										__OpenPrivateShop();
									}
									else
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("개인 상점을 열 수 없는 지역입니다"));
									}
								}
								else
								{
									__OpenPrivateShop();
								}
								break;
								
							case fishing::FISH_MIND_PILL_VNUM:
								AddAffect(AFFECT_FISH_MIND_PILL, POINT_NONE, 0, AFF_FISH_MIND, 20*60, 0, true);
								item->SetCount(item->GetCount() - 1);
								break;

							case 50301: // 통솔력 수련서
							case 50302:
							case 50303:
								{
									if (IsPolymorphed() == true)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("둔갑 중에는 능력을 올릴 수 없습니다."));
										return false;
									}

									int lv = GetSkillLevel(SKILL_LEADERSHIP);

									if (lv < item->GetValue(0))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 책은 너무 어려워 이해하기가 힘듭니다."));
										return false;
									}

									if (lv >= item->GetValue(1))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 책은 아무리 봐도 도움이 될 것 같지 않습니다."));
										return false;
									}

									if (LearnSkillByBook(SKILL_LEADERSHIP))
									{
										// @fixme35
										if (item->GetCount() == 1)
											ITEM_MANAGER::instance().RemoveItem(item);
										else
											item->SetCount(item->GetCount() - 1);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (distribution_test_server) iReadDelay /= 3;

										SetSkillNextReadTime(SKILL_LEADERSHIP, get_global_time() + iReadDelay);
									}
								}
								break;

							case 50304: // 연계기 수련서
							case 50305:
							case 50306:
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변신중에는 책을 읽을수 없습니다."));
										return false;
										
									}
									if (GetSkillLevel(SKILL_COMBO) == 0 && GetLevel() < 30)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("레벨 30이 되기 전에는 습득할 수 있을 것 같지 않습니다."));
										return false;
									}

									if (GetSkillLevel(SKILL_COMBO) == 1 && GetLevel() < 50)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("레벨 50이 되기 전에는 습득할 수 있을 것 같지 않습니다."));
										return false;
									}

									if (GetSkillLevel(SKILL_COMBO) >= 2)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("연계기는 더이상 수련할 수 없습니다."));
										return false;
									}

									int iPct = item->GetValue(0);

									if (LearnSkillByBook(SKILL_COMBO, iPct))
									{
										// @fixme35
										if (item->GetCount() == 1)
											ITEM_MANAGER::instance().RemoveItem(item);
										else
											item->SetCount(item->GetCount() - 1);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (distribution_test_server) iReadDelay /= 3;

										SetSkillNextReadTime(SKILL_COMBO, get_global_time() + iReadDelay);
									}
								}
								break;
							case 50311: // 언어 수련서
							case 50312:
							case 50313:
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변신중에는 책을 읽을수 없습니다."));
										return false;
										
									}
									DWORD dwSkillVnum = item->GetValue(0);
									int iPct = MINMAX(0, item->GetValue(1), 100);
									if (GetSkillLevel(dwSkillVnum)>=20 || dwSkillVnum-SKILL_LANGUAGE1+1 == GetEmpire())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 완벽하게 알아들을 수 있는 언어이다."));
										return false;
									}

									if (LearnSkillByBook(dwSkillVnum, iPct))
									{
										// @fixme35
										if (item->GetCount() == 1)
											ITEM_MANAGER::instance().RemoveItem(item);
										else
											item->SetCount(item->GetCount() - 1);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (distribution_test_server) iReadDelay /= 3;

										SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
									}
								}
								break;

							case 50061 : // 일본 말 소환 스킬 수련서
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변신중에는 책을 읽을수 없습니다."));
										return false;
										
									}
									DWORD dwSkillVnum = item->GetValue(0);
									int iPct = MINMAX(0, item->GetValue(1), 100);

									if (GetSkillLevel(dwSkillVnum) >= 10)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("더 이상 수련할 수 없습니다."));
										return false;
									}

									if (LearnSkillByBook(dwSkillVnum, iPct))
									{
										// @fixme35
										if (item->GetCount() == 1)
											ITEM_MANAGER::instance().RemoveItem(item);
										else
											item->SetCount(item->GetCount() - 1);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (distribution_test_server) iReadDelay /= 3;

										SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
									}
								}
								break;

							case 50314: case 50315: case 50316: // 변신 수련서
							case 50323: case 50324: // 증혈 수련서
							case 50325: case 50326: // 철통 수련서
								{
									if (IsPolymorphed() == true)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("둔갑 중에는 능력을 올릴 수 없습니다."));
										return false;
									}
									
									int iSkillLevelLowLimit = item->GetValue(0);
									int iSkillLevelHighLimit = item->GetValue(1);
									int iPct = MINMAX(0, item->GetValue(2), 100);
									int iLevelLimit = item->GetValue(3);
									DWORD dwSkillVnum = 0;
									
									switch (item->GetVnum())
									{
										case 50314: case 50315: case 50316:
											dwSkillVnum = SKILL_POLYMORPH;
											break;

										case 50323: case 50324:
											dwSkillVnum = SKILL_ADD_HP;
											break;

										case 50325: case 50326:
											dwSkillVnum = SKILL_RESIST_PENETRATE;
											break;

										default:
											return false;
									}

									if (0 == dwSkillVnum)
										return false;

									if (GetLevel() < iLevelLimit)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 책을 읽으려면 레벨을 더 올려야 합니다."));
										return false;
									}

									if (GetSkillLevel(dwSkillVnum) >= 40)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("더 이상 수련할 수 없습니다."));
										return false;
									}

									if (GetSkillLevel(dwSkillVnum) < iSkillLevelLowLimit)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 책은 너무 어려워 이해하기가 힘듭니다."));
										return false;
									}

									if (GetSkillLevel(dwSkillVnum) >= iSkillLevelHighLimit)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 책으로는 더 이상 수련할 수 없습니다."));
										return false;
									}

									if (LearnSkillByBook(dwSkillVnum, iPct))
									{
										// @fixme35
										if (item->GetCount() == 1)
											ITEM_MANAGER::instance().RemoveItem(item);
										else
											item->SetCount(item->GetCount() - 1);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (distribution_test_server) iReadDelay /= 3;

										SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
									}
								}
								break;

							case 50902:
							case 50903:
							case 50904:
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변신중에는 책을 읽을수 없습니다."));
										return false;
										
									}
									DWORD dwSkillVnum = SKILL_CREATE;
									int iPct = MINMAX(0, item->GetValue(1), 100);

									if (GetSkillLevel(dwSkillVnum)>=40)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("더 이상 수련할 수 없습니다."));
										return false;
									}

									if (LearnSkillByBook(dwSkillVnum, iPct))
									{
										// @fixme35
										if (item->GetCount() == 1)
											ITEM_MANAGER::instance().RemoveItem(item);
										else
											item->SetCount(item->GetCount() - 1);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (distribution_test_server) iReadDelay /= 3;

										SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);

										if (test_server) 
										{
											ChatPacket(CHAT_TYPE_INFO, "[TEST_SERVER] Success to learn skill ");
										}
									}
									else
									{
										if (test_server) 
										{
											ChatPacket(CHAT_TYPE_INFO, "[TEST_SERVER] Failed to learn skill ");
										}
									}
								}
								break;

								// MINING
							case ITEM_MINING_SKILL_TRAIN_BOOK:
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변신중에는 책을 읽을수 없습니다."));
										return false;
										
									}
									DWORD dwSkillVnum = SKILL_MINING;
									int iPct = MINMAX(0, item->GetValue(1), 100);

									if (GetSkillLevel(dwSkillVnum)>=40)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("더 이상 수련할 수 없습니다."));
										return false;
									}

									if (LearnSkillByBook(dwSkillVnum, iPct))
									{
										// @fixme35
										if (item->GetCount() == 1)
											ITEM_MANAGER::instance().RemoveItem(item);
										else
											item->SetCount(item->GetCount() - 1);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (distribution_test_server) iReadDelay /= 3;

										SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
									}
								}
								break;
								// END_OF_MINING

							case ITEM_HORSE_SKILL_TRAIN_BOOK:
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변신중에는 책을 읽을수 없습니다."));
										return false;
										
									}
									DWORD dwSkillVnum = SKILL_HORSE;
									int iPct = MINMAX(0, item->GetValue(1), 100);

									if (GetLevel() < 50)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아직 승마 스킬을 수련할 수 있는 레벨이 아닙니다."));
										return false;
									}

									if (!test_server && get_global_time() < GetSkillNextReadTime(dwSkillVnum))
									{
										if (FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
										{
											// 주안술서 사용중에는 시간 제한 무시
											RemoveAffect(AFFECT_SKILL_NO_BOOK_DELAY);
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("주안술서를 통해 주화입마에서 빠져나왔습니다."));
										}
										else
										{
											SkillLearnWaitMoreTimeMessage(GetSkillNextReadTime(dwSkillVnum) - get_global_time());
											return false;
										}
									}

									if (GetPoint(POINT_HORSE_SKILL) >= 20 || 
											GetSkillLevel(SKILL_HORSE_WILDATTACK) + GetSkillLevel(SKILL_HORSE_CHARGE) + GetSkillLevel(SKILL_HORSE_ESCAPE) >= 60 ||
											GetSkillLevel(SKILL_HORSE_WILDATTACK_RANGE) + GetSkillLevel(SKILL_HORSE_CHARGE) + GetSkillLevel(SKILL_HORSE_ESCAPE) >= 60)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("더 이상 승마 수련서를 읽을 수 없습니다."));
										return false;
									}

									if (number(1, 100) <= iPct)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("승마 수련서를 읽어 승마 스킬 포인트를 얻었습니다."));
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("얻은 포인트로는 승마 스킬의 레벨을 올릴 수 있습니다."));
										PointChange(POINT_HORSE_SKILL, 1);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (distribution_test_server) iReadDelay /= 3;

										if (!test_server)
											SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
									}
									else
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("승마 수련서 이해에 실패하였습니다."));
									}

									// @fixme35
									if (item->GetCount() == 1)
										ITEM_MANAGER::instance().RemoveItem(item);
									else
										item->SetCount(item->GetCount() - 1);
									
								}
								break;

							case 70102: // 선두
							case 70103: // 선두
								{
									if (GetAlignment() >= 0)
										return false;

									int delta = MIN(-GetAlignment(), item->GetValue(0));

									sys_log(0, "%s ALIGNMENT ITEM %d", GetName(), delta);

									UpdateAlignment(delta);
									item->SetCount(item->GetCount() - 1);

									if (delta / 10 > 0)
									{
										ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("마음이 맑아지는군. 가슴을 짓누르던 무언가가 좀 가벼워진 느낌이야."));
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("선악치가 %d 증가하였습니다."), delta/10);
									}
								}
								break;

							case 71107: // 천도복숭아
								{
									int val = item->GetValue(0);
									int interval = item->GetValue(1);
									quest::PC* pPC = quest::CQuestManager::instance().GetPC(GetPlayerID());
									int last_use_time = pPC->GetFlag("mythical_peach.last_use_time");

									if (get_global_time() - last_use_time < interval * 60 * 60)
									{
										if (test_server == false)
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아직 사용할 수 없습니다."));
											return false;
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("테스트 서버 시간제한 통과"));
										}
									}
									
									if (GetAlignment() == 200000)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("선악치를 더 이상 올릴 수 없습니다."));
										return false;
									}
									
									if (200000 - GetAlignment() < val * 10)
									{
										val = (200000 - GetAlignment()) / 10;
									}
									
									int old_alignment = GetAlignment() / 10;

									UpdateAlignment(val*10);
									
									item->SetCount(item->GetCount()-1);
									pPC->SetFlag("mythical_peach.last_use_time", get_global_time());

									ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("마음이 맑아지는군. 가슴을 짓누르던 무언가가 좀 가벼워진 느낌이야."));
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("선악치가 %d 증가하였습니다."), val);

									char buf[256 + 1];
									snprintf(buf, sizeof(buf), "%d %d", old_alignment, GetAlignment() / 10);
									LogManager::instance().CharLog(this, val, "MYTHICAL_PEACH", buf);
								}
								break;

							case 71109: // 탈석서
							case 72719:
								{
									LPITEM item2;

									if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
										return false;

									if (item2->IsExchanging() || item2->IsEquipped()) // @fixme14
										return false;

									if (item2->GetSocketCount() == 0)
										return false;

									switch( item2->GetType() )
									{
										case ITEM_WEAPON:
											break;
										case ITEM_ARMOR:
											switch (item2->GetSubType())
											{
											case ARMOR_EAR:
											case ARMOR_WRIST:
											case ARMOR_NECK:
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("빼낼 영석이 없습니다"));
												return false;
											}
											break;

										default:
											return false;
									}

									std::stack<long> socket;

									for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
										socket.push(item2->GetSocket(i));

									int idx = ITEM_SOCKET_MAX_NUM - 1;

									while (socket.size() > 0)
									{
										if (socket.top() > 2 && socket.top() != ITEM_BROKEN_METIN_VNUM)
											break;

										idx--;
										socket.pop();
									}

									if (socket.size() == 0)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("빼낼 영석이 없습니다"));
										return false;
									}

									LPITEM pItemReward = AutoGiveItem(socket.top());

									if (pItemReward != NULL)
									{
										item2->SetSocket(idx, 1);

										char buf[256+1];
										snprintf(buf, sizeof(buf), "%s(%u) %s(%u)", 
												item2->GetName(), item2->GetID(), pItemReward->GetName(), pItemReward->GetID());
										LogManager::instance().ItemLog(this, item, "USE_DETACHMENT_ONE", buf);

										item->SetCount(item->GetCount() - 1);
									}
								}
								break;

							case 70201:   // 탈색제
							case 70202:   // 염색약(흰색)
							case 70203:   // 염색약(금색)
							case 70204:   // 염색약(빨간색)
							case 70205:   // 염색약(갈색)
							case 70206:   // 염색약(검은색)
								{
									// NEW_HAIR_STYLE_ADD
									if (GetPart(PART_HAIR) >= 1001)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("현재 헤어스타일에서는 염색과 탈색이 불가능합니다."));
									}
									// END_NEW_HAIR_STYLE_ADD
									else
									{
										quest::CQuestManager& q = quest::CQuestManager::instance();
										quest::PC* pPC = q.GetPC(GetPlayerID());

										if (pPC)
										{
											int last_dye_level = pPC->GetFlag("dyeing_hair.last_dye_level");

											if (last_dye_level == 0 ||
													last_dye_level+3 <= GetLevel() ||
													item->GetVnum() == 70201)
											{
												SetPart(PART_HAIR, item->GetVnum() - 70201);

												if (item->GetVnum() == 70201)
													pPC->SetFlag("dyeing_hair.last_dye_level", 0);
												else
													pPC->SetFlag("dyeing_hair.last_dye_level", GetLevel());

												item->SetCount(item->GetCount() - 1);
												UpdatePacket();
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d 레벨이 되어야 다시 염색하실 수 있습니다."), last_dye_level+3);
											}
										}
									}
								}
								break;

							case ITEM_NEW_YEAR_GREETING_VNUM:
								{
									DWORD dwBoxVnum = ITEM_NEW_YEAR_GREETING_VNUM;
									std::vector <DWORD> dwVnums;
									std::vector <DWORD> dwCounts;
									std::vector <LPITEM> item_gets;
									int count = 0;

									if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
									{
										for (int i = 0; i < count; i++)
										{
											if (dwVnums[i] == CSpecialItemGroup::GOLD)
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("돈 %d 냥을 획득했습니다."), dwCounts[i]);
										}

										item->SetCount(item->GetCount() - 1);
									}
								}
								break;

							case ITEM_VALENTINE_ROSE:
							case ITEM_VALENTINE_CHOCOLATE:
								{
									DWORD dwBoxVnum = item->GetVnum();
									std::vector <DWORD> dwVnums;
									std::vector <DWORD> dwCounts;
									std::vector <LPITEM> item_gets(0); // @fixme34
									int count = 0;


									if (((item->GetVnum() == ITEM_VALENTINE_ROSE) && (SEX_MALE==GET_SEX(this))) ||
										((item->GetVnum() == ITEM_VALENTINE_CHOCOLATE) && (SEX_FEMALE==GET_SEX(this))))
									{
										// 성별이 맞지않아 쓸 수 없다.
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("성별이 맞지않아 이 아이템을 열 수 없습니다."));
										return false;
									}


									if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
										item->SetCount(item->GetCount()-1);
								}
								break;

							case ITEM_WHITEDAY_CANDY:
							case ITEM_WHITEDAY_ROSE:
								{
									DWORD dwBoxVnum = item->GetVnum();
									std::vector <DWORD> dwVnums;
									std::vector <DWORD> dwCounts;
									std::vector <LPITEM> item_gets(0); // @fixme34
									int count = 0;


									if (((item->GetVnum() == ITEM_WHITEDAY_CANDY) && (SEX_MALE==GET_SEX(this))) ||
										((item->GetVnum() == ITEM_WHITEDAY_ROSE) && (SEX_FEMALE==GET_SEX(this))))
									{
										// 성별이 맞지않아 쓸 수 없다.
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("성별이 맞지않아 이 아이템을 열 수 없습니다."));
										return false;
									}


									if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
										item->SetCount(item->GetCount()-1);
								}
								break;

							case 50011: // 월광보합
								{
									DWORD dwBoxVnum = 50011;
									std::vector <DWORD> dwVnums;
									std::vector <DWORD> dwCounts;
									std::vector <LPITEM> item_gets(0); // @fixme34
									int count = 0;

									if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
									{
										for (int i = 0; i < count; i++)
										{
											char buf[50 + 1];
											snprintf(buf, sizeof(buf), "%u %u", dwVnums[i], dwCounts[i]);
											LogManager::instance().ItemLog(this, item, "MOONLIGHT_GET", buf);

											//ITEM_MANAGER::instance().RemoveItem(item);
											item->SetCount(item->GetCount() - 1);

											switch (dwVnums[i])
											{
											case CSpecialItemGroup::GOLD:
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("돈 %d 냥을 획득했습니다."), dwCounts[i]);
												break;

											case CSpecialItemGroup::EXP:
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 부터 신비한 빛이 나옵니다."));
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d의 경험치를 획득했습니다."), dwCounts[i]);
												break;

											case CSpecialItemGroup::MOB:
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 몬스터가 나타났습니다!"));
												break;

											case CSpecialItemGroup::SLOW:
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 나온 빨간 연기를 들이마시자 움직이는 속도가 느려졌습니다!"));
												break;

											case CSpecialItemGroup::DRAIN_HP:
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자가 갑자기 폭발하였습니다! 생명력이 감소했습니다."));
												break;

											case CSpecialItemGroup::POISON:
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 나온 녹색 연기를 들이마시자 독이 온몸으로 퍼집니다!"));
												break;

#ifdef WJ_WOLFMAN_CHARACTER
											case CSpecialItemGroup::BLEEDING:
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 나온 녹색 연기를 들이마시자 독이 온몸으로 퍼집니다!"));
												break;
#endif
												
											case CSpecialItemGroup::MOB_GROUP:
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 몬스터가 나타났습니다!"));
												break;

											default:
												if (item_gets[i])
												{
													if (dwCounts[i] > 1)
														ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 %s 가 %d 개 나왔습니다."), item_gets[i]->GetName(), dwCounts[i]);
													else
														ChatPacket(CHAT_TYPE_INFO, LC_TEXT("상자에서 %s 가 나왔습니다."), item_gets[i]->GetName());
												}
												break;
											}
										}
									}
									else
									{
										ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("아무것도 얻을 수 없었습니다."));
										return false;
									}
								}
								break;

							case ITEM_GIVE_STAT_RESET_COUNT_VNUM:
								{
									//PointChange(POINT_GOLD, -iCost);
									PointChange(POINT_STAT_RESET_COUNT, 1);
									item->SetCount(item->GetCount()-1);
								}
								break;

							case 50107:
								{
									if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련 중에는 이용할 수 없는 물품입니다."));
										return false;
									}
									
									EffectPacket(SE_CHINA_FIREWORK);
#ifdef ENABLE_FIREWORK_STUN
									// 스턴 공격을 올려준다
									AddAffect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5*60, 0, true);
#endif
									item->SetCount(item->GetCount()-1);
								}
								break;

							case 50108:
								{
									if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련 중에는 이용할 수 없는 물품입니다."));
										return false;
									}

									EffectPacket(SE_SPIN_TOP);
#ifdef ENABLE_FIREWORK_STUN
									// 스턴 공격을 올려준다
									AddAffect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5*60, 0, true);
#endif
									item->SetCount(item->GetCount()-1);
								}
								break;

							case ITEM_WONSO_BEAN_VNUM:
								PointChange(POINT_HP, GetMaxHP() - GetHP());
								item->SetCount(item->GetCount()-1);
								break;

							case ITEM_WONSO_SUGAR_VNUM:
								PointChange(POINT_SP, GetMaxSP() - GetSP());
								item->SetCount(item->GetCount()-1);
								break;

							case ITEM_WONSO_FRUIT_VNUM:
								PointChange(POINT_STAMINA, GetMaxStamina()-GetStamina());
								item->SetCount(item->GetCount()-1);
								break;

							case 90008: // VCARD
							case 90009: // VCARD
								VCardUse(this, this, item);
								break;

							case ITEM_ELK_VNUM: // 돈꾸러미
								{
									int iGold = item->GetSocket(0);
									ITEM_MANAGER::instance().RemoveItem(item);
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("돈 %d 냥을 획득했습니다."), iGold);
									PointChange(POINT_GOLD, iGold);
								}
								break;

								//군주의 증표 
							case 70021:
								{
									int HealPrice = quest::CQuestManager::instance().GetEventFlag("MonarchHealGold");
									if (HealPrice == 0)
										HealPrice = 2000000;

									if (CMonarch::instance().HealMyEmpire(this, HealPrice))
									{
										char szNotice[256];
										snprintf(szNotice, sizeof(szNotice), LC_TEXT("군주의 축복으로 이지역 %s 유저는 HP,SP가 모두 채워집니다."), EMPIRE_NAME(GetEmpire()));
										SendNoticeMap(szNotice, GetMapIndex(), false);
										
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("군주의 축복을 사용하였습니다."));
									}
								}
								break;

							case 27995:
								{
								}
								break;

							case 71092 : // 변신 해체부 임시
								{
									if (m_pkChrTarget != NULL)
									{
										if (m_pkChrTarget->IsPolymorphed())
										{
											m_pkChrTarget->SetPolymorph(0);
											m_pkChrTarget->RemoveAffect(AFFECT_POLYMORPH);
										}
									}
									else
									{
										if (IsPolymorphed())
										{
											SetPolymorph(0);
											RemoveAffect(AFFECT_POLYMORPH);
										}
									}
								}
								break;

#ifndef WJ_ELDER_ATTRIBUTE_SYSTEM
							case 71051 : // 진재가
								{
									LPITEM item2;
									if (!IsValidItemPosition(DestCell) || !(item2 = GetInventoryItem(wDestCell)))
										return false;
									if (ITEM_COSTUME == item2->GetType()){ // @fixme15
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}
									if (item2->IsExchanging() || item2->IsEquipped()) // @fixme14
										return false;
									if (item2->GetAttributeSetIndex() == -1){
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}
									if (item2->AddRareAttribute() == true){
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("성공적으로 속성이 추가 되었습니다"));
										int iAddedIdx = item2->GetRareAttrCount() + 4;
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());
										LogManager::instance().ItemLog(GetPlayerID(), item2->GetAttributeType(iAddedIdx), item2->GetAttributeValue(iAddedIdx), item->GetID(), "ADD_RARE_ATTR", buf, GetDesc()->GetHostName(), item->GetOriginalVnum());
										item->SetCount(item->GetCount() - 1);
									}
									else
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("더 이상 이 아이템으로 속성을 추가할 수 없습니다"));
								}
								break;
#endif
								
#ifdef WJ_ELDER_ATTRIBUTE_SYSTEM
							case 71052:
								{
									LPITEM item2;
									if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
										return false;
									if (ITEM_COSTUME == item2->GetType()){ // @fixme15
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}
									if (item2->IsExchanging() || item2->IsEquipped()) // @fixme14
										return false;
									if (item2->GetAttributeSetIndex() == -1){
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}
									if (item2->GetAttributeCount() == 5){
										item2->RemoveAttributeAt(4);
										item2->AddAttribute();
										item->SetCount(item->GetCount()-1);
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("NEW_ATTRIBUTE_SUCCESS_MARBLE"));
									}
									else if (item2->GetAttributeCount() == 4){
										item2->AddAttribute();
										item->SetCount(item->GetCount()-1);
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("NEW_ATTRIBUTE_SUCCESS_MARBLE"));										
									}
									else
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("NEW_ATTRIBUTE_MARBLE_ENOUGH_ATTR_COUNT"));
								}
								break;
#else
							case 71052 : // 진재경
								{
									LPITEM item2;
									if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
										return false;
									if (ITEM_COSTUME == item2->GetType()){ // @fixme15
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}
									if (item2->IsExchanging() || item2->IsEquipped()) // @fixme14
										return false;
									if (item2->GetAttributeSetIndex() == -1){
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}
									if (item2->ChangeRareAttribute() == true){
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());
										LogManager::instance().ItemLog(this, item, "CHANGE_RARE_ATTR", buf);
										item->SetCount(item->GetCount() - 1);
									}
									else
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변경 시킬 속성이 없습니다"));
								}
								break;
#endif

#ifdef WJ_UPGRADE_SOCKET_SYSTEM
							case 80018:
								{
									LPITEM item2;
									
									if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
										return false;
									
									if (item2->IsExchanging() || item2->IsEquipped())
										return false;
									
									if (item2->GetType() != ITEM_WEAPON)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_OPEN_SOCKET_ONLY_WEAPON"));
										return false;
									}
									
									if (item2->GetLevelLimit() < 65)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_OPEN_SOCKET_WARNING_LV65"));
										return false;
									}
									
									if (item2->GetSocket(3) > 0)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_OPEN_SOCKET_WARNING_SOCKET3"));
										return false;
									}
																		
									item2->SetSocket(3, 1);
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("SUCCESS_OPEN_SOCKET"));
									item->SetCount(item->GetCount()-1);
								}
								break;
								
							case 80019:
								{
									LPITEM item2;
									
									if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
										return false;
									
									if (item2->IsExchanging() || item2->IsEquipped())
										return false;
									
									if (item2->GetType() != ITEM_ARMOR)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_OPEN_SOCKET_ONLY_ARMOR"));
										return false;
									}
									
									if (item2->GetLevelLimit() < 65)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_OPEN_SOCKET_WARNING_LV65"));
										return false;
									}
									
									if (item2->GetSocket(3) > 0)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_OPEN_SOCKET_WARNING_SOCKET3"));
										return false;
									}
									
									item2->SetSocket(3, 1);
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("SUCCESS_OPEN_SOCKET"));
									item->SetCount(item->GetCount()-1);
								}
								break;
								
							case 80020:
								{
									LPITEM item2;
									
									if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
										return false;
									
									if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
										return false;
									
									if (item2->GetType() != ITEM_ARMOR && item2->GetType() != ITEM_WEAPON)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_OPEN_SOCKET_ONLY_ARMOR_AND_WEAPON"));
										return false;
									}
									
									if (item2->GetLevelLimit() < 65)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_OPEN_SOCKET_WARNING_LV65"));
										return false;
									}
									
									if (item2->GetSocket(3) > 0)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_OPEN_SOCKET_WARNING_SOCKET3"));
										return false;
									}
									
									item2->SetSocket(3, 1);
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("SUCCESS_OPEN_SOCKET"));
									item->SetCount(item->GetCount()-1);
								}
								break;
#endif

							case ITEM_AUTO_HP_RECOVERY_S:
							case ITEM_AUTO_HP_RECOVERY_M:
							case ITEM_AUTO_HP_RECOVERY_L:
							case ITEM_AUTO_HP_RECOVERY_X:
							case ITEM_AUTO_SP_RECOVERY_S:
							case ITEM_AUTO_SP_RECOVERY_M:
							case ITEM_AUTO_SP_RECOVERY_L:
							case ITEM_AUTO_SP_RECOVERY_X:
							// 무시무시하지만 이전에 하던 걸 고치기는 무섭고...
							// 그래서 그냥 하드 코딩. 선물 상자용 자동물약 아이템들.
							case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS: 
							case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S: 
							case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS: 
							case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
							case FUCKING_BRAZIL_ITEM_AUTO_SP_RECOVERY_S:
							case FUCKING_BRAZIL_ITEM_AUTO_HP_RECOVERY_S:
								{
									if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련장에서 사용하실 수 없습니다."));
										return false;
									}

									EAffectTypes type = AFFECT_NONE;
									bool isSpecialPotion = false;

									switch (item->GetVnum())
									{
										case ITEM_AUTO_HP_RECOVERY_X:
											isSpecialPotion = true;

										case ITEM_AUTO_HP_RECOVERY_S:
										case ITEM_AUTO_HP_RECOVERY_M:
										case ITEM_AUTO_HP_RECOVERY_L:
										case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
										case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
										case FUCKING_BRAZIL_ITEM_AUTO_HP_RECOVERY_S:
											type = AFFECT_AUTO_HP_RECOVERY;
											break;

										case ITEM_AUTO_SP_RECOVERY_X:
											isSpecialPotion = true;

										case ITEM_AUTO_SP_RECOVERY_S:
										case ITEM_AUTO_SP_RECOVERY_M:
										case ITEM_AUTO_SP_RECOVERY_L:
										case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
										case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
										case FUCKING_BRAZIL_ITEM_AUTO_SP_RECOVERY_S:
											type = AFFECT_AUTO_SP_RECOVERY;
											break;
									}

									if (AFFECT_NONE == type)
										break;

									if (item->GetCount() > 1)
									{
										int pos = GetEmptyInventory(item->GetSize());

										if (-1 == pos)
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지품에 빈 공간이 없습니다."));
											break;
										}

										item->SetCount( item->GetCount() - 1 );

										LPITEM item2 = ITEM_MANAGER::instance().CreateItem( item->GetVnum(), 1 );
										item2->AddToCharacter(this, TItemPos(INVENTORY, pos));

										if (item->GetSocket(1) != 0)
										{
											item2->SetSocket(1, item->GetSocket(1));
										}

										item = item2;
									}

									CAffect* pAffect = FindAffect( type );

									if (NULL == pAffect)
									{
										EPointTypes bonus = POINT_NONE;

										if (true == isSpecialPotion)
										{
											if (type == AFFECT_AUTO_HP_RECOVERY)
											{
												bonus = POINT_MAX_HP_PCT;
											}
											else if (type == AFFECT_AUTO_SP_RECOVERY)
											{
												bonus = POINT_MAX_SP_PCT;
											}
										}

										AddAffect( type, bonus, 4, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);

										item->Lock(true);
										item->SetSocket(0, true);

										AutoRecoveryItemProcess( type );
									}
									else
									{
										if (item->GetID() == pAffect->dwFlag)
										{
											RemoveAffect( pAffect );

											item->Lock(false);
											item->SetSocket(0, false);
										}
										else
										{
											LPITEM old = FindItemByID( pAffect->dwFlag );

											if (NULL != old)
											{
												old->Lock(false);
												old->SetSocket(0, false);
											}

											RemoveAffect( pAffect );

											EPointTypes bonus = POINT_NONE;

											if (true == isSpecialPotion)
											{
												if (type == AFFECT_AUTO_HP_RECOVERY)
												{
													bonus = POINT_MAX_HP_PCT;
												}
												else if (type == AFFECT_AUTO_SP_RECOVERY)
												{
													bonus = POINT_MAX_SP_PCT;
												}
											}

											AddAffect( type, bonus, 4, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);

											item->Lock(true);
											item->SetSocket(0, true);

											AutoRecoveryItemProcess( type );
										}
									}
								}
								break;
						}
						break;

					case USE_CLEAR:
						{
							switch (item->GetVnum())
							{
#ifdef WJ_WOLFMAN_CHARACTER
								case 27124: // Bandage
									RemoveBleeding();
									break;
#endif
								case 27874: // Izgara Levrek
								default:
									RemoveBadAffect();
									break;
							}
							item->SetCount(item->GetCount() - 1);
						}
						break;

					case USE_INVISIBILITY:
						{
							if (item->GetVnum() == 70026)
							{
								quest::CQuestManager& q = quest::CQuestManager::instance();
								quest::PC* pPC = q.GetPC(GetPlayerID());

								if (pPC != NULL)
								{
									int last_use_time = pPC->GetFlag("mirror_of_disapper.last_use_time");

									if (get_global_time() - last_use_time < 10*60)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아직 사용할 수 없습니다."));
										return false;
									}

									pPC->SetFlag("mirror_of_disapper.last_use_time", get_global_time());
								}
							}

							AddAffect(AFFECT_INVISIBILITY, POINT_NONE, 0, AFF_INVISIBILITY, 300, 0, true);
							item->SetCount(item->GetCount() - 1);
						}
						break;

					case USE_POTION_NODELAY:
						{
							if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
							{
								if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련장에서 사용하실 수 없습니다."));
									return false;
								}

								switch (item->GetVnum())
								{
									case 70020 :
									case 71018 :
									case 71019 :
									case 71020 :
										if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
										{
											if (m_nPotionLimit <= 0)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("사용 제한량을 초과하였습니다."));
												return false;
											}
										}
										break;

									default :
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련장에서 사용하실 수 없습니다."));
										return false;
								}
							}

							bool used = false;

							if (item->GetValue(0) != 0) // HP 절대값 회복
							{
								if (GetHP() < GetMaxHP())
								{
									PointChange(POINT_HP, item->GetValue(0) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
									EffectPacket(SE_HPUP_RED);
									used = TRUE;
								}
							}

							if (item->GetValue(1) != 0)	// SP 절대값 회복
							{
								if (GetSP() < GetMaxSP())
								{
									PointChange(POINT_SP, item->GetValue(1) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
									EffectPacket(SE_SPUP_BLUE);
									used = TRUE;
								}
							}

							if (item->GetValue(3) != 0) // HP % 회복
							{
								if (GetHP() < GetMaxHP())
								{
									PointChange(POINT_HP, item->GetValue(3) * GetMaxHP() / 100);
									EffectPacket(SE_HPUP_RED);
									used = TRUE;
								}
							}

							if (item->GetValue(4) != 0) // SP % 회복
							{
								if (GetSP() < GetMaxSP())
								{
									PointChange(POINT_SP, item->GetValue(4) * GetMaxSP() / 100);
									EffectPacket(SE_SPUP_BLUE);
									used = TRUE;
								}
							}

							if (used)
							{
								if (item->GetVnum() == 50085 || item->GetVnum() == 50086)
								{
									if (test_server)
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("월병 또는 종자 를 사용하였습니다"));
									SetUseSeedOrMoonBottleTime();
								}
								if (GetDungeon())
									GetDungeon()->UsePotion(this);

								if (GetWarMap())
									GetWarMap()->UsePotion(this, item);

								m_nPotionLimit--;

								//RESTRICT_USE_SEED_OR_MOONBOTTLE
								item->SetCount(item->GetCount() - 1);
								//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
							}
						}
						break;

					case USE_POTION:
						if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
						{
							if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련장에서 사용하실 수 없습니다."));
								return false;
							}
						
							switch (item->GetVnum())
							{
								case 27001 :
								case 27002 :
								case 27003 :
								case 27004 :
								case 27005 :
								case 27006 :
									if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
									{
										if (m_nPotionLimit <= 0)
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("사용 제한량을 초과하였습니다."));
											return false;
										}
									}
									break;

								default :
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련장에서 사용하실 수 없습니다."));
									return false;
							}
						}
						
						if (item->GetValue(1) != 0)
						{
							if (GetPoint(POINT_SP_RECOVERY) + GetSP() >= GetMaxSP())
							{
								return false;
							}

							PointChange(POINT_SP_RECOVERY, item->GetValue(1) * MIN(200, (100 + GetPoint(POINT_POTION_BONUS))) / 100);
							StartAffectEvent();
							EffectPacket(SE_SPUP_BLUE);
						}

						if (item->GetValue(0) != 0)
						{
							if (GetPoint(POINT_HP_RECOVERY) + GetHP() >= GetMaxHP())
							{
								return false;
							}

							PointChange(POINT_HP_RECOVERY, item->GetValue(0) * MIN(200, (100 + GetPoint(POINT_POTION_BONUS))) / 100);
							StartAffectEvent();
							EffectPacket(SE_HPUP_RED);
						}

						if (GetDungeon())
							GetDungeon()->UsePotion(this);

						if (GetWarMap())
							GetWarMap()->UsePotion(this, item);

						item->SetCount(item->GetCount() - 1);
						m_nPotionLimit--;
						break;

					case USE_POTION_CONTINUE:
						{
							if (item->GetValue(0) != 0)
							{
								AddAffect(AFFECT_HP_RECOVER_CONTINUE, POINT_HP_RECOVER_CONTINUE, item->GetValue(0), 0, item->GetValue(2), 0, true);
							}
							else if (item->GetValue(1) != 0)
							{
								AddAffect(AFFECT_SP_RECOVER_CONTINUE, POINT_SP_RECOVER_CONTINUE, item->GetValue(1), 0, item->GetValue(2), 0, true);
							}
							else
								return false;
						}

						if (GetDungeon())
							GetDungeon()->UsePotion(this);

						if (GetWarMap())
							GetWarMap()->UsePotion(this, item);

						item->SetCount(item->GetCount() - 1);
						break;

					case USE_ABILITY_UP:
						{
							switch (item->GetValue(0))
							{
								case APPLY_MOV_SPEED:
									if (FindAffect(AFFECT_MOV_SPEED)) // @fixme54
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 효과가 걸려 있습니다."));
										return false;
									}
									AddAffect(AFFECT_MOV_SPEED, POINT_MOV_SPEED, item->GetValue(2), AFF_MOV_SPEED_POTION, item->GetValue(1), 0, true);
#ifdef ENABLE_EFFECT_EXTRAPOT
									EffectPacket(SE_DXUP_PURPLE);
#endif
									break;

								case APPLY_ATT_SPEED:
									if (FindAffect(AFFECT_ATT_SPEED)) // @fixme54
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 효과가 걸려 있습니다."));
										return false;
									}
									AddAffect(AFFECT_ATT_SPEED, POINT_ATT_SPEED, item->GetValue(2), AFF_ATT_SPEED_POTION, item->GetValue(1), 0, true);
#ifdef ENABLE_EFFECT_EXTRAPOT
									EffectPacket(SE_SPEEDUP_GREEN);
#endif
									break;

								case APPLY_STR:
									AddAffect(AFFECT_STR, POINT_ST, item->GetValue(2), 0, item->GetValue(1), 0, true);
									break;

								case APPLY_DEX:
									AddAffect(AFFECT_DEX, POINT_DX, item->GetValue(2), 0, item->GetValue(1), 0, true);
									break;

								case APPLY_CON:
									AddAffect(AFFECT_CON, POINT_HT, item->GetValue(2), 0, item->GetValue(1), 0, true);
									break;

								case APPLY_INT:
									AddAffect(AFFECT_INT, POINT_IQ, item->GetValue(2), 0, item->GetValue(1), 0, true);
									break;

								case APPLY_CAST_SPEED:
									AddAffect(AFFECT_CAST_SPEED, POINT_CASTING_SPEED, item->GetValue(2), 0, item->GetValue(1), 0, true);
									break;

								case APPLY_ATT_GRADE_BONUS:
									AddAffect(AFFECT_ATT_GRADE, POINT_ATT_GRADE_BONUS, 
											item->GetValue(2), 0, item->GetValue(1), 0, true);
									break;

								case APPLY_DEF_GRADE_BONUS:
									AddAffect(AFFECT_DEF_GRADE, POINT_DEF_GRADE_BONUS,
											item->GetValue(2), 0, item->GetValue(1), 0, true);
									break;
							}
						}

						if (GetDungeon())
							GetDungeon()->UsePotion(this);

						if (GetWarMap())
							GetWarMap()->UsePotion(this, item);

						item->SetCount(item->GetCount() - 1);
						break;

					case USE_TALISMAN:
						{
							const int TOWN_PORTAL	= 1;
							const int MEMORY_PORTAL = 2;


							// gm_guild_build, oxevent 맵에서 귀환부 귀환기억부 를 사용못하게 막음
							if (GetMapIndex() == 200 || GetMapIndex() == 113)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("현재 위치에서 사용할 수 없습니다."));
								return false;
							}

							if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("대련 중에는 이용할 수 없는 물품입니다."));
								return false;
							}

							if (m_pkWarpEvent)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이동할 준비가 되어있음으로 귀환부를 사용할수 없습니다"));
								return false;
							}

							// CONSUME_LIFE_WHEN_USE_WARP_ITEM
							int consumeLife = CalculateConsume(this);

							if (consumeLife < 0)
								return false;
							// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

							if (item->GetValue(0) == TOWN_PORTAL) // 귀환부
							{
								if (item->GetSocket(0) == 0)
								{
									if (!GetDungeon())
										if (!GiveRecallItem(item))
											return false;

									PIXEL_POSITION posWarp;

									if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(GetMapIndex(), GetEmpire(), posWarp))
									{
										// CONSUME_LIFE_WHEN_USE_WARP_ITEM
										PointChange(POINT_HP, -consumeLife, false);
										// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

										WarpSet(posWarp.x, posWarp.y);
									}
									else
									{
										sys_err("CHARACTER::UseItem : cannot find spawn position (name %s, %d x %d)", GetName(), GetX(), GetY());
									}
								}
								else
								{
									if (test_server)
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("원래 위치로 복귀"));	

									ProcessRecallItem(item);
								}
							}
							else if (item->GetValue(0) == MEMORY_PORTAL) // 귀환기억부
							{
								if (item->GetSocket(0) == 0)
								{
									if (GetDungeon())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("던전 안에서는 %s%s 사용할 수 없습니다."),
												item->GetName(),
												"");
										return false;
									}

									if (!GiveRecallItem(item))
										return false;
								}
								else
								{
									// CONSUME_LIFE_WHEN_USE_WARP_ITEM
									PointChange(POINT_HP, -consumeLife, false);
									// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

									ProcessRecallItem(item);
								}
							}
						}
						break;

					case USE_TUNING:
					case USE_DETACHMENT:
						{
							LPITEM item2;

							if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
								return false;

							if (item2->IsExchanging() || item2->IsEquipped()) // @fixme14
								return false;
						
#ifdef WJ_TIME_LIMIT_WEAPON_SYSTEM						
							TItemTable * table = ITEM_MANAGER::instance().GetTable(item2->GetVnum());
							
							for (int i=0 ; i < ITEM_LIMIT_MAX_NUM ; i++)
							{
								if (LIMIT_REAL_TIME == table->aLimits->bType)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("cannot use detachment time item."));
									return false;
								}
							}
#endif
								
#ifdef WJ_CHANGELOOK_SYSTEM
							if (item->GetValue(0) == CL_CLEAN_ATTR_VALUE0)
							{
								if (!CleanTransmutation(item, item2))
									return false;
								
								return true;
							}
#endif
	
							if (item2->GetVnum() >= 28330 && item2->GetVnum() <= 28343) // 영석+3
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("+3 영석은 이 아이템으로 개량할 수 없습니다"));
								return false;
							}
							
							if (item2->GetVnum() >= 28430 && item2->GetVnum() <= 28443)  // 영석+4
							{
								if (item->GetVnum() == 71056) // 청룡의숨결
								{
									RefineItem(item, item2);
								}
								else
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("영석은 이 아이템으로 개량할 수 없습니다"));
								}
							}
							else
							{
								RefineItem(item, item2);
							}
						}
						break;
						
#ifdef WJ_ENCHANT_COSTUME_SYSTEM
						//  ACCESSORY_REFINE & ADD/CHANGE_ATTRIBUTES
					case USE_CHANGE_COSTUME_ATTR:// Change costume attributes
					case USE_RESET_COSTUME_ATTR:// Change & add costume attributes
						{
							LPITEM item2;
							if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
								return false;

							if (item2->IsEquipped())
								return false;

							if (item2->IsExchanging())
								return false;

#ifdef __ACCE_COSTUME_SYSTEM__
							if (ITEM_COSTUME != item2->GetType() && COSTUME_HAIR < item2->GetSubType())
#else
							if (ITEM_COSTUME != item2->GetType())
#endif
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 항목과 항목의 속성을 변경할 수 없습니다."));
								return false;
							}

							if (item2->GetAttributeSetIndex() == -1)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
								return false;
							}

							switch (item->GetSubType())
							{
								case USE_CHANGE_COSTUME_ATTR:
									{
										if (item2->GetAttributeCount() == 0)
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변경할 속성이 없습니다."));
											return false;
										}

										item2->ChangeAttribute(aiCostumeAttributeLevelPercent);

										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경하였습니다."));
										{
											char buf[21];
											snprintf(buf, sizeof(buf), "%u", item2->GetID());
											LogManager::instance().ItemLog(this, item, "CHANGE_COSTUME_ATTR", buf);
										}

										item->SetCount(item->GetCount() - 1);
									}
									break;

								case USE_RESET_COSTUME_ATTR:
									{
										item2->ClearAttribute();

										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());

										BYTE i;
										for(i=0; i<COSTUME_ATTRIBUTE_MAX_NUM; i++)
										{
											char result[64];
											if (number(1, 100) <= aiCostumeAttributeAddPercent[item2->GetAttributeCount()])
											{
												snprintf(result, sizeof(result), "RESET_COSTUME_ATTR_SUCCESS%d", i);
												item2->AddAttribute();
												int iAddedIdx = item2->GetAttributeCount()-1;
												LogManager::instance().ItemLog(
														GetPlayerID(),
														item2->GetAttributeType(iAddedIdx),
														item2->GetAttributeValue(iAddedIdx),
														item->GetID(),
														result,
														buf,
														GetDesc()->GetHostName(),
														item->GetOriginalVnum());
											}
											else
											{
												snprintf(result, sizeof(result), "RESET_COSTUME_ATTR_FAIL%d", i);
												LogManager::instance().ItemLog(this, item, result, buf);
												break;
											}
										}

										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경하였습니다."));
										item->SetCount(item->GetCount() - 1);
									}
							}
						}
						break;
#endif

#ifdef WJ_CHANGE_ATTRIBUTE_PLUS
					case USE_CHANGE_ATTRIBUTE3:
						{
							if (GetEnchantAttr() < 5555)
								return false;

							LPITEM item2;
							if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
								return false;

							if (item2->IsEquipped())
								BuffOnAttr_RemoveBuffsFromItem(item2);

#ifdef WJ_SOULBINDING_SYSTEM
							if (item2->IsBind() && item->GetSubType() != USE_PUT_INTO_BELT_SOCKET && item->GetSubType() != USE_PUT_INTO_RING_SOCKET && item->GetSubType() != USE_PUT_INTO_ACCESSORY_SOCKET && item->GetSubType() != USE_ADD_ACCESSORY_SOCKET)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't do this because this item is binded."));
								return false;
							}

							if (item2->IsUntilBind() && item->GetSubType() != USE_PUT_INTO_BELT_SOCKET && item->GetSubType() != USE_PUT_INTO_RING_SOCKET && item->GetSubType() != USE_PUT_INTO_ACCESSORY_SOCKET && item->GetSubType() != USE_ADD_ACCESSORY_SOCKET)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't do this because this until unbind process."));
								return false;
							}
#endif

							if (ITEM_COSTUME == item2->GetType())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
								return false;
							}

							if (item2->IsExchanging() || item2->IsEquipped())
								return false;

							if (item2->GetAttributeSetIndex() == -1)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
								return false;
							}

							if (item2->GetAttributeCount() == 0)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변경할 속성이 없습니다."));
								return false;
							}

							newEnchant_type1 = 0;
							newEnchant_value1 = 0;
							newEnchant_type2 = 0;
							newEnchant_value2 = 0;
							newEnchant_type3 = 0;
							newEnchant_value3 = 0;
							newEnchant_type4 = 0;
							newEnchant_value4 = 0;
							newEnchant_type5 = 0;
							newEnchant_value5 = 0;
							SetEnchantAttr(item2->GetCell());
							item2->PrepareAttribute();
							ChatPacket(CHAT_TYPE_COMMAND, "EnchantAttr_open #%d#%d#%d#%d#%d#%d#%d#%d#%d#%d#%d#", DestCell.cell, newEnchant_type1, newEnchant_value1, newEnchant_type2, newEnchant_value2, newEnchant_type3, newEnchant_value3, newEnchant_type4, newEnchant_value4, newEnchant_type5, newEnchant_value5);
							{
								char buf[128];
								snprintf(buf, sizeof(buf), "%u", item2->GetID());
								LogManager::instance().ItemLog(this, item, "CHANGE_ATTRIBUTE3 WAIT_PLAYER", buf);
							}

							item2->Lock(true);
							item->SetCount(item->GetCount() - 1);
						}
						break;
#endif

#ifdef WJ_CHANGE_ATTRIBUTE_MINUS
					case USE_CHANGE_ATTRIBUTE4:
						{
							if (GetEnchant2Attr() < 5555 || GetEnchant2Slot() < 5555)
								return false;

							LPITEM item2;
							if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
								return false;

							if (item2->IsEquipped())
								BuffOnAttr_RemoveBuffsFromItem(item2);

#ifdef WJ_SOULBINDING_SYSTEM
							if (item2->IsBind() && item->GetSubType() != USE_PUT_INTO_BELT_SOCKET && item->GetSubType() != USE_PUT_INTO_RING_SOCKET && item->GetSubType() != USE_PUT_INTO_ACCESSORY_SOCKET && item->GetSubType() != USE_ADD_ACCESSORY_SOCKET)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't do this because this item is binded."));
								return false;
							}

							if (item2->IsUntilBind() && item->GetSubType() != USE_PUT_INTO_BELT_SOCKET && item->GetSubType() != USE_PUT_INTO_RING_SOCKET && item->GetSubType() != USE_PUT_INTO_ACCESSORY_SOCKET && item->GetSubType() != USE_ADD_ACCESSORY_SOCKET)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't do this because this until unbind process."));
								return false;
							}
#endif

							if (ITEM_COSTUME == item2->GetType())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
								return false;
							}

							if (item2->IsExchanging() || item2->IsEquipped())
								return false;

							if (item2->GetAttributeSetIndex() == -1)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
								return false;
							}

							if (item2->GetAttributeCount() == 0)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변경할 속성이 없습니다."));
								return false;
							}

							SetEnchant2Slot(item->GetCell());
							SetEnchant2Attr(item2->GetCell());
							ChatPacket(CHAT_TYPE_COMMAND, "Enchant2Attr_open #%d#", DestCell.cell);
							{
								char buf[128];
								snprintf(buf, sizeof(buf), "%u", item2->GetID());
								LogManager::instance().ItemLog(this, item, "CHANGE_ATTRIBUTE4 WAIT_PLAYER", buf);
							}

							item->Lock(true);
							item2->Lock(true);
						}
						break;
#endif

						//  ACCESSORY_REFINE & ADD/CHANGE_ATTRIBUTES
					case USE_PUT_INTO_BELT_SOCKET:
					case USE_PUT_INTO_RING_SOCKET:
					case USE_PUT_INTO_ACCESSORY_SOCKET:
					case USE_ADD_ACCESSORY_SOCKET:
					case USE_CLEAN_SOCKET:
					case USE_CHANGE_ATTRIBUTE:
					case USE_CHANGE_ATTRIBUTE2 :
					case USE_ADD_ATTRIBUTE:
					case USE_ADD_ATTRIBUTE2:
						{
							LPITEM item2;
							if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
								return false;

							if (item2->IsEquipped())
							{
								BuffOnAttr_RemoveBuffsFromItem(item2);
							}
							
#ifdef WJ_SOULBINDING_SYSTEM
							if (item2->IsBind())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't do this because this item is binded."));
								return false;
							}
							
							if (item2->IsUntilBind())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't do this because this until unbind process."));
								return false;
							}
#endif

							// [NOTE] 코스튬 아이템에는 아이템 최초 생성시 랜덤 속성을 부여하되, 재경재가 등등은 막아달라는 요청이 있었음.
							// 원래 ANTI_CHANGE_ATTRIBUTE 같은 아이템 Flag를 추가하여 기획 레벨에서 유연하게 컨트롤 할 수 있도록 할 예정이었으나
							// 그딴거 필요없으니 닥치고 빨리 해달래서 그냥 여기서 막음... -_-
							if (ITEM_COSTUME == item2->GetType())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
								return false;
							}

							if (item2->IsExchanging() || item2->IsEquipped()) // @fixme14
								return false;

							switch (item->GetSubType())
							{
								case USE_CLEAN_SOCKET:
									{
										int i;
										for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
										{
											if (item2->GetSocket(i) == ITEM_BROKEN_METIN_VNUM)
												break;
										}

										if (i == ITEM_SOCKET_MAX_NUM)
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("청소할 석이 박혀있지 않습니다."));
											return false;
										}

										int j = 0;

										for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
										{
											if (item2->GetSocket(i) != ITEM_BROKEN_METIN_VNUM && item2->GetSocket(i) != 0)
												item2->SetSocket(j++, item2->GetSocket(i));
										}

										for (; j < ITEM_SOCKET_MAX_NUM; ++j)
										{
											if (item2->GetSocket(j) > 0)
												item2->SetSocket(j, 1);
										}

										{
											char buf[21];
											snprintf(buf, sizeof(buf), "%u", item2->GetID());
											LogManager::instance().ItemLog(this, item, "CLEAN_SOCKET", buf);
										}

										item->SetCount(item->GetCount() - 1);

									}
									break;

								case USE_CHANGE_ATTRIBUTE :
								case USE_CHANGE_ATTRIBUTE2 : // @fixme16
								
									if (item2->GetAttributeSetIndex() == -1)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}

									if (item2->GetAttributeCount() == 0)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변경할 속성이 없습니다."));
										return false;
									}
									
									if (item2->IsEquipped()) // @fixme36
										return false;

									if ((GM_PLAYER == GetGMLevel()) && (false == test_server) && (false == g_bDisableItemBonusChangeTime))
									{
										//
										// Event Flag 를 통해 이전에 아이템 속성 변경을 한 시간으로 부터 충분한 시간이 흘렀는지 검사하고
										// 시간이 충분히 흘렀다면 현재 속성변경에 대한 시간을 설정해 준다.
										//

										DWORD dwChangeItemAttrCycle = quest::CQuestManager::instance().GetEventFlag(msc_szChangeItemAttrCycleFlag);
										if (dwChangeItemAttrCycle < msc_dwDefaultChangeItemAttrCycle)
											dwChangeItemAttrCycle = msc_dwDefaultChangeItemAttrCycle;

										quest::PC* pPC = quest::CQuestManager::instance().GetPC(GetPlayerID());

										if (pPC)
										{
											DWORD dwNowMin = get_global_time() / 60;

											DWORD dwLastChangeItemAttrMin = pPC->GetFlag(msc_szLastChangeItemAttrFlag);

											if (dwLastChangeItemAttrMin + dwChangeItemAttrCycle > dwNowMin)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 바꾼지 %d분 이내에는 다시 변경할 수 없습니다.(%d 분 남음)"),
														dwChangeItemAttrCycle, dwChangeItemAttrCycle - (dwNowMin - dwLastChangeItemAttrMin));
												return false;
											}

											pPC->SetFlag(msc_szLastChangeItemAttrFlag, dwNowMin);
										}
									}

									if (item->GetSubType() == USE_CHANGE_ATTRIBUTE2)
									{
										int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] = 
										{
											0, 0, 30, 40, 3
										};

										item2->ChangeAttribute(aiChangeProb);
									}
									else if (item->GetVnum() == 76014)
									{
										int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] = 
										{
											0, 10, 50, 39, 1
										};

										item2->ChangeAttribute(aiChangeProb);
									}

									else
									{
										// 연재경 특수처리
										// 절대로 연재가 추가 안될거라 하여 하드 코딩함.
										if (item->GetVnum() == 71151 || item->GetVnum() == 76023)
										{
											if ((item2->GetType() == ITEM_WEAPON)
												|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_BODY))
											{
												bool bCanUse = true;
												for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
												{
													if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 40)
													{
														bCanUse = false;
														break;
													}
												}
												if (false == bCanUse)
												{
													ChatPacket(CHAT_TYPE_INFO, LC_TEXT("적용 레벨보다 높아 사용이 불가능합니다."));
													break;
												}
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("무기와 갑옷에만 사용 가능합니다."));
												break;
											}
										}
										item2->ChangeAttribute();
									}

									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경하였습니다."));
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());
										LogManager::instance().ItemLog(this, item, "CHANGE_ATTRIBUTE", buf);
									}

									item->SetCount(item->GetCount() - 1);
									break;

								case USE_ADD_ATTRIBUTE :
								
									if (ITEM_COSTUME == item2->GetType())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}
								
									if (item2->GetAttributeSetIndex() == -1)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}
									
									if (item2->IsEquipped()) // @fixme36
										return false;

									if (item2->GetAttributeCount() < 4)
									{
										// 연재가 특수처리
										// 절대로 연재가 추가 안될거라 하여 하드 코딩함.
										if (item->GetVnum() == 71152 || item->GetVnum() == 76024)
										{
											if ((item2->GetType() == ITEM_WEAPON)
#ifdef ENABLE_SWITCH_ALL
												|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_WRIST)
												|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_FOOTS)
												|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_NECK)
												|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_EAR)
												|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_HEAD)
												|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_SHIELD)
#endif
												|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_BODY))
											{
												bool bCanUse = true;
												for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
												{
													if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 40)
													{
														bCanUse = false;
														break;
													}
												}
												if (false == bCanUse)
												{
													ChatPacket(CHAT_TYPE_INFO, LC_TEXT("적용 레벨보다 높아 사용이 불가능합니다."));
													break;
												}
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("무기와 갑옷에만 사용 가능합니다."));
												break;
											}
										}
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());

										if (number(1, 100) <= aiItemAttributeAddPercent[item2->GetAttributeCount()])
										{
											item2->AddAttribute();
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성 추가에 성공하였습니다."));

											int iAddedIdx = item2->GetAttributeCount() - 1;
											LogManager::instance().ItemLog(
													GetPlayerID(), 
													item2->GetAttributeType(iAddedIdx),
													item2->GetAttributeValue(iAddedIdx),
													item->GetID(), 
													"ADD_ATTRIBUTE_SUCCESS",
													buf,
													GetDesc()->GetHostName(),
													item->GetOriginalVnum());
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성 추가에 실패하였습니다."));
											LogManager::instance().ItemLog(this, item, "ADD_ATTRIBUTE_FAIL", buf);
										}

										item->SetCount(item->GetCount() - 1);
									}
									else
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("더이상 이 아이템을 이용하여 속성을 추가할 수 없습니다."));
									}
									break;

								case USE_ADD_ATTRIBUTE2 :
									// 축복의 구슬 
									// 재가비서를 통해 속성을 4개 추가 시킨 아이템에 대해서 하나의 속성을 더 붙여준다.
									if (ITEM_COSTUME == item2->GetType())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}
									
									if (item2->GetAttributeSetIndex() == -1)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}
									
									if (item2->IsEquipped()) // @fixme36
										return false;

									// 속성이 이미 4개 추가 되었을 때만 속성을 추가 가능하다.
									if (item2->GetAttributeCount() == 4)
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());

										if (number(1, 100) <= aiItemAttributeAddPercent[item2->GetAttributeCount()])
										{
											item2->AddAttribute();
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성 추가에 성공하였습니다."));

											int iAddedIdx = item2->GetAttributeCount() - 1;
											LogManager::instance().ItemLog(
													GetPlayerID(),
													item2->GetAttributeType(iAddedIdx),
													item2->GetAttributeValue(iAddedIdx),
													item->GetID(),
													"ADD_ATTRIBUTE2_SUCCESS",
													buf,
													GetDesc()->GetHostName(),
													item->GetOriginalVnum());
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("속성 추가에 실패하였습니다."));
											LogManager::instance().ItemLog(this, item, "ADD_ATTRIBUTE2_FAIL", buf);
										}

										item->SetCount(item->GetCount() - 1);
									}
									else if (item2->GetAttributeCount() == 5)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("더 이상 이 아이템을 이용하여 속성을 추가할 수 없습니다."));
									}
									else if (item2->GetAttributeCount() < 4)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("먼저 재가비서를 이용하여 속성을 추가시켜 주세요."));
									}
									else
									{
										// wtf ?!
										sys_err("ADD_ATTRIBUTE2 : Item has wrong AttributeCount(%d)", item2->GetAttributeCount());
									}
									break;

								case USE_ADD_ACCESSORY_SOCKET:
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());

										if (item2->IsAccessoryForSocket())
										{
											if (item2->GetAccessorySocketMaxGrade() < ITEM_ACCESSORY_SOCKET_MAX_NUM)
											{
												if (number(1, 100) <= 50)
												{
													item2->SetAccessorySocketMaxGrade(item2->GetAccessorySocketMaxGrade() + 1);
													ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소켓이 성공적으로 추가되었습니다."));
													LogManager::instance().ItemLog(this, item, "ADD_SOCKET_SUCCESS", buf);
												}
												else
												{
													ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소켓 추가에 실패하였습니다."));
													LogManager::instance().ItemLog(this, item, "ADD_SOCKET_FAIL", buf);
												}

												item->SetCount(item->GetCount() - 1);
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 액세서리에는 더이상 소켓을 추가할 공간이 없습니다."));
											}
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템으로 소켓을 추가할 수 없는 아이템입니다."));
										}
									}
									break;

								case USE_PUT_INTO_BELT_SOCKET:
								case USE_PUT_INTO_ACCESSORY_SOCKET:
									if (item2->IsAccessoryForSocket() && item->CanPutInto(item2))
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());

										if (item2->GetAccessorySocketGrade() < item2->GetAccessorySocketMaxGrade())
										{
											if (number(1, 100) <= aiAccessorySocketPutPct[item2->GetAccessorySocketGrade()])
											{
												item2->SetAccessorySocketGrade(item2->GetAccessorySocketGrade() + 1);
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("장착에 성공하였습니다."));
												LogManager::instance().ItemLog(this, item, "PUT_SOCKET_SUCCESS", buf);
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("장착에 실패하였습니다."));
												LogManager::instance().ItemLog(this, item, "PUT_SOCKET_FAIL", buf);
											}

											item->SetCount(item->GetCount() - 1);
										}
										else
										{
											if (item2->GetAccessorySocketMaxGrade() == 0)
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("먼저 다이아몬드로 악세서리에 소켓을 추가해야합니다."));
											else if (item2->GetAccessorySocketMaxGrade() < ITEM_ACCESSORY_SOCKET_MAX_NUM)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 액세서리에는 더이상 장착할 소켓이 없습니다."));
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("다이아몬드로 소켓을 추가해야합니다."));
											}
											else
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 액세서리에는 더이상 보석을 장착할 수 없습니다."));
										}
									}
									else
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템을 장착할 수 없습니다."));
									}
									break;
							}
							if (item2->IsEquipped())
							{
								BuffOnAttr_AddBuffsFromItem(item2);
							}
						}
						break;
						//  END_OF_ACCESSORY_REFINE & END_OF_ADD_ATTRIBUTES & END_OF_CHANGE_ATTRIBUTES
						
#ifdef WJ_SOULBINDING_SYSTEM
					case USE_BIND:
						{
							LPITEM item2;
							if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
								return false;
							
							if (item2->IsEquipped())
							{
								BuffOnAttr_RemoveBuffsFromItem(item2);
							}
							
							if (item2->IsExchanging() || item2->IsEquipped())
								return false;
							
							if (item2->IsBind())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item is already binded."));
								return false;
							}
							
							if (item2->IsUntilBind())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item is until unbind process."));
								return false;
							}
							
							if (IS_SET(item2->GetAntiFlag(), ITEM_ANTIFLAG_BIND))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item can't be binded."));
								return false;
							}
							
							if ((item2->GetType() != ITEM_WEAPON) && (item2->GetType() != ITEM_ARMOR) && (item2->GetType() != ARMOR_BODY))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item can't be binded."));
								return false;
							}
							
							long time_proc = item->GetValue(3);
							item2->Bind(time_proc);
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have bind succesfully this item."));
							item->SetCount(item->GetCount() - 1);
						}
						break;
					case USE_UNBIND:
						{
							LPITEM item2;
							if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
								return false;
							
							if (item2->IsEquipped())
							{
								BuffOnAttr_RemoveBuffsFromItem(item2);
							}
							
							if (item2->IsExchanging() || item2->IsEquipped())
								return false;
							
							if (item2->IsUntilBind())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item is until unbind process."));
								return false;
							}
							
							if (!item2->IsBind())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item is not binded."));
								return false;
							}
							
							long time_proc = item->GetValue(3);
							item2->Bind(time(0) + time_proc);
							item2->StartUnBindingExpireEvent();
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have unbinded succesfully this item, this process require 3 days."));
							item->SetCount(item->GetCount() - 1);
						}
						break;
#endif

					case USE_BAIT:
						{

							if (m_pkFishingEvent)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("낚시 중에 미끼를 갈아끼울 수 없습니다."));
								return false;
							}

							LPITEM weapon = GetWear(WEAR_WEAPON);

							if (!weapon || weapon->GetType() != ITEM_ROD)
								return false;

							if (weapon->GetSocket(2))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 꽂혀있던 미끼를 빼고 %s를 끼웁니다."), item->GetName());
							}
							else
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("낚시대에 %s를 미끼로 끼웁니다."), item->GetName());
							}

							weapon->SetSocket(2, item->GetValue(0));
							item->SetCount(item->GetCount() - 1);
						}
						break;

					case USE_MOVE:
					case USE_TREASURE_BOX:
					case USE_MONEYBAG:
						break;

					case USE_AFFECT :
						{
							if (FindAffect(item->GetValue(0), aApplyInfo[item->GetValue(1)].bPointType))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 효과가 걸려 있습니다."));
							}
							else
							{
								// PC_BANG_ITEM_ADD
								if (item->IsPCBangItem() == true)
								{
									// PC방인지 체크해서 처리
									if (CPCBangManager::instance().IsPCBangIP(GetDesc()->GetHostName()) == false)
									{
										// PC방이 아님!
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템은 PC방에서만 사용할 수 있습니다."));
										return false;
									}
								}
								// END_PC_BANG_ITEM_ADD

								AddAffect(item->GetValue(0), aApplyInfo[item->GetValue(1)].bPointType, item->GetValue(2), 0, item->GetValue(3), 0, false);
								item->SetCount(item->GetCount() - 1);
							}
						}
						break;

					case USE_CREATE_STONE:
						AutoGiveItem(number(28000, 28013));
						item->SetCount(item->GetCount() - 1);
						break;

					// 물약 제조 스킬용 레시피 처리	
					case USE_RECIPE :
						{
							LPITEM pSource1 = FindSpecifyItem(item->GetValue(1));
							DWORD dwSourceCount1 = item->GetValue(2);

							LPITEM pSource2 = FindSpecifyItem(item->GetValue(3));
							DWORD dwSourceCount2 = item->GetValue(4);

							if (dwSourceCount1 != 0)
							{
								if (pSource1 == NULL)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("물약 조합을 위한 재료가 부족합니다."));
									return false;
								}
							}

							if (dwSourceCount2 != 0)
							{
								if (pSource2 == NULL)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("물약 조합을 위한 재료가 부족합니다."));
									return false;
								}
							}

							if (pSource1 != NULL)
							{
								if (pSource1->GetCount() < dwSourceCount1)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("재료(%s)가 부족합니다."), pSource1->GetName());
									return false;
								}

								pSource1->SetCount(pSource1->GetCount() - dwSourceCount1);
							}

							if (pSource2 != NULL)
							{
								if (pSource2->GetCount() < dwSourceCount2)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("재료(%s)가 부족합니다."), pSource2->GetName());
									return false;
								}

								pSource2->SetCount(pSource2->GetCount() - dwSourceCount2);
							}

							LPITEM pBottle = FindSpecifyItem(50901);

							if (!pBottle || pBottle->GetCount() < 1)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("빈 병이 모자릅니다."));
								return false;
							}

							pBottle->SetCount(pBottle->GetCount() - 1);

							if (number(1, 100) > item->GetValue(5))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("물약 제조에 실패했습니다."));
								return false;
							}

							AutoGiveItem(item->GetValue(0));
						}
						break;
				}
			}
			break;

		case ITEM_METIN:
			{
				LPITEM item2;

				if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme14
					return false;
	
#ifdef WJ_TIME_LIMIT_WEAPON_SYSTEM	
				TItemTable * table = ITEM_MANAGER::instance().GetTable(item2->GetVnum());
							
				for (int i=0 ; i < ITEM_LIMIT_MAX_NUM ; i++)
				{
					if (LIMIT_REAL_TIME == table->aLimits->bType)
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("cannot add metin stones time item."));
						return false;
					}
				}
#endif

#ifdef WJ_SOULBINDING_SYSTEM
				if (item2->IsBind())
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't add metin stone this item because item is binded."));
					return false;
				}
				
				if (item2->IsUntilBind())
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't add metin stone this item because item is binded."));
					return false;
				}
#endif

				if (item2->GetType() == ITEM_PICK) return false;
				if (item2->GetType() == ITEM_ROD) return false;

				int i;

				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				{
					DWORD dwVnum;   

					if ((dwVnum = item2->GetSocket(i)) <= 2)
						continue;

					TItemTable * p = ITEM_MANAGER::instance().GetTable(dwVnum);

					if (!p)
						continue;

					if (item->GetValue(5) == p->alValues[5])
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("같은 종류의 메틴석은 여러개 부착할 수 없습니다."));
						return false;
					}
				}

				if (item2->GetType() == ITEM_ARMOR)
				{
					if (!IS_SET(item->GetWearFlag(), WEARABLE_BODY) || !IS_SET(item2->GetWearFlag(), WEARABLE_BODY))
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 메틴석은 장비에 부착할 수 없습니다."));
						return false;
					}
				}
				else if (item2->GetType() == ITEM_WEAPON)
				{
					if (!IS_SET(item->GetWearFlag(), WEARABLE_WEAPON))
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 메틴석은 무기에 부착할 수 없습니다."));
						return false;
					}
				}
				else
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("부착할 수 있는 슬롯이 없습니다."));
					return false;
				}

				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
					if (item2->GetSocket(i) >= 1 && item2->GetSocket(i) <= 2 && item2->GetSocket(i) >= item->GetValue(2))
					{
						// 석 확률
						if (number(1, 100) <= 30)
						{
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("메틴석 부착에 성공하였습니다."));
							item2->SetSocket(i, item->GetVnum());
						}
						else
						{
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("메틴석 부착에 실패하였습니다."));
							item2->SetSocket(i, ITEM_BROKEN_METIN_VNUM);
						}

						LogManager::instance().ItemLog(this, item2, "SOCKET", item->GetName());
						ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (METIN)");
						break;
					}

				if (i == ITEM_SOCKET_MAX_NUM)
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("부착할 수 있는 슬롯이 없습니다."));
			}
			break;

		case ITEM_AUTOUSE:
		case ITEM_MATERIAL:
		case ITEM_SPECIAL:
		case ITEM_TOOL:
		case ITEM_LOTTERY:
			break;

		case ITEM_TOTEM:
			{
				if (!item->IsEquipped())
					EquipItem(item);
			}
			break;

		case ITEM_BLEND:
			// 새로운 약초들
			sys_log(0,"ITEM_BLEND!!");
			if (Blend_Item_find(item->GetVnum()))
			{
				int		affect_type		= AFFECT_BLEND;
				int		apply_type		= aApplyInfo[item->GetSocket(0)].bPointType;
				int		apply_value		= item->GetSocket(1);
				int		apply_duration	= item->GetSocket(2);
				
				if (FindAffect(affect_type, apply_type))
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 효과가 걸려 있습니다."));
				}
				else
				{
					if (FindAffect(AFFECT_EXP_BONUS_EURO_FREE, POINT_RESIST_MAGIC))
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 효과가 걸려 있습니다."));
					}
					else
					{
						AddAffect(affect_type, apply_type, apply_value, 0, apply_duration, 0, false);
						item->SetCount(item->GetCount() - 1);
					}
				}
			}
			break;
		case ITEM_EXTRACT:
			{
				LPITEM pDestItem = GetItem(DestCell);
				if (NULL == pDestItem)
				{
					return false;
				}
				switch (item->GetSubType())
				{
				case EXTRACT_DRAGON_SOUL:
					if (pDestItem->IsDragonSoul())
					{
						return DSManager::instance().PullOut(this, NPOS, pDestItem, item);
					}
					return false;
				case EXTRACT_DRAGON_HEART:
					if (item->GetVnum() == 71053)
					{
						if (pDestItem->IsDragonSoul())
						{
							if (DSManager::instance().IsActiveDragonSoul(pDestItem) == true)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("EQUIPPED_DSS"));
								return false;
							}
							
							pDestItem->SetForceAttribute(0, 0, 0);
							pDestItem->SetForceAttribute(1, 0, 0);
							pDestItem->SetForceAttribute(2, 0, 0);
							pDestItem->SetForceAttribute(3, 0, 0);
							pDestItem->SetForceAttribute(4, 0, 0);
							pDestItem->SetForceAttribute(5, 0, 0);
							pDestItem->SetForceAttribute(6, 0, 0);
							
							bool ret = DSManager::instance().PutAttributes(pDestItem);
							if (ret == true)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("DSS_CHANGE_SUCCES"));
								item->SetCount(item->GetCount()-1);
							}
							else
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("DSS_CHANGE_FAILED"));
							}
						}
					}
					else
					{
						if (pDestItem->IsDragonSoul())
						{
							return DSManager::instance().ExtractDragonHeart(this, pDestItem, item);
						}
					}
					return false;
				default:
					return false;
				}
			}
			break;

		case ITEM_NONE:
			sys_err("Item type NONE %s", item->GetName());
			break;

		default:
			sys_log(0, "UseItemEx: Unknown type %s %d", item->GetName(), item->GetType());
			return false;
	}

	return true;
}

int g_nPortalLimitTime = 10;

bool CHARACTER::UseItem(TItemPos Cell, TItemPos DestCell)
{
	WORD wCell = Cell.cell;
	BYTE window_type = Cell.window_type;
	// WORD wDestCell = DestCell.cell;
	// BYTE bDestInven = DestCell.window_type;
	LPITEM item;

#ifdef WJ_SECURITY_SYSTEM
	if (IsActivateSecurity() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot use item with security key activate"));
		return false;
	}
#endif

	if (!CanHandleItem())
		return false;

	if (!IsValidItemPosition(Cell) || !(item = GetItem(Cell)))
		return false;

	// @fixme49
	if (item->GetType() == ITEM_BELT && CBeltInventoryHelper::IsExistItemInBeltInventory(this))
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이는에러 메시지 이다"));

	sys_log(0, "%s: USE_ITEM %s (inven %d, cell: %d)", GetName(), item->GetName(), window_type, wCell);

	if (item->IsExchanging())
		return false;

	if (!item->CanUsedBy(this))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("군직이 맞지않아 이 아이템을 사용할 수 없습니다."));
		return false;
	}

	if (IsStun())
		return false;

	if (false == FN_check_item_sex(this, item))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("성별이 맞지않아 이 아이템을 사용할 수 없습니다."));
		return false;
	}

	//PREVENT_TRADE_WINDOW
	if (IS_SUMMON_ITEM(item->GetVnum()))
	{
		if (false == IS_SUMMONABLE_ZONE(GetMapIndex()))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("사용할수 없습니다."));
			return false;
		}

		// 경혼반지 사용지 상대방이 SUMMONABLE_ZONE에 있는가는 WarpToPC()에서 체크
		
		//삼거리 관려 맵에서는 귀환부를 막아버린다.
		if (CThreeWayWar::instance().IsThreeWayWarMapIndex(GetMapIndex()))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("삼거리 전투 참가중에는 귀환부,귀환기억부를 사용할수 없습니다."));
			return false;
		}
		int iPulse = thecore_pulse();

		//창고 연후 체크
		if (iPulse - GetSafeboxLoadTime() < PASSES_PER_SEC(g_nPortalLimitTime))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("창고를 연후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), g_nPortalLimitTime);

			if (test_server)
				ChatPacket(CHAT_TYPE_INFO, "[TestOnly]Pulse %d LoadTime %d PASS %d", iPulse, GetSafeboxLoadTime(), PASSES_PER_SEC(g_nPortalLimitTime));
			return false; 
		}

		//거래관련 창 체크
#if defined(WJ_OFFLINESHOP_SYSTEM) and defined(WJ_ACCE_SYSTEM)
		if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen() || GetOfflineShopOwner() || IsAcceWindowOpen())
#elif defined(WJ_OFFLINESHOP_SYSTEM) and !defined(WJ_ACCE_SYSTEM)
		if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen() || GetOfflineShopOwner())
#elif !defined(WJ_OFFLINESHOP_SYSTEM) and defined(WJ_ACCE_SYSTEM)
		if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen() || IsAcceWindowOpen())
#else
		if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen())
#endif
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("거래창,창고 등을 연 상태에서는 귀환부,귀환기억부 를 사용할수 없습니다."));
			return false;
		}

		//PREVENT_REFINE_HACK
		//개량후 시간체크 
		{
			if (iPulse - GetRefineTime() < PASSES_PER_SEC(g_nPortalLimitTime))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 개량후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), g_nPortalLimitTime);
				return false;
			}
		}
		//END_PREVENT_REFINE_HACK
		

		//PREVENT_ITEM_COPY
		{
			if (iPulse - GetMyShopTime() < PASSES_PER_SEC(g_nPortalLimitTime))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("개인상점 사용후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), g_nPortalLimitTime);
				return false;
			}

#ifdef WJ_OFFLINESHOP_SYSTEM
			if (iPulse - GetMyOfflineShopTime() < PASSES_PER_SEC(g_nPortalLimitTime))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("개인상점 사용후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), g_nPortalLimitTime);
				return false;
			}
#endif			
		}
		//END_PREVENT_ITEM_COPY
		

		//귀환부 거리체크
		if (item->GetVnum() != 70302)
		{
			PIXEL_POSITION posWarp;

			int x = 0;
			int y = 0;

			double nDist = 0;
			const double nDistant = 5000.0;
			//귀환기억부 
			if (item->GetVnum() == 22010)
			{
				x = item->GetSocket(0) - GetX();
				y = item->GetSocket(1) - GetY();
			}
			//귀환부
			else if (item->GetVnum() == 22000) 
			{
				SECTREE_MANAGER::instance().GetRecallPositionByEmpire(GetMapIndex(), GetEmpire(), posWarp);

				if (item->GetSocket(0) == 0)
				{
					x = posWarp.x - GetX();
					y = posWarp.y - GetY();
				}
				else
				{
					x = item->GetSocket(0) - GetX();
					y = item->GetSocket(1) - GetY();
				}
			}

			nDist = sqrt(pow((float)x,2) + pow((float)y,2));

			if (nDistant > nDist)
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이동 되어질 위치와 너무 가까워 귀환부를 사용할수 없습니다."));				
				if (test_server)
					ChatPacket(CHAT_TYPE_INFO, "PossibleDistant %f nNowDist %f", nDistant,nDist); 
				return false;
			}
		}

		//PREVENT_PORTAL_AFTER_EXCHANGE
		//교환 후 시간체크
		if (iPulse - GetExchangeTime()  < PASSES_PER_SEC(g_nPortalLimitTime))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("거래 후 %d초 이내에는 귀환부,귀환기억부등을 사용할 수 없습니다."), g_nPortalLimitTime);
			return false;
		}
		//END_PREVENT_PORTAL_AFTER_EXCHANGE

	}

	//보따리 비단 사용시 거래창 제한 체크 
	if ((item->GetVnum() == 50200) || (item->GetVnum() == 71049))
	{
#if defined(WJ_OFFLINESHOP_SYSTEM) and defined(WJ_ACCE_SYSTEM)
		if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen() || GetOfflineShopOwner() || IsAcceWindowOpen())
#elif defined(WJ_OFFLINESHOP_SYSTEM) and !defined(WJ_ACCE_SYSTEM)
		if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen() || GetOfflineShopOwner())
#elif !defined(WJ_OFFLINESHOP_SYSTEM) and defined(WJ_ACCE_SYSTEM)
		if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen() || IsAcceWindowOpen())
#else
		if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen())
#endif
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("거래창,창고 등을 연 상태에서는 보따리,비단보따리를 사용할수 없습니다."));
			return false;
		}

	}
	
#ifdef WJ_OFFLINESHOP_SYSTEM
	if (item->GetVnum() == 71049 && HasOfflineShop())
	{			
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_HAVE_SHOP"));
		return false;
	}
#endif
	//END_PREVENT_TRADE_WINDOW

	if (IS_SET(item->GetFlag(), ITEM_FLAG_LOG)) // 사용 로그를 남기는 아이템 처리
	{
		DWORD vid = item->GetVID();
		DWORD oldCount = item->GetCount();
		DWORD vnum = item->GetVnum();

		char hint[ITEM_NAME_MAX_LEN + 32 + 1];
		int len = snprintf(hint, sizeof(hint) - 32, "%s", item->GetName());

		if (len < 0 || len >= (int) sizeof(hint) - 32)
			len = (sizeof(hint) - 32) - 1;

		bool ret = UseItemEx(item, DestCell);

		if (NULL == ITEM_MANAGER::instance().FindByVID(vid)) // UseItemEx에서 아이템이 삭제 되었다. 삭제 로그를 남김
		{
			LogManager::instance().ItemLog(this, vid, vnum, "REMOVE", hint);
		}
		else if (oldCount != item->GetCount())
		{
			snprintf(hint + len, sizeof(hint) - len, " %u", oldCount - 1);
			LogManager::instance().ItemLog(this, vid, vnum, "USE_ITEM", hint);
		}
		return (ret);
	}
	else
		return UseItemEx(item, DestCell);
}

bool CHARACTER::DropItem(TItemPos Cell, BYTE bCount)
{

#ifdef WJ_SECURITY_SYSTEM	
	if (IsActivateSecurity() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot drop item with security key activate"));
		return false;
	}
#endif
	
#ifdef ENABLE_DROP_ITEM
	LPITEM item = NULL; 

	if (!CanHandleItem())
	{
		if (NULL != DragonSoul_RefineWindow_GetOpener())
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("강화창을 연 상태에서는 아이템을 옮길 수 없습니다."));
		return false;
	}
	
#ifdef ENABLE_NEWSTUFF
	if (0 != g_ItemDropTimeLimitValue)
	{
		if (get_dword_time() < m_dwLastItemDropTime+g_ItemDropTimeLimitValue)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("EVENT_FLAG_WAIT"));
			return false;
		}
	}

	m_dwLastItemDropTime = get_dword_time();
#endif

	if (IsDead())
		return false;

	if (!IsValidItemPosition(Cell) || !(item = GetItem(Cell)))
		return false;

	if (item->IsExchanging())
		return false;

	if (true == item->isLocked())
		return false;

	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
		return false;
	
#ifdef WJ_SOULBINDING_SYSTEM
	if (item->IsBind() || item->IsUntilBind())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't drop this item because is binded!"));
		return false;
	}
#endif

#ifdef WJ_GROWTH_PET_SYSTEM
	if (item->GetVnum() == 55701 || item->GetVnum() == 55702 || item->GetVnum() == 55703 || item->GetVnum() == 55704)
		if (GetNewPetSystem()->IsActivePet())
			return false;
#endif

	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_DROP | ITEM_ANTIFLAG_GIVE))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("버릴 수 없는 아이템입니다."));
		return false;
	}

	if (bCount == 0 || bCount > item->GetCount())
		bCount = item->GetCount();

	SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 1000);	// Quickslot 에서 지움

	LPITEM pkItemToDrop;

	if (bCount == item->GetCount())
	{
		item->RemoveFromCharacter();
		pkItemToDrop = item;
	}
	else
	{
		if (bCount == 0)
		{
			if (test_server)
				sys_log(0, "[DROP_ITEM] drop item count == 0");
			return false;
		}

		item->SetCount(item->GetCount() - bCount);
		ITEM_MANAGER::instance().FlushDelayedSave(item);

		pkItemToDrop = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), bCount);

		// copy item socket -- by mhh
		FN_copy_item_socket(pkItemToDrop, item);

		char szBuf[51 + 1];
		snprintf(szBuf, sizeof(szBuf), "%u %u", pkItemToDrop->GetID(), pkItemToDrop->GetCount());
		LogManager::instance().ItemLog(this, item, "ITEM_SPLIT", szBuf);
	}

	PIXEL_POSITION pxPos = GetXYZ();

	if (pkItemToDrop->AddToGround(GetMapIndex(), pxPos))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("떨어진 아이템은 3분 후 사라집니다."));
#ifdef ENABLE_NEWSTUFF
		pkItemToDrop->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPITEM]);
#else
		pkItemToDrop->StartDestroyEvent();
#endif

		ITEM_MANAGER::instance().FlushDelayedSave(pkItemToDrop);
		
		char szHint[32 + 1];
		snprintf(szHint, sizeof(szHint), "%s %u %u", pkItemToDrop->GetName(), pkItemToDrop->GetCount(), pkItemToDrop->GetOriginalVnum());
		LogManager::instance().ItemLog(this, pkItemToDrop, "DROP", szHint);
		//Motion(MOTION_PICKUP);
	}

	return true;
#else
	ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEM_DROP_BLOCK"));
	return false;
#endif
}

#ifdef WJ_NEW_DROP_DIALOG
bool CHARACTER::DestroyItem(TItemPos Cell)
{
	LPITEM item = NULL;
	
	if (!CanHandleItem())
	{
		if (NULL != DragonSoul_RefineWindow_GetOpener())
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("강화창을 연 상태에서는 아이템을 옮길 수 없습니다."));
		return false;
	}
	
	if (IsDead())
		return false;
	
	if (!IsValidItemPosition(Cell) || !(item = GetItem(Cell)))
		return false;
	
	if (item->IsExchanging())
		return false;
	
	if (true == item->isLocked())
		return false;
	
#ifdef WJ_SOULBINDING_SYSTEM
	if (item->IsBind() || item->IsUntilBind())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("BIND_ITEM_NOT_DELETE"));
		return false;
	}
#endif

	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
		return false;
	
	if (item->GetCount() <= 0)
		return false;
	
#ifdef WJ_GROWTH_PET_SYSTEM
	if (item->GetVnum() == 55701 || item->GetVnum() == 55702 || item->GetVnum() == 55703 || item->GetVnum() == 55704)
		if (GetNewPetSystem()->IsActivePet())
			return false;
#endif

	SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 1000);
	ITEM_MANAGER::instance().RemoveItem(item);
	ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEM_DESTROY_SUCCES"), item->GetName());
	
	return true;
}

bool CHARACTER::SellItem(TItemPos Cell, BYTE bCount)
{
	LPITEM item = NULL;
	
	if (!CanHandleItem())
	{
		if (NULL != DragonSoul_RefineWindow_GetOpener())
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("강화창을 연 상태에서는 아이템을 옮길 수 없습니다."));
		return false;
	}
	
	if (IsDead())
		return false;
	
	if (!IsValidItemPosition(Cell) || !(item = GetItem(Cell)))
		return false;
	
	if (item->IsExchanging())
		return false;
	
	if (true == item->isLocked())
		return false;
	
#ifdef WJ_SOULBINDING_SYSTEM
	if (item->IsBind() || item->IsUntilBind())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("BIND_ITEM_NOT_SELL"));
		return false;
	}
#endif

	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SELL))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("THIS_ITEM_NOT_SELL"));
		return false;
	}
	
	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
		return false;
	
	if (item->GetCount() <= 0)
		return false;
	
#ifdef WJ_GROWTH_PET_SYSTEM
	if (item->GetVnum() == 55701 || item->GetVnum() == 55702 || item->GetVnum() == 55703 || item->GetVnum() == 55704)
		if (GetNewPetSystem()->IsActivePet())
			return false;
#endif

	DWORD dwPrice;
	
	if (bCount == 0 || bCount > item->GetCount())
		bCount = item->GetCount();
	
	dwPrice = item->GetShopBuyPrice();
	
	if (IS_SET(item->GetFlag(), ITEM_FLAG_COUNT_PER_1GOLD))
	{
		if (dwPrice == 0)
			dwPrice = bCount;
		else
			dwPrice = bCount / dwPrice;
	}
	else
		dwPrice *= bCount;
	
	dwPrice /= 5;
	
	DWORD dwTax = 0;
#ifdef ENABLE_NEWSTUFF
	if (g_iTaxes != 0)
		dwTax = dwPrice * g_iTaxes/100;
#endif

	dwPrice -= dwTax;

	const int64_t nTotalMoney = static_cast<int64_t>(GetGold()) + static_cast<int64_t>(dwPrice);
	if (GOLD_MAX <= nTotalMoney)
	{
		sys_err("[OVERFLOW_GOLD] id %u name %s gold %u", GetPlayerID(), GetName(), GetGold());
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("20억냥이 초과하여 물품을 팔수 없습니다."));
		return false;
	}
	
	sys_log(0, "SHOP: SELL: %s item name: %s(x%d):%u price: %u", GetName(), item->GetName(), bCount, item->GetID(), dwPrice);
	
#ifdef ENABLE_NEWSTUFF
	if (g_iTaxes > 0)
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("판매금액의 %d %% 가 세금으로 나가게됩니다"), g_iTaxes);
#endif
	
	DBManager::instance().SendMoneyLog(MONEY_LOG_SHOP, item->GetVnum(), dwPrice);
	item->SetCount(item->GetCount() - bCount);
	PointChange(POINT_GOLD, dwPrice, false);
	ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEM_SELL_SUCCES"), item->GetName());
	
	return true;
}
#endif

bool CHARACTER::DropGold(int gold)
{
#ifdef WJ_SECURITY_SYSTEM
	if (IsActivateSecurity() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot drop gold with security key activate"));
		return false;
	}
#endif
	
#ifdef ENABLE_DROP_GOLD
	if (gold <= 0 || gold > GetGold())
		return false;

	if (!CanHandleItem())
		return false;

	#ifdef ENABLE_NEWSTUFF
	if (0 != g_GoldDropTimeLimitValue)
	{
		if (get_dword_time() < m_dwLastGoldDropTime+g_GoldDropTimeLimitValue)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("EVENT_FLAG_WAIT"));
			return false;
		}
	}
	#endif

	m_dwLastGoldDropTime = get_dword_time();

	LPITEM item = ITEM_MANAGER::instance().CreateItem(1, gold);

	if (item)
	{
		PIXEL_POSITION pos = GetXYZ();

		if (item->AddToGround(GetMapIndex(), pos))
		{
			//Motion(MOTION_PICKUP);
			PointChange(POINT_GOLD, -gold, true);
			
			if (gold > 1000) // 천원 이상만 기록한다.
				LogManager::instance().CharLog(this, gold, "DROP_GOLD", "");

			#ifdef ENABLE_NEWSTUFF
			item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPGOLD]);
			#else
			item->StartDestroyEvent();
			#endif
			// ChatPacket(CHAT_TYPE_INFO, LC_TEXT("떨어진 아이템은 %d분 후 사라집니다."), 150/60);
		}

		Save();
		return true;
	}
#endif
	ChatPacket(CHAT_TYPE_INFO, LC_TEXT("MONEY_DROP_BLOCK"));
	return false;
}

bool CHARACTER::MoveItem(TItemPos Cell, TItemPos DestCell, BYTE count)
{
	LPITEM item = NULL;
	
#ifdef WJ_SECURITY_SYSTEM
	if (IsActivateSecurity() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot move item with security key activate"));
		return false;
	}
#endif

	if (!IsValidItemPosition(Cell))
		return false;

	if (!(item = GetItem(Cell)))
		return false;
	
	if (item->IsExchanging())
		return false;

	if (item->GetCount() < count)
		return false;

	if (INVENTORY == Cell.window_type && Cell.cell >= INVENTORY_MAX_NUM && IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		return false;

	if (true == item->isLocked())
		return false;

	if (!IsValidItemPosition(DestCell))
	{
		return false;
	}

	if (!CanHandleItem())
	{
		if (NULL != DragonSoul_RefineWindow_GetOpener())
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("강화창을 연 상태에서는 아이템을 옮길 수 없습니다."));
		return false;
	}

	// 기획자의 요청으로 벨트 인벤토리에는 특정 타입의 아이템만 넣을 수 있다.
#ifdef ENABLE_NEWSTUFF
	if (DestCell.IsBeltInventoryPosition() && false == CBeltInventoryHelper::CanMoveIntoBeltInventory(item) && !g_bBeltAllowItems)
#else
	if (DestCell.IsBeltInventoryPosition() && false == CBeltInventoryHelper::CanMoveIntoBeltInventory(item))
#endif
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템은 벨트 인벤토리로 옮길 수 없습니다."));			
		return false;
	}
	
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	if (Cell.IsSkillBookInventoryPosition() && !DestCell.IsSkillBookInventoryPosition())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot move item this window."));
		return false;
	}
	
	if (Cell.IsUpgradeItemsInventoryPosition() && !DestCell.IsUpgradeItemsInventoryPosition())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot move item this window."));
		return false;
	}
#endif

	// 이미 착용중인 아이템을 다른 곳으로 옮기는 경우, '장책 해제' 가능한 지 확인하고 옮김
	if (Cell.IsEquipPosition() && !CanUnequipNow(item))
		return false;

	if (DestCell.IsEquipPosition())
	{
		if (GetItem(DestCell))	// 장비일 경우 한 곳만 검사해도 된다.
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 장비를 착용하고 있습니다."));
			
			return false;
		}

		EquipItem(item, DestCell.cell - INVENTORY_MAX_NUM);
	}
#ifdef WJ_INVENTORY_EX_SYSTEM
	else if (DestCell.cell > GetInventoryEx() && DestCell.IsDefaultInventoryPosition())
	{
		if (item->GetVnum() == 72319)
		{
			PointChange(POINT_INVENTORY_EX, 5, true);
			item->SetCount(item->GetCount() - 1);
		}
		else if (item->GetVnum() == 72320)
		{
			if (GetInventoryEx() == 175)
				PointChange(POINT_INVENTORY_EX, 5, true);
			else
				PointChange(POINT_INVENTORY_EX, 10, true);
			
			item->SetCount(item->GetCount() - 1);	
		}
		else
			return false;
	}
#endif
	else
	{
		if (item->IsDragonSoul())
		{
			if (item->IsEquipped())
			{
				return DSManager::instance().PullOut(this, DestCell, item);
			}
			else
			{
				if (DestCell.window_type != DRAGON_SOUL_INVENTORY)
				{
					return false;
				}

				if (!DSManager::instance().IsValidCellForThisItem(item, DestCell))
					return false;
			}
		}
		// 용혼석이 아닌 아이템은 용혼석 인벤에 들어갈 수 없다.
		else if (DRAGON_SOUL_INVENTORY == DestCell.window_type)
			return false;

		LPITEM item2;

		if ((item2 = GetItem(DestCell)) && item != item2 && item2->IsStackable() &&
				!IS_SET(item2->GetAntiFlag(), ITEM_ANTIFLAG_STACK) &&
				item2->GetVnum() == item->GetVnum()) // 합칠 수 있는 아이템의 경우
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				if (item2->GetSocket(i) != item->GetSocket(i))
					return false;

			if (count == 0)
				count = item->GetCount();

			sys_log(0, "%s: ITEM_STACK %s (window: %d, cell : %d) -> (window:%d, cell %d) count %d", GetName(), item->GetName(), Cell.window_type, Cell.cell, 
				DestCell.window_type, DestCell.cell, count);

			count = MIN(g_bItemCountLimit - item2->GetCount(), count);

			item->SetCount(item->GetCount() - count);
			item2->SetCount(item2->GetCount() + count);
			return true;
		}

		if (!IsEmptyItemGrid(DestCell, item->GetSize(), Cell.cell))
			return false;

		if (count == 0 || count >= item->GetCount() || !item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
		{
			sys_log(0, "%s: ITEM_MOVE %s (window: %d, cell : %d) -> (window:%d, cell %d) count %d", GetName(), item->GetName(), Cell.window_type, Cell.cell, 
				DestCell.window_type, DestCell.cell, count);
			
			item->RemoveFromCharacter();
			SetItem(DestCell, item);

			if (INVENTORY == Cell.window_type && INVENTORY == DestCell.window_type)
				SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, DestCell.cell);
		}
		else if (count < item->GetCount())
		{
			sys_log(0, "%s: ITEM_SPLIT %s (window: %d, cell : %d) -> (window:%d, cell %d) count %d", GetName(), item->GetName(), Cell.window_type, Cell.cell, 
				DestCell.window_type, DestCell.cell, count);

			item->SetCount(item->GetCount() - count);
			LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), count);

			// copy socket -- by mhh
			FN_copy_item_socket(item2, item);

			item2->AddToCharacter(this, DestCell);

			char szBuf[51+1];
			snprintf(szBuf, sizeof(szBuf), "%u %u %u %u ", item2->GetID(), item2->GetCount(), item->GetCount(), item->GetCount() + item2->GetCount());
			LogManager::instance().ItemLog(this, item, "ITEM_SPLIT", szBuf);
		}
	}

	return true;
}

namespace NPartyPickupDistribute
{
	struct FFindOwnership
	{
		LPITEM item;
		LPCHARACTER owner;

		FFindOwnership(LPITEM item) 
			: item(item), owner(NULL)
		{
		}

		void operator () (LPCHARACTER ch)
		{
			if (item->IsOwnership(ch))
				owner = ch;
		}
	};

	struct FCountNearMember
	{
		int		total;
		int		x, y;

		FCountNearMember(LPCHARACTER center )
			: total(0), x(center->GetX()), y(center->GetY())
		{
		}

		void operator () (LPCHARACTER ch)
		{
			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
				total += 1;
		}
	};

	struct FMoneyDistributor
	{
		int		total;
		LPCHARACTER	c;
		int		x, y;
		int		iMoney;

		FMoneyDistributor(LPCHARACTER center, int iMoney)
			: total(0), c(center), x(center->GetX()), y(center->GetY()), iMoney(iMoney)
		{
		}

		void operator ()(LPCHARACTER ch)
		{
			if (ch!=c)
				if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
				{
					ch->PointChange(POINT_GOLD, iMoney, true);

					if (iMoney > 1000) // 천원 이상만 기록한다.
					{
						LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(ch, iMoney, "GET_GOLD", ""));
					}
				}
		}
	};
}

void CHARACTER::GiveGold(int iAmount)
{
	if (iAmount <= 0)
		return;

	sys_log(0, "GIVE_GOLD: %s %d", GetName(), iAmount);

	if (GetParty())
	{
		LPPARTY pParty = GetParty();

		// 파티가 있는 경우 나누어 가진다.
		DWORD dwTotal = iAmount;
		DWORD dwMyAmount = dwTotal;

		NPartyPickupDistribute::FCountNearMember funcCountNearMember(this);
		pParty->ForEachOnlineMember(funcCountNearMember);

		if (funcCountNearMember.total > 1)
		{
			DWORD dwShare = dwTotal / funcCountNearMember.total;
			dwMyAmount -= dwShare * (funcCountNearMember.total - 1);

			NPartyPickupDistribute::FMoneyDistributor funcMoneyDist(this, dwShare);

			pParty->ForEachOnlineMember(funcMoneyDist);
		}

		PointChange(POINT_GOLD, dwMyAmount, true);

		if (dwMyAmount > 1000) // 천원 이상만 기록한다.
		{
			LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(this, dwMyAmount, "GET_GOLD", ""));
		}
	}
	else
	{
		PointChange(POINT_GOLD, iAmount, true);

		if (iAmount > 1000) // 천원 이상만 기록한다.
		{
			LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(this, iAmount, "GET_GOLD", ""));
		}
	}
}

#ifdef WJ_CHEQUE_SYSTEM
void CHARACTER::GiveCheque(int iAmount)
{
	if (iAmount <= 0)
		return;

	sys_log(0, "GIVE_CHEQUE: %s %d", GetName(), iAmount);

	if (GetParty())
	{
		LPPARTY pParty = GetParty();

		// 파티가 있는 경우 나누어 가진다.
		DWORD dwTotal = iAmount;
		DWORD dwMyAmount = dwTotal;

		NPartyPickupDistribute::FCountNearMember funcCountNearMember(this);
		pParty->ForEachOnlineMember(funcCountNearMember);

		if (funcCountNearMember.total > 1)
		{
			DWORD dwShare = dwTotal / funcCountNearMember.total;
			dwMyAmount -= dwShare * (funcCountNearMember.total - 1);

			NPartyPickupDistribute::FMoneyDistributor funcMoneyDist(this, dwShare);

			pParty->ForEachOnlineMember(funcMoneyDist);
		}

		PointChange(POINT_CHEQUE, dwMyAmount, true);

		if (dwMyAmount > 1000) // 천원 이상만 기록한다.
		{
			LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(this, dwMyAmount, "GET_CHEQUE", ""));
		}
	}
	else
	{
		PointChange(POINT_CHEQUE, iAmount, true);

		if (iAmount > 1000) // 천원 이상만 기록한다.
		{
			LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(this, iAmount, "GET_CHEQUE", ""));
		}
	}	
}
#endif

bool CHARACTER::PickupItem(DWORD dwVID)
{
#ifdef WJ_SECURITY_SYSTEM
	if (IsActivateSecurity() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot pickup item with security key activate"));
		return false;
	}
#endif
	
	LPITEM item = ITEM_MANAGER::instance().FindByVID(dwVID);

	if (IsObserverMode())
		return false;

	if (!item || !item->GetSectree())
		return false;

	if (item->DistanceValid(this))
	{
		if (item->IsOwnership(this))
		{
			// 만약 주으려 하는 아이템이 엘크라면
			if (item->GetType() == ITEM_ELK)
			{
				GiveGold(item->GetCount());
				item->RemoveFromGround();

				M2_DESTROY_ITEM(item);

				Save();
			}
			// 평범한 아이템이라면
			else
			{
				if (item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
				{
					BYTE bCount = item->GetCount();

#ifdef WJ_INVENTORY_EX_SYSTEM
					for (int i = 0; i < GetInventoryEx(); ++i)
#else
					for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
					{
						LPITEM item2 = GetInventoryItem(i);

						if (!item2)
							continue;

						if (item2->GetVnum() == item->GetVnum())
						{
							int j;

							for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
								if (item2->GetSocket(j) != item->GetSocket(j))
									break;

							if (j != ITEM_SOCKET_MAX_NUM)
								continue;

							BYTE bCount2 = MIN(g_bItemCountLimit - item2->GetCount(), bCount);
							bCount -= bCount2;

							item2->SetCount(item2->GetCount() + bCount2);

							if (bCount == 0)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 획득: %s"), item2->GetName());
								M2_DESTROY_ITEM(item);
								if (item2->GetType() == ITEM_QUEST)
									quest::CQuestManager::instance().PickupItem (GetPlayerID(), item2);
								return true;
							}
						}
					}

					item->SetCount(bCount);
				}

				int iEmptyCell;
				if (item->IsDragonSoul())
				{
					if ((iEmptyCell = GetEmptyDragonSoulInventory(item)) == -1)
					{
						sys_log(0, "No empty ds inventory pid %u size %ud itemid %u", GetPlayerID(), item->GetSize(), item->GetID());
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
				else if (item->IsSkillBook())
				{
					if ((iEmptyCell = GetEmptySkillBookInventory(item->GetSize())) == -1)
					{
						sys_log(0, "No empty ds inventory pid %u size %ud itemid %u", GetPlayerID(), item->GetSize(), item->GetID());
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}
				else if (item->IsUpgradeItem())
				{
					if ((iEmptyCell = GetEmptyUpgradeItemsInventory(item->GetSize())) == -1)
					{
						sys_log(0, "No empty ds inventory pid %u size %ud itemid %u", GetPlayerID(), item->GetSize(), item->GetID());
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}					
				}
#endif
				else
				{
					if ((iEmptyCell = GetEmptyInventory(item->GetSize())) == -1)
					{
						sys_log(0, "No empty inventory pid %u size %ud itemid %u", GetPlayerID(), item->GetSize(), item->GetID());
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}

				item->RemoveFromGround();
				
				if (item->IsDragonSoul())
					item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyCell));
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
				else if (item->IsSkillBook())
					item->AddToCharacter(this, TItemPos(INVENTORY, iEmptyCell));
				else if (item->IsUpgradeItem())
					item->AddToCharacter(this, TItemPos(INVENTORY, iEmptyCell));
#endif
				else
					item->AddToCharacter(this, TItemPos(INVENTORY, iEmptyCell));

				char szHint[32+1];
				snprintf(szHint, sizeof(szHint), "%s %u %u", item->GetName(), item->GetCount(), item->GetOriginalVnum());
				LogManager::instance().ItemLog(this, item, "GET", szHint);
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 획득: %s"), item->GetName());

				if (item->GetType() == ITEM_QUEST)
					quest::CQuestManager::instance().PickupItem (GetPlayerID(), item);
			}

			//Motion(MOTION_PICKUP);
			return true;
		}
		else if (!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP) && GetParty())
		{
			// 다른 파티원 소유권 아이템을 주으려고 한다면
			NPartyPickupDistribute::FFindOwnership funcFindOwnership(item);

			GetParty()->ForEachOnlineMember(funcFindOwnership);

			LPCHARACTER owner = funcFindOwnership.owner;
			
			// @fixme17
			if (!owner)
				return false;

			int iEmptyCell;

			if (item->IsDragonSoul())
			{
				if (!(owner && (iEmptyCell = owner->GetEmptyDragonSoulInventory(item)) != -1))
				{
					owner = this;

					if ((iEmptyCell = GetEmptyDragonSoulInventory(item)) == -1)
					{
						owner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}
			}
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
			else if (item->IsSkillBook())
			{
				if (!(owner && (iEmptyCell = owner->GetEmptySkillBookInventory(item->GetSize())) != -1))
				{
					owner = this;

					if ((iEmptyCell = GetEmptySkillBookInventory(item->GetSize())) == -1)
					{
						owner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}				
			}
			else if (item->IsUpgradeItem())
			{
				if (!(owner && (iEmptyCell = owner->GetEmptyUpgradeItemsInventory(item->GetSize())) != -1))
				{
					owner = this;

					if ((iEmptyCell = GetEmptyUpgradeItemsInventory(item->GetSize())) == -1)
					{
						owner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}					
			}
#endif
			else
			{
				if (!(owner && (iEmptyCell = owner->GetEmptyInventory(item->GetSize())) != -1))
				{
					owner = this;

					if ((iEmptyCell = GetEmptyInventory(item->GetSize())) == -1)
					{
						owner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}
			}

			item->RemoveFromGround();

			if (item->IsDragonSoul())
				item->AddToCharacter(owner, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyCell));
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
			else if (item->IsSkillBook())
				item->AddToCharacter(owner, TItemPos(INVENTORY, iEmptyCell));
			else if (item->IsUpgradeItem())
				item->AddToCharacter(owner, TItemPos(INVENTORY, iEmptyCell));
#endif
			else
				item->AddToCharacter(owner, TItemPos(INVENTORY, iEmptyCell));

			char szHint[32+1];
			snprintf(szHint, sizeof(szHint), "%s %u %u", item->GetName(), item->GetCount(), item->GetOriginalVnum());
			LogManager::instance().ItemLog(owner, item, "GET", szHint);

			if (owner == this)
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 획득: %s"), item->GetName());
			else
			{
				owner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 획득: %s 님으로부터 %s"), GetName(), item->GetName());
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 전달: %s 님에게 %s"), owner->GetName(), item->GetName());
			}

			if (item->GetType() == ITEM_QUEST)
				quest::CQuestManager::instance().PickupItem (owner->GetPlayerID(), item);

			return true;
		}
	}

	return false;
}

bool CHARACTER::SwapItem(UINT bCell, UINT bDestCell)
{
	if (!CanHandleItem())
		return false;

	TItemPos srcCell(INVENTORY, bCell), destCell(INVENTORY, bDestCell);

	// 올바른 Cell 인지 검사
	// 용혼석은 Swap할 수 없으므로, 여기서 걸림.
	//if (bCell >= INVENTORY_MAX_NUM + WEAR_MAX_NUM || bDestCell >= INVENTORY_MAX_NUM + WEAR_MAX_NUM)
	if (srcCell.IsDragonSoulEquipPosition() || destCell.IsDragonSoulEquipPosition())
		return false;

	// 같은 CELL 인지 검사
	if (bCell == bDestCell)
		return false;

	// 둘 다 장비창 위치면 Swap 할 수 없다.
	if (srcCell.IsEquipPosition() && destCell.IsEquipPosition())
		return false;

	LPITEM item1, item2;

	// item2가 장비창에 있는 것이 되도록.
	if (srcCell.IsEquipPosition())
	{
		item1 = GetInventoryItem(bDestCell);
		item2 = GetInventoryItem(bCell);
	}
	else
	{
		item1 = GetInventoryItem(bCell);
		item2 = GetInventoryItem(bDestCell);
	}

	if (!item1 || !item2)
		return false;
	
	if (item1 == item2)
	{
	    sys_log(0, "[WARNING][WARNING][HACK USER!] : %s %d %d", m_stName.c_str(), bCell, bDestCell);
	    return false;
	}

	// item2가 bCell위치에 들어갈 수 있는지 확인한다.
	if (!IsEmptyItemGrid(TItemPos (INVENTORY, item1->GetCell()), item2->GetSize(), item1->GetCell()))
		return false;

	// 바꿀 아이템이 장비창에 있으면
	if (TItemPos(EQUIPMENT, item2->GetCell()).IsEquipPosition())
	{
		INT bEquipCell = item2->GetCell() - INVENTORY_MAX_NUM;
		UINT bInvenCell = item1->GetCell();

		// 착용중인 아이템을 벗을 수 있고, 착용 예정 아이템이 착용 가능한 상태여야만 진행
		if (item2->IsDragonSoul()) // @fixme18
		{
			if (false == CanUnequipNow(item2) || false == CanEquipNow(item1))
				return false;
		}

		if (bEquipCell != item1->FindEquipCell(this)) // 같은 위치일때만 허용
			return false;

		item2->RemoveFromCharacter();

		if (item1->EquipTo(this, bEquipCell))
#ifdef WJ_HIGHLIGHT_SYSTEM
			item2->AddToCharacter(this, TItemPos(INVENTORY, bInvenCell), false);
#else
			item2->AddToCharacter(this, TItemPos(INVENTORY, bInvenCell));
#endif
		else
			sys_err("SwapItem cannot equip %s! item1 %s", item2->GetName(), item1->GetName());
	}
	else
	{
		UINT bCell1 = item1->GetCell();
		UINT bCell2 = item2->GetCell();
		
		item1->RemoveFromCharacter();
		item2->RemoveFromCharacter();

#ifdef WJ_HIGHLIGHT_SYSTEM
		item1->AddToCharacter(this, TItemPos(INVENTORY, bCell2), false);
		item2->AddToCharacter(this, TItemPos(INVENTORY, bCell1), false);
#else
		item1->AddToCharacter(this, TItemPos(INVENTORY, bCell2));
		item2->AddToCharacter(this, TItemPos(INVENTORY, bCell1));
#endif
	}

	return true;
}

bool CHARACTER::UnequipItem(LPITEM item)
{
#ifdef WJ_SECURITY_SYSTEM
	if (IsActivateSecurity() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot unequip item with security key activate"));
		return false;
	}	
#endif
	
	int pos;

	if (false == CanUnequipNow(item))
		return false;

	if (item->IsDragonSoul())
		pos = GetEmptyDragonSoulInventory(item);
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	else if (item->IsSkillBook())
		pos = GetEmptySkillBookInventory(item->GetSize());
	else if (item->IsUpgradeItem())
		pos = GetEmptyUpgradeItemsInventory(item->GetSize());
#endif
	else
		pos = GetEmptyInventory(item->GetSize());

	// HARD CODING
	if (item->GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		ShowAlignment(true);

	item->RemoveFromCharacter();
	if (item->IsDragonSoul())
#ifdef WJ_HIGHLIGHT_SYSTEM
		item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, pos), false);
#else
		item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, pos));
#endif
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	else if (item->IsSkillBook())
#ifdef WJ_HIGHLIGHT_SYSTEM
		item->AddToCharacter(this, TItemPos(INVENTORY, pos), false);
#else
		item->AddToCharacter(this, TItemPos(INVENTORY, pos));
#endif
	else if (item->IsUpgradeItem())
#ifdef WJ_HIGHLIGHT_SYSTEM
		item->AddToCharacter(this, TItemPos(INVENTORY, pos), false);
#else
		item->AddToCharacter(this, TItemPos(INVENTORY, pos));
#endif
#endif
	else
#ifdef WJ_HIGHLIGHT_SYSTEM
		item->AddToCharacter(this, TItemPos(INVENTORY, pos), false);
#else
		item->AddToCharacter(this, TItemPos(INVENTORY, pos));
#endif

	CheckMaximumPoints();

	return true;
}

//
// @version	05/07/05 Bang2ni - Skill 사용후 1.5 초 이내에 장비 착용 금지
//
bool CHARACTER::EquipItem(LPITEM item, int iCandidateCell)
{
#ifdef WJ_SECURITY_SYSTEM
	if (IsActivateSecurity() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot equip item with security key activate"));
		return false;
	}	
#endif
	
	if (item->IsExchanging())
		return false;

	if (false == item->IsEquipable())
		return false;

	if (false == CanEquipNow(item))
		return false;
	
	// @fixme48
	if (ITEM_BELT == item->GetType() && CBeltInventoryHelper::IsExistItemInBeltInventory(this))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("KEMER_ENVANTERINI_BOSALT"));
		return false;
	}

	int iWearCell = item->FindEquipCell(this, iCandidateCell);

	if (iWearCell < 0)
		return false;
	
	// 무언가를 탄 상태에서 턱시도 입기 금지
	if (iWearCell == WEAR_BODY && IsRiding() && (item->GetVnum() >= 11901 && item->GetVnum() <= 11904))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("말을 탄 상태에서 예복을 입을 수 없습니다."));
		return false;
	}

	if (iWearCell != WEAR_ARROW && IsPolymorphed())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("둔갑 중에는 착용중인 장비를 변경할 수 없습니다."));
		return false;
	}

	if (FN_check_item_sex(this, item) == false)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("성별이 맞지않아 이 아이템을 사용할 수 없습니다."));
		return false;
	}

	//신규 탈것 사용시 기존 말 사용여부 체크
	if(item->IsRideItem() && IsRiding())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 탈것을 이용중입니다."));
		return false;
	}

	// 화살 이외에는 마지막 공격 시간 또는 스킬 사용 1.5 후에 장비 교체가 가능
	DWORD dwCurTime = get_dword_time();

	if (iWearCell != WEAR_ARROW 
		&& (dwCurTime - GetLastAttackTime() <= 1500 || dwCurTime - m_dwLastSkillTime <= 1500))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("가만히 있을 때만 착용할 수 있습니다."));
		return false;
	}

	// 용혼석 특수 처리
	if (item->IsDragonSoul())
	{
		// 같은 타입의 용혼석이 이미 들어가 있다면 착용할 수 없다.
		// 용혼석은 swap을 지원하면 안됨.
		if(GetInventoryItem(INVENTORY_MAX_NUM + iWearCell))
		{
			ChatPacket(CHAT_TYPE_INFO, "이미 같은 종류의 용혼석을 착용하고 있습니다.");
			return false;
		}
		
		if (!item->EquipTo(this, iWearCell))
		{
			return false;
		}
	}
	// 용혼석이 아님.
	else
	{
		// 착용할 곳에 아이템이 있다면,
		if (GetWear(iWearCell) && !IS_SET(GetWear(iWearCell)->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		{
			// 이 아이템은 한번 박히면 변경 불가. swap 역시 완전 불가
			if (item->GetWearFlag() == WEARABLE_ABILITY) 
				return false;

			if (false == SwapItem(item->GetCell(), INVENTORY_MAX_NUM + iWearCell))
			{
				return false;
			}
		}
		else
		{
			UINT bOldCell = item->GetCell();

			if (item->EquipTo(this, iWearCell))
			{
				SyncQuickslot(QUICKSLOT_TYPE_ITEM, bOldCell, iWearCell);
			}
		}
	}

	if (true == item->IsEquipped())
	{
		// 아이템 최초 사용 이후부터는 사용하지 않아도 시간이 차감되는 방식 처리. 
		if (-1 != item->GetProto()->cLimitRealTimeFirstUseIndex)
		{
			// 한 번이라도 사용한 아이템인지 여부는 Socket1을 보고 판단한다. (Socket1에 사용횟수 기록)
			if (0 == item->GetSocket(1))
			{
				// 사용가능시간은 Default 값으로 Limit Value 값을 사용하되, Socket0에 값이 있으면 그 값을 사용하도록 한다. (단위는 초)
				long duration = (0 != item->GetSocket(0)) ? item->GetSocket(0) : item->GetProto()->aLimits[item->GetProto()->cLimitRealTimeFirstUseIndex].lValue;

				if (0 == duration)
					duration = 60 * 60 * 24 * 7;

				item->SetSocket(0, time(0) + duration);
				item->StartRealTimeExpireEvent();
			}

			item->SetSocket(1, item->GetSocket(1) + 1);
		}

		if (item->GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
			ShowAlignment(false);

		const DWORD& dwVnum = item->GetVnum();

		// 라마단 이벤트 초승달의 반지(71135) 착용시 이펙트 발동
		if (true == CItemVnumHelper::IsRamadanMoonRing(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_RAMADAN_RING);
		}
		// 할로윈 사탕(71136) 착용시 이펙트 발동
		else if (true == CItemVnumHelper::IsHalloweenCandy(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_HALLOWEEN_CANDY);
		}
		// 행복의 반지(71143) 착용시 이펙트 발동
		else if (true == CItemVnumHelper::IsHappinessRing(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_HAPPINESS_RING);
		}
		// 사랑의 팬던트(71145) 착용시 이펙트 발동
		else if (true == CItemVnumHelper::IsLovePendant(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_LOVE_PENDANT);
		}
		else if (true == CItemVnumHelper::IsDeadlyPowerRing(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_DEADLY_POWER_RING);
		}
		else if (true == CItemVnumHelper::IsWillPowerRing(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_WILL_POWER_RING);
		}
		else if (true == CItemVnumHelper::IsMedalOfHeroism(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_MEDAL_OF_HEROISM);
		}
		else if (true == CItemVnumHelper::IsMagicCandy(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_MAGIC_CANDY);
		}
		else if (true == CItemVnumHelper::IsChocolateCharm(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_CHOCOLATE_CHARM);
		}
		else if (true == CItemVnumHelper::IsEvilEyeCharm(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_EVIL_EYE_CHARM);
		}
		// ITEM_UNIQUE의 경우, SpecialItemGroup에 정의되어 있고, (item->GetSIGVnum() != NULL)
		// 
		else if (ITEM_UNIQUE == item->GetType() && 0 != item->GetSIGVnum())
		{
			const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(item->GetSIGVnum());
			if (NULL != pGroup)
			{
				const CSpecialAttrGroup* pAttrGroup = ITEM_MANAGER::instance().GetSpecialAttrGroup(pGroup->GetAttrVnum(item->GetVnum()));
				if (NULL != pAttrGroup)
				{
					const std::string& std = pAttrGroup->m_stEffectFileName;
					SpecificEffectPacket(std.c_str());
				}
			}
		}
#ifdef WJ_ACCE_SYSTEM
		else if (true == CItemVnumHelper::IsAcceItem(item->GetType(), item->GetSubType()))
		{
			if (item->GetSocket(0) > 18)
				this->EffectPacket(SE_ACCE_BACK);

			this->EffectPacket(SE_ACCE_EQUIP);
		}
#endif

		if (UNIQUE_SPECIAL_RIDE == item->GetSubType() && IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_USE))
		{
			quest::CQuestManager::instance().UseItem(GetPlayerID(), item, false);
		}
	}

	return true;
}

void CHARACTER::BuffOnAttr_AddBuffsFromItem(LPITEM pItem)
{
	// @fixme int -> size_t
	for (size_t i = 0; i < sizeof(g_aBuffOnAttrPoints)/sizeof(g_aBuffOnAttrPoints[0]); i++)
	{
		TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.find(g_aBuffOnAttrPoints[i]);
		if (it != m_map_buff_on_attrs.end())
		{
			it->second->AddBuffFromItem(pItem);
		}
	}
}

void CHARACTER::BuffOnAttr_RemoveBuffsFromItem(LPITEM pItem)
{
	// @fixme int -> size_t
	for (size_t i = 0; i < sizeof(g_aBuffOnAttrPoints)/sizeof(g_aBuffOnAttrPoints[0]); i++)
	{
		TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.find(g_aBuffOnAttrPoints[i]);
		if (it != m_map_buff_on_attrs.end())
		{
			it->second->RemoveBuffFromItem(pItem);
		}
	}
}

void CHARACTER::BuffOnAttr_ClearAll()
{
	for (TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.begin(); it != m_map_buff_on_attrs.end(); it++)
	{
		CBuffOnAttributes* pBuff = it->second;
		if (pBuff)
		{
			pBuff->Initialize();
		}
	}
}

void CHARACTER::BuffOnAttr_ValueChange(BYTE bType, BYTE bOldValue, BYTE bNewValue)
{
	TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.find(bType);

	if (0 == bNewValue)
	{
		if (m_map_buff_on_attrs.end() == it)
			return;
		else
			it->second->Off();
	}
	else if(0 == bOldValue)
	{
		CBuffOnAttributes* pBuff = NULL;
		if (m_map_buff_on_attrs.end() == it)
		{
			switch (bType)
			{
			case POINT_ENERGY:
				{
					static BYTE abSlot[] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_SHIELD };
					static std::vector <BYTE> vec_slots (abSlot, abSlot + _countof(abSlot));
					pBuff = M2_NEW CBuffOnAttributes(this, bType, &vec_slots);
				}
				break;
			case POINT_COSTUME_ATTR_BONUS:
				{
					static BYTE abSlot[] = {WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR,
#ifdef WJ_WEAPON_COSTUME_SYSTEM
											WEAR_COSTUME_WEAPON,
#endif
					};
					static std::vector <BYTE> vec_slots (abSlot, abSlot + _countof(abSlot));
					pBuff = M2_NEW CBuffOnAttributes(this, bType, &vec_slots);
				}
				break;
			default:
				break;
			}
			m_map_buff_on_attrs.insert(TMapBuffOnAttrs::value_type(bType, pBuff));

		}
		else
			pBuff = it->second;
		if (pBuff != NULL)
			pBuff->On(bNewValue);
	}
	else
	{
		assert (m_map_buff_on_attrs.end() != it);
		it->second->ChangeBuffValue(bNewValue);
	}
}


LPITEM CHARACTER::FindSpecifyItem(DWORD vnum) const
{
#ifdef WJ_INVENTORY_EX_SYSTEM
	for (int i = 0; i < GetInventoryEx(); ++i)
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		if (GetInventoryItem(i) && GetInventoryItem(i)->GetVnum() == vnum)
			return GetInventoryItem(i);

	return NULL;
}

LPITEM CHARACTER::FindItemByID(DWORD id) const
{
#ifdef WJ_INVENTORY_EX_SYSTEM
	for (int i=0 ; i < GetInventoryEx() ; ++i)
#else
	for (int i=0 ; i < INVENTORY_MAX_NUM ; ++i)
#endif
	{
		if (NULL != GetInventoryItem(i) && GetInventoryItem(i)->GetID() == id)
			return GetInventoryItem(i);
	}

	for (int i=BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END ; ++i)
	{
		if (NULL != GetInventoryItem(i) && GetInventoryItem(i)->GetID() == id)
			return GetInventoryItem(i);
	}

	return NULL;
}

int CHARACTER::CountSpecifyItem(DWORD vnum) const
{
	int	count = 0;
	LPITEM item;

#ifdef WJ_INVENTORY_EX_SYSTEM
	for (int i = 0; i < GetInventoryEx(); ++i)
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
	{
		item = GetInventoryItem(i);
		if (NULL != item && item->GetVnum() == vnum)
		{
			// 개인 상점에 등록된 물건이면 넘어간다.
			if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
			{
				continue;
			}
			else
			{
				count += item->GetCount();
			}
		}
	}
	
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	for (int i = SKILL_BOOK_INVENTORY_SLOT_START; i < SKILL_BOOK_INVENTORY_SLOT_END; ++i)
	{
		item = GetInventoryItem(i);
		if (NULL != item && item->GetVnum() == vnum)
		{
			// 개인 상점에 등록된 물건이면 넘어간다.
			if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
			{
				continue;
			}
			else
			{
				count += item->GetCount();
			}
		}		
	}
	
	for (int i = UPGRADE_ITEMS_INVENTORY_SLOT_START; i < UPGRADE_ITEMS_INVENTORY_SLOT_END; ++i)
	{
		item = GetInventoryItem(i);
		if (NULL != item && item->GetVnum() == vnum)
		{
			// 개인 상점에 등록된 물건이면 넘어간다.
			if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
			{
				continue;
			}
			else
			{
				count += item->GetCount();
			}
		}			
	}
#endif

	return count;
}

#ifdef ENABLE_NEWSTUFF
int CHARACTER::CountSpecifyItem0(DWORD vnum) const
{
	int    count = 0;
	LPITEM item;

	for (int i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
	{
		item = GetFullInventoryItem(i);
		if (NULL != item && item->GetVnum() == vnum)
		{
			if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
				continue;
			else
				count += item->GetCount();
		}
	}

	return count;
}
#endif

void CHARACTER::RemoveSpecifyItem(DWORD vnum, DWORD count)
{
	if (0 == count)
		return;

#ifdef WJ_INVENTORY_EX_SYSTEM
	for (UINT i = 0; i < GetInventoryEx(); ++i)
#else
	for (UINT i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
	{
		if (NULL == GetInventoryItem(i))
			continue;

		if (GetInventoryItem(i)->GetVnum() != vnum)
			continue;

		//개인 상점에 등록된 물건이면 넘어간다. (개인 상점에서 판매될때 이 부분으로 들어올 경우 문제!)
		if(m_pkMyShop)
		{
			bool isItemSelling = m_pkMyShop->IsSellingItem(GetInventoryItem(i)->GetID());
			if (isItemSelling)
				continue;
		}

		if (vnum >= 80003 && vnum <= 80007)
			LogManager::instance().GoldBarLog(GetPlayerID(), GetInventoryItem(i)->GetID(), QUEST, "RemoveSpecifyItem");

		if (count >= GetInventoryItem(i)->GetCount())
		{
			count -= GetInventoryItem(i)->GetCount();
			GetInventoryItem(i)->SetCount(0);

			if (0 == count)
				return;
		}
		else
		{
			GetInventoryItem(i)->SetCount(GetInventoryItem(i)->GetCount() - count);
			return;
		}
	}
	
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	for (UINT i = SKILL_BOOK_INVENTORY_SLOT_START; i < UPGRADE_ITEMS_INVENTORY_SLOT_END; ++i)
	{
		if (NULL == GetInventoryItem(i))
			continue;

		if (GetInventoryItem(i)->GetVnum() != vnum)
			continue;

		//개인 상점에 등록된 물건이면 넘어간다. (개인 상점에서 판매될때 이 부분으로 들어올 경우 문제!)
		if(m_pkMyShop)
		{
			bool isItemSelling = m_pkMyShop->IsSellingItem(GetInventoryItem(i)->GetID());
			if (isItemSelling)
				continue;
		}

		if (vnum >= 80003 && vnum <= 80007)
			LogManager::instance().GoldBarLog(GetPlayerID(), GetInventoryItem(i)->GetID(), QUEST, "RemoveSpecifyItem");

		if (count >= GetInventoryItem(i)->GetCount())
		{
			count -= GetInventoryItem(i)->GetCount();
			GetInventoryItem(i)->SetCount(0);

			if (0 == count)
				return;
		}
		else
		{
			GetInventoryItem(i)->SetCount(GetInventoryItem(i)->GetCount() - count);
			return;
		}
	}
#endif	

	// 예외처리가 약하다.
	if (count)
		sys_log(0, "CHARACTER::RemoveSpecifyItem cannot remove enough item vnum %u, still remain %d", vnum, count);
}

#ifdef ENABLE_NEWSTUFF
void CHARACTER::RemoveSpecifyItem0(DWORD vnum, DWORD count)
{
	if (0 == count)
		return;

	for (UINT i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
	{
		if (NULL == GetFullInventoryItem(i))
			continue;

		if (GetFullInventoryItem(i)->GetVnum() != vnum)
			continue;

		if (count >= GetFullInventoryItem(i)->GetCount())
		{
			count -= GetFullInventoryItem(i)->GetCount();
			GetFullInventoryItem(i)->SetCount(0);

			if (0 == count)
				return;
		}
		else
		{
			GetFullInventoryItem(i)->SetCount(GetFullInventoryItem(i)->GetCount() - count);
			return;
		}
	}

	if (count)
		sys_log(0, "CHARACTER::RemoveSpecifyItem cannot remove enough item vnum %u, still remain %d", vnum, count);
}
#endif

int CHARACTER::CountSpecifyTypeItem(BYTE type) const
{
	int	count = 0;

#ifdef WJ_INVENTORY_EX_SYSTEM
	for (int i = 0; i < GetInventoryEx(); ++i)
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
	{
		LPITEM pItem = GetInventoryItem(i);
		if (pItem != NULL && pItem->GetType() == type)
		{
			count += pItem->GetCount();
		}
	}
	
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	for (int i = SKILL_BOOK_INVENTORY_SLOT_START; i < UPGRADE_ITEMS_INVENTORY_SLOT_END; ++i)
	{
		LPITEM pItem = GetInventoryItem(i);
		if (pItem != NULL && pItem->GetType() == type)
		{
			count += pItem->GetCount();
		}
	}
#endif

	return count;
}

void CHARACTER::RemoveSpecifyTypeItem(BYTE type, DWORD count)
{
	if (0 == count)
		return;

	for (UINT i = 0; i < INVENTORY_MAX_NUM; ++i)
	{
		if (NULL == GetInventoryItem(i))
			continue;

		if (GetInventoryItem(i)->GetType() != type)
			continue;

		//개인 상점에 등록된 물건이면 넘어간다. (개인 상점에서 판매될때 이 부분으로 들어올 경우 문제!)
		if(m_pkMyShop)
		{
			bool isItemSelling = m_pkMyShop->IsSellingItem(GetInventoryItem(i)->GetID());
			if (isItemSelling)
				continue;
		}

		if (count >= GetInventoryItem(i)->GetCount())
		{
			count -= GetInventoryItem(i)->GetCount();
			GetInventoryItem(i)->SetCount(0);

			if (0 == count)
				return;
		}
		else
		{
			GetInventoryItem(i)->SetCount(GetInventoryItem(i)->GetCount() - count);
			return;
		}
	}
	
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	for (UINT i = SKILL_BOOK_INVENTORY_SLOT_START; i < UPGRADE_ITEMS_INVENTORY_SLOT_END; ++i)
	{
		if (NULL == GetInventoryItem(i))
			continue;

		if (GetInventoryItem(i)->GetType() != type)
			continue;

		//개인 상점에 등록된 물건이면 넘어간다. (개인 상점에서 판매될때 이 부분으로 들어올 경우 문제!)
		if(m_pkMyShop)
		{
			bool isItemSelling = m_pkMyShop->IsSellingItem(GetInventoryItem(i)->GetID());
			if (isItemSelling)
				continue;
		}

		if (count >= GetInventoryItem(i)->GetCount())
		{
			count -= GetInventoryItem(i)->GetCount();
			GetInventoryItem(i)->SetCount(0);

			if (0 == count)
				return;
		}
		else
		{
			GetInventoryItem(i)->SetCount(GetInventoryItem(i)->GetCount() - count);
			return;
		}
	}
#endif
}

void CHARACTER::AutoGiveItem(LPITEM item, bool longOwnerShip)
{
	if (NULL == item)
	{
		sys_err ("NULL point.");
		return;
	}
	if (item->GetOwner())
	{
		sys_err ("item %d 's owner exists!",item->GetID());
		return;
	}
	
	int cell;
	if (item->IsDragonSoul())
	{
		cell = GetEmptyDragonSoulInventory(item);
	}
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	else if (item->IsSkillBook())
	{
		cell = GetEmptySkillBookInventory(item->GetSize());
	}
	else if (item->IsUpgradeItem())
	{
		cell = GetEmptyUpgradeItemsInventory(item->GetSize());
	}
#endif
	else
	{
		cell = GetEmptyInventory (item->GetSize());
	}

	if (cell != -1)
	{
		if (item->IsDragonSoul())
			item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, cell));
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
		else if (item->IsSkillBook())
			item->AddToCharacter(this, TItemPos(INVENTORY, cell));
		else if (item->IsUpgradeItem())
			item->AddToCharacter(this, TItemPos(INVENTORY, cell));
#endif
		else
			item->AddToCharacter(this, TItemPos(INVENTORY, cell));

		LogManager::instance().ItemLog(this, item, "SYSTEM", item->GetName());

		if (item->GetType() == ITEM_USE && item->GetSubType() == USE_POTION)
		{
			TQuickslot * pSlot;

			if (GetQuickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE)
			{
				TQuickslot slot;
				slot.type = QUICKSLOT_TYPE_ITEM;
				slot.pos = cell;
				SetQuickslot(0, slot);
			}
		}
	}
	else
	{
		item->AddToGround (GetMapIndex(), GetXYZ());
#ifdef ENABLE_NEWSTUFF
		item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
		item->StartDestroyEvent();
#endif

		if (longOwnerShip)
			item->SetOwnership (this, 300);
		else
			item->SetOwnership (this, 60);
		LogManager::instance().ItemLog(this, item, "SYSTEM_DROP", item->GetName());
	}
}

LPITEM CHARACTER::AutoGiveItem(DWORD dwItemVnum, BYTE bCount, int iRarePct, bool bMsg)
{
	TItemTable * p = ITEM_MANAGER::instance().GetTable(dwItemVnum);

	if (!p)
		return NULL;

	DBManager::instance().SendMoneyLog(MONEY_LOG_DROP, dwItemVnum, bCount);

	if (p->dwFlags & ITEM_FLAG_STACKABLE && p->bType != ITEM_BLEND) 
	{
#ifdef WJ_INVENTORY_EX_SYSTEM
		for (int i = 0; i < GetInventoryEx(); ++i)
#else
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		{
			LPITEM item = GetInventoryItem(i);

			if (!item)
				continue;

			if (item->GetVnum() == dwItemVnum && FN_check_item_socket(item))
			{
				if (IS_SET(p->dwFlags, ITEM_FLAG_MAKECOUNT))
				{
					if (bCount < p->alValues[1])
						bCount = p->alValues[1];
				}

				BYTE bCount2 = MIN(g_bItemCountLimit - item->GetCount(), bCount);
				bCount -= bCount2;

				item->SetCount(item->GetCount() + bCount2);

				if (bCount == 0)
				{
					if (bMsg)
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 획득: %s"), item->GetName());

					return item;
				}
			}
		}
	}
	
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	if (p->dwFlags & ITEM_FLAG_STACKABLE && p->bType != ITEM_BLEND) 
	{
		for (int i = SKILL_BOOK_INVENTORY_SLOT_START; i < UPGRADE_ITEMS_INVENTORY_SLOT_END; ++i)
		{
			LPITEM item = GetInventoryItem(i);

			if (!item)
				continue;

			if (item->GetVnum() == dwItemVnum && FN_check_item_socket(item))
			{
				if (IS_SET(p->dwFlags, ITEM_FLAG_MAKECOUNT))
				{
					if (bCount < p->alValues[1])
						bCount = p->alValues[1];
				}

				BYTE bCount2 = MIN(g_bItemCountLimit - item->GetCount(), bCount);
				bCount -= bCount2;

				item->SetCount(item->GetCount() + bCount2);

				if (bCount == 0)
				{
					if (bMsg)
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 획득: %s"), item->GetName());

					return item;
				}
			}
		}
	}
#endif

	LPITEM item = ITEM_MANAGER::instance().CreateItem(dwItemVnum, bCount, 0, true);

	if (!item)
	{
		sys_err("cannot create item by vnum %u (name: %s)", dwItemVnum, GetName());
		return NULL;
	}

	if (item->GetType() == ITEM_BLEND)
	{
#ifdef WJ_INVENTORY_EX_SYSTEM
		for (int i = 0; i < GetInventoryEx(); ++i)
#else
		for (int i=0; i < INVENTORY_MAX_NUM; i++)
#endif
		{
			LPITEM inv_item = GetInventoryItem(i);

			if (inv_item == NULL) continue;

			if (inv_item->GetType() == ITEM_BLEND)
			{
				if (inv_item->GetVnum() == item->GetVnum())
				{
					if (inv_item->GetSocket(0) == item->GetSocket(0) &&
							inv_item->GetSocket(1) == item->GetSocket(1) &&
							inv_item->GetSocket(2) == item->GetSocket(2) &&
							inv_item->GetCount() < g_bItemCountLimit)
					{
						inv_item->SetCount(inv_item->GetCount() + item->GetCount());
						return inv_item;
					}
				}
			}
		}
	}

	int iEmptyCell;
	if (item->IsDragonSoul())
		iEmptyCell = GetEmptyDragonSoulInventory(item);
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	else if (item->IsSkillBook())
		iEmptyCell = GetEmptySkillBookInventory(item->GetSize());
	else if (item->IsUpgradeItem())
		iEmptyCell = GetEmptyUpgradeItemsInventory(item->GetSize());
#endif
	else
		iEmptyCell = GetEmptyInventory(item->GetSize());

	if (iEmptyCell != -1)
	{
		if (bMsg)
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 획득: %s"), item->GetName());

		if (item->IsDragonSoul())
			item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyCell));
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
		else if (item->IsSkillBook())
			item->AddToCharacter(this, TItemPos(INVENTORY, iEmptyCell));
		else if (item->IsUpgradeItem())
			item->AddToCharacter(this, TItemPos(INVENTORY, iEmptyCell));
#endif
		else
			item->AddToCharacter(this, TItemPos(INVENTORY, iEmptyCell));
		LogManager::instance().ItemLog(this, item, "SYSTEM", item->GetName());

		if (item->GetType() == ITEM_USE && item->GetSubType() == USE_POTION)
		{
			TQuickslot * pSlot;

			if (GetQuickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE)
			{
				TQuickslot slot;
				slot.type = QUICKSLOT_TYPE_ITEM;
				slot.pos = iEmptyCell;
				SetQuickslot(0, slot);
			}
		}
	}
	else
	{
		item->AddToGround(GetMapIndex(), GetXYZ());
#ifdef ENABLE_NEWSTUFF
		item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
		item->StartDestroyEvent();
#endif
		// 안티 드랍 flag가 걸려있는 아이템의 경우, 
		// 인벤에 빈 공간이 없어서 어쩔 수 없이 떨어트리게 되면,
		// ownership을 아이템이 사라질 때까지(300초) 유지한다.
		if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_DROP))
			item->SetOwnership(this, 300);
		else
			item->SetOwnership(this, 60);
		LogManager::instance().ItemLog(this, item, "SYSTEM_DROP", item->GetName());
	}

	sys_log(0, 
		"7: %d %d", dwItemVnum, bCount);
	return item;
}

bool CHARACTER::GiveItem(LPCHARACTER victim, TItemPos Cell)
{
	if (!CanHandleItem())
		return false;

	LPITEM item = GetItem(Cell);

	if (item && !item->IsExchanging())
	{
		if (victim->CanReceiveItem(this, item))
		{
			victim->ReceiveItem(this, item);
			return true;
		}
	}

	return false;
}

bool CHARACTER::CanReceiveItem(LPCHARACTER from, LPITEM item) const
{
	if (IsPC())
		return false;

	// TOO_LONG_DISTANCE_EXCHANGE_BUG_FIX
	if (DISTANCE_APPROX(GetX() - from->GetX(), GetY() - from->GetY()) > 2000)
		return false;
	// END_OF_TOO_LONG_DISTANCE_EXCHANGE_BUG_FIX

#ifdef WJ_SOULBINDING_SYSTEM
	if (item->IsBind())
	{
		from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't refine this item because is binded!"));
		return false;
	}

	if (item->IsUntilBind())
	{
		from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't refine this item because is binded!"));
		return false;
	}
#endif

	switch (GetRaceNum())
	{
		case fishing::CAMPFIRE_MOB:
			if (item->GetType() == ITEM_FISH && 
					(item->GetSubType() == FISH_ALIVE || item->GetSubType() == FISH_DEAD))
				return true;
			break;

		case fishing::FISHER_MOB:
			if (item->GetType() == ITEM_ROD)
				return true;
			break;

			// BUILDING_NPC
		case BLACKSMITH_WEAPON_MOB:
		case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
			if (item->GetType() == ITEM_WEAPON && 
					item->GetRefinedVnum())
				return true;
			else
				return false;
			break;

		case BLACKSMITH_ARMOR_MOB:
		case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
			if (item->GetType() == ITEM_ARMOR && 
					(item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_SHIELD || item->GetSubType() == ARMOR_HEAD) &&
					item->GetRefinedVnum())
				return true;
			else
				return false;
			break;

		case BLACKSMITH_ACCESSORY_MOB:
		case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
			if (item->GetType() == ITEM_ARMOR &&
					!(item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_SHIELD || item->GetSubType() == ARMOR_HEAD) &&
					item->GetRefinedVnum())
				return true;
			else
				return false;
			break;
			// END_OF_BUILDING_NPC

		case BLACKSMITH_MOB:
			if (item->GetRefinedVnum() && item->GetRefineSet() < 500)
				return true;
			else
				return false;

		case BLACKSMITH2_MOB:
			if (item->GetRefineSet() >= 500)
				return true;
			else
				return false;

		case ALCHEMIST_MOB:
			if (item->GetRefinedVnum())
				return true;
			break;

		case 20101:
		case 20102:
		case 20103:
			// 초급 말
			if (item->GetVnum() == ITEM_REVIVE_HORSE_1)
			{
				if (!IsDead())
				{
					from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("죽지 않은 말에게 선초를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->GetVnum() == ITEM_HORSE_FOOD_1)
			{
				if (IsDead())
				{
					from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("죽은 말에게 사료를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->GetVnum() == ITEM_HORSE_FOOD_2 || item->GetVnum() == ITEM_HORSE_FOOD_3)
			{
				return false;
			}
			break;
		case 20104:
		case 20105:
		case 20106:
			// 중급 말
			if (item->GetVnum() == ITEM_REVIVE_HORSE_2)
			{
				if (!IsDead())
				{
					from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("죽지 않은 말에게 선초를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->GetVnum() == ITEM_HORSE_FOOD_2)
			{
				if (IsDead())
				{
					from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("죽은 말에게 사료를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->GetVnum() == ITEM_HORSE_FOOD_1 || item->GetVnum() == ITEM_HORSE_FOOD_3)
			{
				return false;
			}
			break;
		case 20107:
		case 20108:
		case 20109:
			// 고급 말
			if (item->GetVnum() == ITEM_REVIVE_HORSE_3)
			{
				if (!IsDead())
				{
					from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("죽지 않은 말에게 선초를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->GetVnum() == ITEM_HORSE_FOOD_3)
			{
				if (IsDead())
				{
					from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("죽은 말에게 사료를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->GetVnum() == ITEM_HORSE_FOOD_1 || item->GetVnum() == ITEM_HORSE_FOOD_2)
			{
				return false;
			}
			break;
	}

	//if (IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_GIVE))
	{
		return true;
	}

	return false;
}

void CHARACTER::ReceiveItem(LPCHARACTER from, LPITEM item)
{
	if (IsPC())
		return;

	switch (GetRaceNum())
	{
		case fishing::CAMPFIRE_MOB:
			if (item->GetType() == ITEM_FISH && (item->GetSubType() == FISH_ALIVE || item->GetSubType() == FISH_DEAD))
				fishing::Grill(from, item);
			else
			{
				// TAKE_ITEM_BUG_FIX
				from->SetQuestNPCID(GetVID());
				// END_OF_TAKE_ITEM_BUG_FIX
				quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
			}
			break;
			
#ifdef WJ_MELEY_LAIR_DUNGEON
		case MeleyLair::STATUE_VNUM:
			{
				if (MeleyLair::CMgr::instance().IsMeleyMap(from->GetMapIndex()))
					MeleyLair::CMgr::instance().OnKillStatue(item, from, this, from->GetGuild());
			}
			break;
#endif

			// DEVILTOWER_NPC 
		case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
		case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
		case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
			if (item->GetRefinedVnum() != 0 && item->GetRefineSet() != 0 && item->GetRefineSet() < 500)
			{
				from->SetRefineNPC(this);
				from->RefineInformation(item->GetCell(), REFINE_TYPE_MONEY_ONLY);
			}
			else
			{
				from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템은 개량할 수 없습니다."));
			}
			break;
			// END_OF_DEVILTOWER_NPC

		case BLACKSMITH_MOB:
		case BLACKSMITH2_MOB:
		case BLACKSMITH_WEAPON_MOB:
		case BLACKSMITH_ARMOR_MOB:
		case BLACKSMITH_ACCESSORY_MOB:
			if (item->GetRefinedVnum())
			{
				from->SetRefineNPC(this);
				from->RefineInformation(item->GetCell(), REFINE_TYPE_NORMAL);
			}
			else
			{
				from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이 아이템은 개량할 수 없습니다."));
			}
			break;

		case 20101:
		case 20102:
		case 20103:
		case 20104:
		case 20105:
		case 20106:
		case 20107:
		case 20108:
		case 20109:
			if (item->GetVnum() == ITEM_REVIVE_HORSE_1 ||
					item->GetVnum() == ITEM_REVIVE_HORSE_2 ||
					item->GetVnum() == ITEM_REVIVE_HORSE_3)
			{
				from->ReviveHorse();
				item->SetCount(item->GetCount()-1);
				from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("말에게 선초를 주었습니다."));
			}
			else if (item->GetVnum() == ITEM_HORSE_FOOD_1 ||
					item->GetVnum() == ITEM_HORSE_FOOD_2 ||
					item->GetVnum() == ITEM_HORSE_FOOD_3)
			{
				from->FeedHorse();
				from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("말에게 사료를 주었습니다."));
				item->SetCount(item->GetCount()-1);
				EffectPacket(SE_HPUP_RED);
			}
			break;

		default:
			sys_log(0, "TakeItem %s %d %s", from->GetName(), GetRaceNum(), item->GetName());
			from->SetQuestNPCID(GetVID());
			quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
			break;
	}
}

bool CHARACTER::IsEquipUniqueItem(DWORD dwItemVnum) const
{
	{
		LPITEM u = GetWear(WEAR_UNIQUE1);

		if (u && u->GetVnum() == dwItemVnum)
			return true;
	}

	{
		LPITEM u = GetWear(WEAR_UNIQUE2);

		if (u && u->GetVnum() == dwItemVnum)
			return true;
	}

	// 언어반지인 경우 언어반지(견본) 인지도 체크한다.
	if (dwItemVnum == UNIQUE_ITEM_RING_OF_LANGUAGE)
		return IsEquipUniqueItem(UNIQUE_ITEM_RING_OF_LANGUAGE_SAMPLE);

	return false;
}

// CHECK_UNIQUE_GROUP
bool CHARACTER::IsEquipUniqueGroup(DWORD dwGroupVnum) const
{
	{
		LPITEM u = GetWear(WEAR_UNIQUE1);

		if (u && u->GetSpecialGroup() == (int) dwGroupVnum)
			return true;
	}

	{
		LPITEM u = GetWear(WEAR_UNIQUE2);

		if (u && u->GetSpecialGroup() == (int) dwGroupVnum)
			return true;
	}

	return false;
}
// END_OF_CHECK_UNIQUE_GROUP

void CHARACTER::SetRefineMode(int iAdditionalCell)
{
	m_iRefineAdditionalCell = iAdditionalCell;
	m_bUnderRefine = true;
}

void CHARACTER::ClearRefineMode()
{
	m_bUnderRefine = false;
	SetRefineNPC( NULL );
}

bool CHARACTER::GiveItemFromSpecialItemGroup(DWORD dwGroupNum, std::vector<DWORD> &dwItemVnums, 
											std::vector<DWORD> &dwItemCounts, std::vector <LPITEM> &item_gets, int &count)
{
	const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(dwGroupNum);

	if (!pGroup)
	{
		sys_err("cannot find special item group %d", dwGroupNum);
		return false;
	}

	std::vector <int> idxes;
	int n = pGroup->GetMultiIndex(idxes);

	bool bSuccess;

	for (int i = 0; i < n; i++)
	{
		bSuccess = false;
		int idx = idxes[i];
		DWORD dwVnum = pGroup->GetVnum(idx);
		DWORD dwCount = pGroup->GetCount(idx);
		int	iRarePct = pGroup->GetRarePct(idx);
		LPITEM item_get = NULL;
		switch (dwVnum)
		{
			case CSpecialItemGroup::GOLD:
				PointChange(POINT_GOLD, dwCount);
				LogManager::instance().CharLog(this, dwCount, "TREASURE_GOLD", "");

				bSuccess = true;
				break;
			case CSpecialItemGroup::EXP:
				{
					PointChange(POINT_EXP, dwCount);
					LogManager::instance().CharLog(this, dwCount, "TREASURE_EXP", "");

					bSuccess = true;
				}
				break;

			case CSpecialItemGroup::MOB:
				{
					sys_log(0, "CSpecialItemGroup::MOB %d", dwCount);
					int x = GetX() + number(-500, 500);
					int y = GetY() + number(-500, 500);

					LPCHARACTER ch = CHARACTER_MANAGER::instance().SpawnMob(dwCount, GetMapIndex(), x, y, 0, true, -1);
					if (ch)
						ch->SetAggressive();
					bSuccess = true;
				}
				break;
			case CSpecialItemGroup::SLOW:
				{
					sys_log(0, "CSpecialItemGroup::SLOW %d", -(int)dwCount);
					AddAffect(AFFECT_SLOW, POINT_MOV_SPEED, -(int)dwCount, AFF_SLOW, 300, 0, true);
					bSuccess = true;
				}
				break;
			case CSpecialItemGroup::DRAIN_HP:
				{
					int iDropHP = GetMaxHP()*dwCount/100;
					sys_log(0, "CSpecialItemGroup::DRAIN_HP %d", -iDropHP);
					iDropHP = MIN(iDropHP, GetHP()-1);
					sys_log(0, "CSpecialItemGroup::DRAIN_HP %d", -iDropHP);
					PointChange(POINT_HP, -iDropHP);
					bSuccess = true;
				}
				break;
			case CSpecialItemGroup::POISON:
				{
					AttackedByPoison(NULL);
					bSuccess = true;
				}
				break;
#ifdef WJ_WOLFMAN_CHARACTER
			case CSpecialItemGroup::BLEEDING:
				{
					AttackedByBleeding(NULL);
					bSuccess = true;
				}
				break;
#endif
				
			case CSpecialItemGroup::MOB_GROUP:
				{
					int sx = GetX() - number(300, 500);
					int sy = GetY() - number(300, 500);
					int ex = GetX() + number(300, 500);
					int ey = GetY() + number(300, 500);
					CHARACTER_MANAGER::instance().SpawnGroup(dwCount, GetMapIndex(), sx, sy, ex, ey, NULL, true);

					bSuccess = true;
				}
				break;
			default:
				{
					item_get = AutoGiveItem(dwVnum, dwCount, iRarePct);

					if (item_get)
					{
						bSuccess = true;
					}
				}
				break;
		}
	
		if (bSuccess)
		{
			dwItemVnums.push_back(dwVnum);
			dwItemCounts.push_back(dwCount);
			item_gets.push_back(item_get);
			count++;

		}
		else
		{
			return false;
		}
	}
	return bSuccess;
}

// NEW_HAIR_STYLE_ADD
bool CHARACTER::ItemProcess_Hair(LPITEM item, int iDestCell)
{
	if (item->CheckItemUseLevel(GetLevel()) == false)
	{
		// 레벨 제한에 걸림
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아직 이 머리를 사용할 수 없는 레벨입니다."));
		return false;
	}

	DWORD hair = item->GetVnum();

	switch (GetJob())
	{
		case JOB_WARRIOR :
			hair -= 72000; // 73001 - 72000 = 1001 부터 헤어 번호 시작
			break;

		case JOB_ASSASSIN :
			hair -= 71250;
			break;

		case JOB_SURA :
			hair -= 70500;
			break;

		case JOB_SHAMAN :
			hair -= 69750;
			break;

#ifdef WJ_WOLFMAN_CHARACTER
		case JOB_WOLFMAN:
			break; // NOTE: 이 헤어코드는 안 쓰이므로 패스. (현재 헤어시스템은 이미 코스튬으로 대체 된 상태임)
#endif
			
		default :
			return false;
			break;
	}

	if (hair == GetPart(PART_HAIR))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("동일한 머리 스타일로는 교체할 수 없습니다."));
		return true;
	}

	item->SetCount(item->GetCount() - 1);

	SetPart(PART_HAIR, hair);
	UpdatePacket();

	return true;
}
// END_NEW_HAIR_STYLE_ADD

bool CHARACTER::ItemProcess_Polymorph(LPITEM item)
{
	if (IsPolymorphed())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 둔갑중인 상태입니다."));
		return false;
	}

	if (true == IsRiding())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("둔갑할 수 없는 상태입니다."));
		return false;
	}

	DWORD dwVnum = item->GetSocket(0);

	if (dwVnum == 0)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("잘못된 둔갑 아이템입니다."));
		item->SetCount(item->GetCount()-1);
		return false;
	}

	const CMob* pMob = CMobManager::instance().Get(dwVnum);

	if (pMob == NULL)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("잘못된 둔갑 아이템입니다."));
		item->SetCount(item->GetCount()-1);
		return false;
	}

	switch (item->GetVnum())
	{
		case 70104 :
		case 70105 :
		case 70106 :
		case 70107 :
		case 71093 :
			{
				// 둔갑구 처리
				sys_log(0, "USE_POLYMORPH_BALL PID(%d) vnum(%d)", GetPlayerID(), dwVnum);

				// 레벨 제한 체크
				int iPolymorphLevelLimit = MAX(0, 20 - GetLevel() * 3 / 10);
				if (pMob->m_table.bLevel >= GetLevel() + iPolymorphLevelLimit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("나보다 너무 높은 레벨의 몬스터로는 변신 할 수 없습니다."));
					return false;
				}

				int iDuration = GetSkillLevel(POLYMORPH_SKILL_ID) == 0 ? 5 : (5 + (5 + GetSkillLevel(POLYMORPH_SKILL_ID)/40 * 25));
				iDuration *= 60;

				DWORD dwBonus = 0;
				
				dwBonus = (2 + GetSkillLevel(POLYMORPH_SKILL_ID)/40) * 100;

				AddAffect(AFFECT_POLYMORPH, POINT_POLYMORPH, dwVnum, AFF_POLYMORPH, iDuration, 0, true);
				AddAffect(AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonus, AFF_POLYMORPH, iDuration, 0, false);
				
				item->SetCount(item->GetCount()-1);
			}
			break;

		case 50322:
			{
				// 보류

				// 둔갑서 처리
				// 소켓0                소켓1           소켓2   
				// 둔갑할 몬스터 번호   수련정도        둔갑서 레벨
				sys_log(0, "USE_POLYMORPH_BOOK: %s(%u) vnum(%u)", GetName(), GetPlayerID(), dwVnum);

				if (CPolymorphUtils::instance().PolymorphCharacter(this, item, pMob) == true)
				{
					CPolymorphUtils::instance().UpdateBookPracticeGrade(this, item);
				}
				else
				{
				}
			}
			break;

		default :
			sys_err("POLYMORPH invalid item passed PID(%d) vnum(%d)", GetPlayerID(), item->GetOriginalVnum());
			return false;
	}

	return true;
}

bool CHARACTER::CanDoCube() const
{
	if (m_bIsObserver)	return false;
	if (GetShop())		return false;
	if (GetMyShop())	return false;
	if (m_bUnderRefine)	return false;
	if (IsWarping())	return false;
#ifdef WJ_OFFLINESHOP_SYSTEM
	if (GetOfflineShop()) return false;
#endif
#ifdef WJ_ITEM_COMBINATION_SYSTEM
	if (IsCombOpen()) return false;
#endif

	return true;
}

#ifdef WJ_ITEM_COMBINATION_SYSTEM
bool CHARACTER::CanDoComb() const
{
	if (m_bIsObserver) return false;
	if (GetShop()) return false;
	if (GetMyShop()) return false;
	if (m_bUnderRefine) return false;
	if (IsWarping()) return false;
#ifdef WJ_OFFLINESHOP_SYSTEM
	if (GetOfflineShop()) return false;
#endif
	if (IsCubeOpen()) return false;

	return true;
}
#endif

#ifdef WJ_MOUNT_SYSTEM
bool CHARACTER::UnEquipMountUniqueItem()
{
	LPITEM pkItem = GetWear(WEAR_COSTUME_MOUNT);
	if (pkItem != NULL)
		return UnequipItem(pkItem);
	
	return false;
}
#endif

bool CHARACTER::UnEquipSpecialRideUniqueItem()
{
	LPITEM Unique1 = GetWear(WEAR_UNIQUE1);
	LPITEM Unique2 = GetWear(WEAR_UNIQUE2);

	if( NULL != Unique1 )
	{
		if( UNIQUE_GROUP_SPECIAL_RIDE == Unique1->GetSpecialGroup() )
		{
			return UnequipItem(Unique1);
		}
	}

	if( NULL != Unique2 )
	{
		if( UNIQUE_GROUP_SPECIAL_RIDE == Unique2->GetSpecialGroup() )
		{
			return UnequipItem(Unique2);
		}
	}

	return true;
}

void CHARACTER::AutoRecoveryItemProcess(const EAffectTypes type)
{
	if (true == IsDead() || true == IsStun())
		return;

	if (false == IsPC())
		return;

	if (AFFECT_AUTO_HP_RECOVERY != type && AFFECT_AUTO_SP_RECOVERY != type)
		return;

	if (NULL != FindAffect(AFFECT_STUN))
		return;

	{
		const DWORD stunSkills[] = { SKILL_TANHWAN, SKILL_GEOMPUNG, SKILL_BYEURAK, SKILL_GIGUNG };

		for (size_t i=0 ; i < sizeof(stunSkills)/sizeof(DWORD) ; ++i)
		{
			const CAffect* p = FindAffect(stunSkills[i]);

			if (NULL != p && AFF_STUN == p->dwFlag)
				return;
		}
	}

	const CAffect* pAffect = FindAffect(type);
	const size_t idx_of_amount_of_used = 1;
	const size_t idx_of_amount_of_full = 2;

	if (NULL != pAffect)
	{
		LPITEM pItem = FindItemByID(pAffect->dwFlag);

		if (NULL != pItem && true == pItem->GetSocket(0))
		{
			if (false == CArenaManager::instance().IsArenaMap(GetMapIndex()))
			{
				const long amount_of_used = pItem->GetSocket(idx_of_amount_of_used);
				const long amount_of_full = pItem->GetSocket(idx_of_amount_of_full);
				
				const int32_t avail = amount_of_full - amount_of_used;

				int32_t amount = 0;

				if (AFFECT_AUTO_HP_RECOVERY == type)
				{
					amount = GetMaxHP() - (GetHP() + GetPoint(POINT_HP_RECOVERY));
				}
				else if (AFFECT_AUTO_SP_RECOVERY == type)
				{
					amount = GetMaxSP() - (GetSP() + GetPoint(POINT_SP_RECOVERY));
				}

				if (amount > 0)
				{
					if (avail > amount)
					{
						const int pct_of_used = amount_of_used * 100 / amount_of_full;
						const int pct_of_will_used = (amount_of_used + amount) * 100 / amount_of_full;

						bool bLog = false;
						// 사용량의 10% 단위로 로그를 남김
						// (사용량의 %에서, 십의 자리가 바뀔 때마다 로그를 남김.)
						if ((pct_of_will_used / 10) - (pct_of_used / 10) >= 1)
							bLog = true;
						pItem->SetSocket(idx_of_amount_of_used, amount_of_used + amount, bLog);
					}
					else
					{
						amount = avail;

						ITEM_MANAGER::instance().RemoveItem( pItem );
					}

					if (AFFECT_AUTO_HP_RECOVERY == type)
					{
						PointChange( POINT_HP_RECOVERY, amount );
						EffectPacket( SE_AUTO_HPUP );
					}
					else if (AFFECT_AUTO_SP_RECOVERY == type)
					{
						PointChange( POINT_SP_RECOVERY, amount );
						EffectPacket( SE_AUTO_SPUP );
					}
				}
			}
			else
			{
				pItem->Lock(false);
				pItem->SetSocket(0, false);
				RemoveAffect( const_cast<CAffect*>(pAffect) );
			}
		}
		else
		{
			RemoveAffect( const_cast<CAffect*>(pAffect) );
		}
	}
}

bool CHARACTER::IsValidItemPosition(TItemPos Pos) const
{
	BYTE window_type = Pos.window_type;
	WORD cell = Pos.cell;
	
	switch (window_type)
	{
	case RESERVED_WINDOW:
		return false;

	case INVENTORY:
	case EQUIPMENT:
		return cell < (INVENTORY_AND_EQUIP_SLOT_MAX);

	case DRAGON_SOUL_INVENTORY:
		return cell < (DRAGON_SOUL_INVENTORY_MAX_NUM);

	case SAFEBOX:
		if (NULL != m_pkSafebox)
			return m_pkSafebox->IsValidPosition(cell);
		else
			return false;

	case MALL:
		if (NULL != m_pkMall)
			return m_pkMall->IsValidPosition(cell);
		else
			return false;
	default:
		return false;
	}
}


// 귀찮아서 만든 매크로.. exp가 true면 msg를 출력하고 return false 하는 매크로 (일반적인 verify 용도랑은 return 때문에 약간 반대라 이름때문에 헷갈릴 수도 있겠다..)
#define VERIFY_MSG(exp, msg)  \
	if (true == (exp)) { \
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT(msg)); \
			return false; \
	}

		
/// 현재 캐릭터의 상태를 바탕으로 주어진 item을 착용할 수 있는 지 확인하고, 불가능 하다면 캐릭터에게 이유를 알려주는 함수
bool CHARACTER::CanEquipNow(const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell) /*const*/
{
	const TItemTable* itemTable = item->GetProto();
	// BYTE itemType = item->GetType();
	// BYTE itemSubType = item->GetSubType();

	switch (GetJob())
	{
		case JOB_WARRIOR:
			if (item->GetAntiFlag() & ITEM_ANTIFLAG_WARRIOR)
				return false;
			break;

		case JOB_ASSASSIN:
			if (item->GetAntiFlag() & ITEM_ANTIFLAG_ASSASSIN)
				return false;
			break;

		case JOB_SHAMAN:
			if (item->GetAntiFlag() & ITEM_ANTIFLAG_SHAMAN)
				return false;
			break;

		case JOB_SURA:
			if (item->GetAntiFlag() & ITEM_ANTIFLAG_SURA)
				return false;
			break;
			
#ifdef WJ_WOLFMAN_CHARACTER
		case JOB_WOLFMAN:
			if (item->GetAntiFlag() & ITEM_ANTIFLAG_WOLFMAN)
				return false;
			break; // TODO: 수인족 아이템 착용가능여부 처리
#endif
	}

	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		long limit = itemTable->aLimits[i].lValue;
		switch (itemTable->aLimits[i].bType)
		{
			case LIMIT_LEVEL:
				if (GetLevel() < limit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("레벨이 낮아 착용할 수 없습니다."));
					return false;
				}
				break;

			case LIMIT_STR:
				if (GetPoint(POINT_ST) < limit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("근력이 낮아 착용할 수 없습니다."));
					return false;
				}
				break;

			case LIMIT_INT:
				if (GetPoint(POINT_IQ) < limit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("지능이 낮아 착용할 수 없습니다."));
					return false;
				}
				break;

			case LIMIT_DEX:
				if (GetPoint(POINT_DX) < limit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("민첩이 낮아 착용할 수 없습니다."));
					return false;
				}
				break;

			case LIMIT_CON:
				if (GetPoint(POINT_HT) < limit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("체력이 낮아 착용할 수 없습니다."));
					return false;
				}
				break;
		}
	}

	if (item->GetWearFlag() & WEARABLE_UNIQUE)
	{
		if ((GetWear(WEAR_UNIQUE1) && GetWear(WEAR_UNIQUE1)->IsSameSpecialGroup(item)) ||
			(GetWear(WEAR_UNIQUE2) && GetWear(WEAR_UNIQUE2)->IsSameSpecialGroup(item)))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("같은 종류의 유니크 아이템 두 개를 동시에 장착할 수 없습니다."));
			return false;
		}

		if (marriage::CManager::instance().IsMarriageUniqueItem(item->GetVnum()) && 
			!marriage::CManager::instance().IsMarried(GetPlayerID()))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("결혼하지 않은 상태에서 예물을 착용할 수 없습니다."));
			return false;
		}

	}
	
#ifdef WJ_WEAPON_COSTUME_SYSTEM
	if (item->GetType() == ITEM_WEAPON && item->GetSubType() != WEAPON_ARROW)
	{
		LPITEM pkItem = GetWear(WEAR_COSTUME_WEAPON);
		if (pkItem)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("For can do this unwear the costume weapon."));
			return false;
		}
	}
	else if (item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_WEAPON)
	{
		LPITEM pkItem = GetWear(WEAR_WEAPON);
		if (!pkItem)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't wear a costume weapon without have a weapon weared."));
			return false;
		}
		else if (item->GetValue(3) != pkItem->GetSubType())
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't wear a costume weapon who has different type of your weapon."));
			return false;
		}
	}
#endif
	
	// @fixme47
	LPITEM ring1;
	LPITEM ring2;
	
	if (item->GetType() == ITEM_RING)
	{
		LPITEM ring1 = GetWear(WEAR_RING1);
		LPITEM ring2 = GetWear(WEAR_RING2);
		
		if (ring1 && (ring1->GetVnum() == item->GetVnum()))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("같은 종류의 유니크 아이템 두 개를 동시에 장착할 수 없습니다."));
			return false;			
		}
		
		if (ring2 && (ring2->GetVnum() == item->GetVnum()))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("같은 종류의 유니크 아이템 두 개를 동시에 장착할 수 없습니다."));
			return false;			
		}
	}

	return true;
}

/// 현재 캐릭터의 상태를 바탕으로 착용 중인 item을 벗을 수 있는 지 확인하고, 불가능 하다면 캐릭터에게 이유를 알려주는 함수
bool CHARACTER::CanUnequipNow(const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell) /*const*/
{	

	if (ITEM_BELT == item->GetType())
		VERIFY_MSG(CBeltInventoryHelper::IsExistItemInBeltInventory(this), "벨트 인벤토리에 아이템이 존재하면 해제할 수 없습니다.");

	// 영원히 해제할 수 없는 아이템
	if (IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		return false;

	// 아이템 unequip시 인벤토리로 옮길 때 빈 자리가 있는 지 확인
	{
		int pos = -1;

		if (item->IsDragonSoul())
			pos = GetEmptyDragonSoulInventory(item);
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
		else if (item->IsSkillBook())
			pos = GetEmptySkillBookInventory(item->GetSize());
		else if (item->IsUpgradeItem())
			pos = GetEmptyUpgradeItemsInventory(item->GetSize());
#endif
		else
			pos = GetEmptyInventory(item->GetSize());

		VERIFY_MSG( -1 == pos, "소지품에 빈 공간이 없습니다." );
	}

#ifdef WJ_WEAPON_COSTUME_SYSTEM
	if (item->GetType() == ITEM_WEAPON && item->GetSubType() != WEAPON_ARROW)
	{
		LPITEM pkItem = GetWear(WEAR_COSTUME_WEAPON);
		if (pkItem)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("For can do this unwear the costume weapon."));
			return false;
		}
	}
#endif


	return true;
}

#ifdef WJ_ACCE_SYSTEM
int GetAcceRefineGrade(DWORD vnum)
{
	if (vnum >= 85001 && vnum <= 85004)
		return vnum - 85000;
	else if (vnum >= 85005 && vnum <= 85008)
		return vnum - 85004;
	else if (vnum >= 85011 && vnum <= 85014)
		return vnum - 85010;
	else if (vnum >= 85015 && vnum <= 85018)
		return vnum - 85014;
	else
		return 0;
}

void CHARACTER::AcceRefineCheckin(BYTE acceSlot, TItemPos currentCell)
{
	if (GetAcceWindowType() == 3)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("INVALID_ACCE_WINDOW_HACKER_QST"));
		return;
	}

	if (IsOpenSafebox() || IsCubeOpen())
		return;

	if (!IsValidItemPosition(currentCell))
		return;

	if (IsOpenSafebox())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_PLEASE_CLOSE_SAFEBOX"));
		return;
	}

	if (m_pointsInstant.pAcceSlots[acceSlot] != WORD_MAX)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_SLOT_ALREADY_IN_USE"));
		return;
	}

	LPITEM pkItem = GetItem(currentCell);

	if (pkItem == NULL)
		return;

	if (ABSORB == GetAcceWindowType())
	{
		//ChatInfoCond(test_server, "AcceRefineCheckin Window Type Absorb %d", acceSlot);
		if (acceSlot == ACCE_SLOT_RIGHT)
		{
			if (pkItem->GetType() == ITEM_COSTUME)
				return;

			if (pkItem->GetType() != ITEM_WEAPON)
			{
				//ChatInfoCond(test_server, "AcceRefineCheckin Window Type Absorb %d No Weapon", acceSlot);
				return;
			}

			if (pkItem->GetType() == ITEM_WEAPON && pkItem->GetSubType() == WEAPON_ARROW)
				return;

			//if (pkItem->GetType() == ITEM_WEAPON && pkItem->GetSubType() == WEAPON_QUIVER)
			//return;
		}
		else if (acceSlot == ACCE_SLOT_LEFT)
		{
			if (pkItem->GetType() != ITEM_COSTUME)
				return;

			if (pkItem->GetSubType() != COSTUME_ACCE)
				return;

			if (!CItemVnumHelper::IsAcceItem(pkItem->GetType(), pkItem->GetSubType()))
			{
				return;
			}

			if (pkItem->GetSocket(1) != 0 && ABSORB == GetAcceWindowType())
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_ALREADY_ABSORBED_BONUS"));
				return;
			}

		}

	}
	else if (COMBINE == GetAcceWindowType())
	{
		if (pkItem->GetType() != ITEM_COSTUME && pkItem->GetSubType() != COSTUME_ACCE)
			return;
	}



	if (pkItem->GetCell() >= INVENTORY_MAX_NUM && IS_SET(pkItem->GetFlag(), ITEM_FLAG_IRREMOVABLE))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<창고> 창고로 옮길 수 없는 아이템 입니다."));
		return;
	}

	if (true == pkItem->isLocked())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<창고> 이 아이템은 넣을 수 없습니다."));
		return;
	}
	
#ifdef WJ_ACCE_SYSTEM
	if (true == pkItem->IsBind() || true == pkItem->IsUntilBind())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_NO_BIND_ITEM"));
		return;
	}
#endif

	if (COMBINE == GetAcceWindowType())
	{
		if (pkItem->GetType() != ITEM_COSTUME)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_NO_ACCE_ITEM"));
			return;
		}


		if (pkItem->GetSubType() != COSTUME_ACCE)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_NO_ACCE_ITEM"));
			return;
		}

		if (!CItemVnumHelper::IsAcceItem(pkItem->GetType(), pkItem->GetSubType()))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_NO_ACCE_ITEM"));
			return;
		}


		if (acceSlot == ACCE_SLOT_LEFT && pkItem->GetSocket(0) == ITEM_MAX_ACCEDRAIN)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_ALREADY_MAX"));
			return;
		}

		if (acceSlot == ACCE_SLOT_RIGHT && pkItem->GetSocket(0) == ITEM_MAX_ACCEDRAIN)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_ALREADY_MAX"));
			return;
		}

		LPITEM pkAcceChosen = NULL;
		if (acceSlot == ACCE_SLOT_LEFT)
			pkAcceChosen = GetItem(TItemPos(INVENTORY, m_pointsInstant.pAcceSlots[ACCE_SLOT_RIGHT]));
		else if (acceSlot == ACCE_SLOT_RIGHT)
			pkAcceChosen = GetItem(TItemPos(INVENTORY, m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT]));

		DWORD gradeOfChosen = 0;
		DWORD gradeOfNew = GetAcceRefineGrade(pkItem->GetVnum());

		if (pkAcceChosen != NULL)
		{
			gradeOfChosen = GetAcceRefineGrade(pkAcceChosen->GetVnum());

			if (gradeOfChosen != 0 && gradeOfNew != 0)
			{
				if (gradeOfNew != gradeOfChosen)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_CANNOT_COMBINE_DIFFERENT_GRADE"));
					return;
				}
			}
			else
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_CANNOT_COMBINE_DIFFERENT_GRADE"));
				return;
			}

		}
	}

	if (m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT] != currentCell.cell && m_pointsInstant.pAcceSlots[ACCE_SLOT_RIGHT] != currentCell.cell) {
		m_pointsInstant.pAcceSlots[acceSlot] = currentCell.cell;
	} else {
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_SLOT_ALREADY_IN_USE"));
		return;
	}

	if (ABSORB == GetAcceWindowType())
	{

		if (m_pointsInstant.pAcceSlots[ACCE_SLOT_RIGHT] != WORD_MAX && m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT] != WORD_MAX)
		{
			LPITEM pkAcce = GetItem(TItemPos(INVENTORY, m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT]));
			LPITEM pkWeaponToAbsorb = GetItem(TItemPos(INVENTORY, m_pointsInstant.pAcceSlots[ACCE_SLOT_RIGHT]));
			LPITEM pShowItem = ITEM_MANAGER::instance().CreateItem(pkAcce->GetVnum(), 1, 0, false, -1, true);

			pkWeaponToAbsorb->CopyAttributeTo(pShowItem);
			pShowItem->SetSocket(0, pkAcce->GetSocket(0));
			pShowItem->SetSocket(1, pkWeaponToAbsorb->GetVnum());

			TPacketGCAcce pack;
			pack.header = HEADER_GC_ACCE;
			pack.subheader = ACCE_SUBHEADER_GC_SET_ITEM;
			pack.size = sizeof(TPacketGCAcce) + sizeof(TPacketGCItemSet);

			TPacketGCItemSet pack_sub;

			pack_sub.header = 0;
			pack_sub.Cell = TItemPos(ACCEREFINE, (WORD)ACCE_SLOT_RESULT);
			pack_sub.vnum = pShowItem->GetVnum();
			pack_sub.count = (BYTE)pShowItem->GetCount();
			pack_sub.flags = pShowItem->GetFlag();
			pack_sub.anti_flags = pShowItem->GetAntiFlag();
			//pack_sub.nSealDate = pShowItem->GetSealDate();
			thecore_memcpy(pack_sub.alSockets, pShowItem->GetSockets(), sizeof(pack_sub.alSockets));
			thecore_memcpy(pack_sub.aAttr, pShowItem->GetAttributes(), sizeof(pack_sub.aAttr));
			GetDesc()->BufferedPacket(&pack, sizeof(TPacketGCAcce));
			GetDesc()->Packet(&pack_sub, sizeof(TPacketGCItemSet));

			M2_DESTROY_ITEM(pShowItem);
		}

	}

	if (COMBINE == GetAcceWindowType())
	{
		if (m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT] != WORD_MAX)
		{
			LPITEM pkAcce = GetItem(TItemPos(INVENTORY, m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT]));
			LPITEM pkNewAcce = ITEM_MANAGER::instance().CreateItem(pkAcce->GetRefinedVnum() ? pkAcce->GetRefinedVnum() : pkAcce->GetVnum(), 1, 0, false, -1, true);

			if (pkAcce->GetSocket(1) > 0) {
				pkNewAcce->SetSocket(1, pkAcce->GetSocket(1));

				if (pkAcce->GetAttributeCount() > 0)
					pkAcce->CopyAttributeTo(pkNewAcce);
			}

			TPacketGCAcce pack;
			pack.header = HEADER_GC_ACCE;
			pack.subheader = ACCE_SUBHEADER_GC_SET_ITEM;
			pack.size = sizeof(TPacketGCAcce) + sizeof(TPacketGCItemSet);

			TPacketGCItemSet pack_sub;

			pack_sub.header = 0;
			pack_sub.Cell = TItemPos(ACCEREFINE, (WORD)ACCE_SLOT_RESULT);
			pack_sub.vnum = pkNewAcce->GetVnum();
			pack_sub.count = (BYTE)pkNewAcce->GetCount();
			pack_sub.flags = pkNewAcce->GetFlag();
			pack_sub.anti_flags = pkNewAcce->GetAntiFlag();

			thecore_memcpy(pack_sub.alSockets, pkNewAcce->GetSockets(), sizeof(pack_sub.alSockets));
			thecore_memcpy(pack_sub.aAttr, pkNewAcce->GetAttributes(), sizeof(pack_sub.aAttr));
			GetDesc()->BufferedPacket(&pack, sizeof(TPacketGCAcce));
			GetDesc()->Packet(&pack_sub, sizeof(TPacketGCItemSet));
		}
	}


	TPacketGCAcce pack;
	pack.header = HEADER_GC_ACCE;
	pack.subheader = ACCE_SUBHEADER_GC_SET_ITEM;
	pack.size = sizeof(TPacketGCAcce) + sizeof(TPacketGCItemSet);

	TPacketGCItemSet pack_sub;

	pack_sub.header = 0;
	pack_sub.Cell = TItemPos(ACCEREFINE, (WORD)acceSlot);
	pack_sub.vnum = pkItem->GetVnum();
	pack_sub.count = (BYTE)pkItem->GetCount();
	pack_sub.flags = pkItem->GetFlag();

	pack_sub.anti_flags = pkItem->GetAntiFlag();
	thecore_memcpy(pack_sub.alSockets, pkItem->GetSockets(), sizeof(pack_sub.alSockets));
	thecore_memcpy(pack_sub.aAttr, pkItem->GetAttributes(), sizeof(pack_sub.aAttr));

	GetDesc()->BufferedPacket(&pack, sizeof(TPacketGCAcce));
	GetDesc()->Packet(&pack_sub, sizeof(TPacketGCItemSet));
}

void CHARACTER::AcceRefineCheckout(BYTE acceSlot)
{

	if (m_pointsInstant.pAcceSlots[acceSlot] == WORD_MAX)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_SLOT_ALREADY_EMPTY"));
		return;
	}

	m_pointsInstant.pAcceSlots[acceSlot] = WORD_MAX;

	LPDESC d = GetDesc();

	struct SPacketGCClearAcceSlot
	{
		WORD cell;

	};

	TPacketGCAcce p;
	p.header = HEADER_GC_ACCE;
	p.size = sizeof(TPacketGCAcce) + sizeof(SPacketGCClearAcceSlot);
	p.subheader = ACCE_SUBHEADER_GC_CLEAR_SLOT;

	SPacketGCClearAcceSlot p_sub;
	p_sub.cell = acceSlot;

	d->BufferedPacket(&p, sizeof(p));
	d->Packet(&p_sub, sizeof(p_sub));
}

void CHARACTER::AcceRefineAccept(int windowType)
{
	bool uppMax = false;
	if (ABSORB == windowType)
	{

		if (m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT] == WORD_MAX)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_NO_ACCE_ITEM_TO_ABSORB"));
			return;
		}


		if (m_pointsInstant.pAcceSlots[ACCE_SLOT_RIGHT] == WORD_MAX)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_NO_ACCE_ITEM_TO_ABSORB_FROM"));
			return;
		}

		LPITEM pAcce = GetItem(TItemPos(INVENTORY, m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT]));
		LPITEM pAbsorbItem = GetItem(TItemPos(INVENTORY, m_pointsInstant.pAcceSlots[ACCE_SLOT_RIGHT]));

		if (pAcce->IsEquipped() || pAbsorbItem->IsEquipped())
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_CANT_ABSORB_WHILE_EQUIPPED"));
			return;
		}

		if (pAcce && pAbsorbItem)
		{
			if (!CItemVnumHelper::IsAcceItem(pAcce->GetType(), pAcce->GetSubType()))
			{
				AcceRefineCheckout(ACCE_SLOT_LEFT);
				return;
			}

			WORD TargetCell = pAcce->GetCell();
			pAcce->RemoveFromCharacter();

			pAbsorbItem->CopyAttributeTo(pAcce);
			pAcce->SetSocket(1, pAbsorbItem->GetVnum());

			pAcce->AddToCharacter(this, TItemPos(INVENTORY, TargetCell));
			pAbsorbItem->RemoveFromCharacter();
			ITEM_MANAGER::instance().RemoveItem(pAbsorbItem, "ACCE_ABSORBED");
			AcceRefineCancel();

			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_SUCCESS"));
		}

	}
	else if (COMBINE == windowType)
	{

		if (m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT] == WORD_MAX)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_NO_ACCE_ITEM_TO_ABSORB"));
			return;
		}
		if (m_pointsInstant.pAcceSlots[ACCE_SLOT_RIGHT] == WORD_MAX)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_NO_ACCE_ITEM_TO_ABSORB_FROM"));
			return;
		}

		LPITEM pBaseItem = GetItem(TItemPos(INVENTORY, m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT]));
		LPITEM pMaterialItem = GetItem(TItemPos(INVENTORY, m_pointsInstant.pAcceSlots[ACCE_SLOT_RIGHT]));

		if (pBaseItem && pMaterialItem)
		{

			if (!CItemVnumHelper::IsAcceItem(pBaseItem->GetType(), pBaseItem->GetSubType()))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_CANNOT_COMBINE_NON_ACCE_ITEMS"));
				return;
			}

			if (!CItemVnumHelper::IsAcceItem(pMaterialItem->GetType(), pMaterialItem->GetSubType()))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_CANNOT_COMBINE_NON_ACCE_ITEMS"));
				return;
			}

			if (GetAcceRefineGrade(pBaseItem->GetVnum()) != GetAcceRefineGrade(pMaterialItem->GetVnum()))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_CANNOT_COMBINE_DIFFERENT_GRADE"));
				return;
			}

			DWORD result_vnum = 0;

			if ((pBaseItem->GetRefinedVnum() == 0) && (pBaseItem->GetSocket(0) < ITEM_MAX_ACCEDRAIN) && (GetAcceRefineGrade(pBaseItem->GetVnum()) == 4))
			{
				if (pBaseItem->GetSocket(0) < ITEM_MIN_ACCEDRAIN) {
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_COMBINE_ABSORB_TO_LOW"));
					return;
				}
				uppMax = true;
				result_vnum = pBaseItem->GetVnum();
			}
			else {
				result_vnum = pBaseItem->GetRefinedVnum();
			}

			const TRefineTable * prt = CRefineManager::instance().GetRefineRecipe(pBaseItem->GetRefineSet());

			if (!prt)
				return;

			DWORD cost = pBaseItem->GetProto()->dwGold;

			if (GetGold() < static_cast<unsigned long long>(cost))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("개량을 하기 위한 돈이 부족합니다."));
				return;
			}

			int prob = number(1, 100);

			if (uppMax)
			{
				if (pBaseItem->GetSocket(0) - ITEM_MIN_ACCEDRAIN <= 0) {
					prob += 0;
				}
				else {
					prob -= (pBaseItem->GetSocket(0) - ITEM_MIN_ACCEDRAIN) * 2;
					if (prob <= 0)
						prob = 1;
				}
			}

			if (prob <= prt->prob)
			{

				LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);
				if (pkNewItem)
				{

					pMaterialItem->RemoveFromCharacter();

					WORD Cell = pBaseItem->GetCell();

					pBaseItem->CopyAttributeTo(pkNewItem);
					pkNewItem->SetSocket(1, pBaseItem->GetSocket(1));

					if (!pkNewItem->GetRefinedVnum())
					{
						if (uppMax) {
							int num = number(pBaseItem->GetSocket(0) - 1, pkNewItem->GetProto()->aApplies[0].lValue);
							pkNewItem->SetSocket(0, num <= ITEM_MIN_ACCEDRAIN ? ITEM_MIN_ACCEDRAIN : num);
						}
						else {
							pkNewItem->SetSocket(0, number(pBaseItem->GetProto()->aApplies[0].lValue + 1, pkNewItem->GetProto()->aApplies[0].lValue - 1));
						}
					}
					ITEM_MANAGER::instance().RemoveItem(pMaterialItem, "ACCE_MAT");
					ITEM_MANAGER::instance().RemoveItem(pBaseItem, "ACCE_BASE");

					pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, Cell));
					ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

					LogManager::instance().ItemLog(this, pkNewItem, "ACCE COMBINE SUCCESS", pkNewItem->GetName());

					PayRefineFee(cost);
					AcceRefineCancel();

					EffectPacket(SE_ACCE_SUCESS);

					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_SUCCESS"));
				}
				else
				{
					sys_err("cannot create item %u", result_vnum);
					AcceRefineCancel();
				}

			}
			else
			{

				ITEM_MANAGER::instance().RemoveItem(pMaterialItem, "ACCE_FAIL");
				PayRefineFee(cost);
				AcceRefineCancel();

				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ACCE_FAILED"));
			}
		}
	}
}

void CHARACTER::AcceRefineCancel()
{
	TPacketGCAcce pack;
	pack.header = HEADER_GC_ACCE;
	pack.subheader = ACCE_SUBHEADER_GC_CLEAR_ALL;
	pack.size = sizeof(TPacketGCAcce);

	GetDesc()->Packet(&pack, sizeof(TPacketGCAcce));
	m_pointsInstant.pAcceSlots[ACCE_SLOT_RESULT] = WORD_MAX;
	m_pointsInstant.pAcceSlots[ACCE_SLOT_LEFT] = WORD_MAX;
	m_pointsInstant.pAcceSlots[ACCE_SLOT_RIGHT] = WORD_MAX;
}

void CHARACTER::AcceClose()
{
	//AcceRefineCancel();
	ChatPacket(CHAT_TYPE_COMMAND, "acce close");
}

#endif