#include "stdafx.h"
#include "../../libgame/include/grid.h"
#include "constants.h"
#include "utils.h"
#include "config.h"
#include "offline_shop.h"
#include "desc.h"
#include "desc_manager.h"
#include "char.h"
#include "char_manager.h"
#include "item.h"
#include "item_manager.h"
#include "buffer_manager.h"
#include "packet.h"
#include "log.h"
#include "db.h"
#include "questmanager.h"
#include "monarch.h"
#include "mob_manager.h"
#include "locale_service.h"
#include "desc_client.h"
#include "group_text_parse_tree.h"
#include <boost/algorithm/string/predicate.hpp>
#include <cctype>
#include "offlineshop_manager.h"
#include "p2p.h"
#include "entity.h"
#include "sectree_manager.h"
#include "target.h"

COfflineShopManager::COfflineShopManager()
{
}

COfflineShopManager::~COfflineShopManager()
{
}

struct FFindOfflineShop
{
	const char * szName;

	DWORD dwVID, dwRealOwner;
	FFindOfflineShop(const char * c_szName) : szName(c_szName), dwVID(0), dwRealOwner(0) {};

	void operator()(LPENTITY ent)
	{
		if (!ent)
			return;

		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER)ent;
			if (ch->IsOfflineShopNPC() && !strcmp(szName, ch->GetName()))
			{
				dwVID = ch->GetVID();
				dwRealOwner = ch->GetOfflineShopRealOwner();
				M2_DESTROY_CHARACTER(ch);
			}
		}
	}
};

bool COfflineShopManager::StartShopping(LPCHARACTER pkChr, LPCHARACTER pkChrShopKeeper)
{
	if (pkChr->GetOfflineShopOwner() == pkChrShopKeeper)
		return false;

	if (pkChrShopKeeper->IsPC())
		return false;

	sys_log(0, "OFFLINE_SHOP: START: %s", pkChr->GetName());

	return true;
}

LPOFFLINESHOP COfflineShopManager::CreateOfflineShop(LPCHARACTER npc, DWORD dwOwnerPID, TOfflineShopItemTable * pTable, short bItemCount, long lMapIndex, int iTime, const char * szSign)
{
	if (FindOfflineShop(npc->GetVID()))
		return NULL;

	LPOFFLINESHOP pkOfflineShop = M2_NEW COfflineShop;
	pkOfflineShop->SetOfflineShopNPC(npc);
	if (pTable)
	{
		pkOfflineShop->SetOfflineShopItems(dwOwnerPID, pTable, bItemCount);
		pkOfflineShop->SetOfflineShopBankValues(dwOwnerPID);
		pkOfflineShop->SetOfflineShopMapIndex(lMapIndex);
		pkOfflineShop->SetOfflineShopTime(6*60*60);
	}
	else
	{
		pkOfflineShop->CreateTable(dwOwnerPID);
		pkOfflineShop->SetOfflineShopBankValues(dwOwnerPID);
		pkOfflineShop->SetOfflineShopMapIndex(lMapIndex);
		pkOfflineShop->SetOfflineShopTime(iTime);
		pkOfflineShop->SetShopSign(szSign);
	}

	m_map_pkOfflineShopByNPC.insert(TShopMap::value_type(npc->GetVID(), pkOfflineShop));
	m_Map_pkOfflineShopByNPC2.insert(TOfflineShopMap::value_type(dwOwnerPID, npc->GetVID()));
	
	return pkOfflineShop;
}

LPOFFLINESHOP COfflineShopManager::FindOfflineShop(DWORD dwVID)
{
	TShopMap::iterator it = m_map_pkOfflineShopByNPC.find(dwVID);

	if (it == m_map_pkOfflineShopByNPC.end())
		return NULL;

	return it->second;
}

void COfflineShopManager::DestroyOfflineShop(LPCHARACTER ch, DWORD dwVID, bool bDestroyAll)
{
	if (dwVID == 0)
	{
		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT mapIndex, mapIndex,channel FROM %soffline_shop_npc WHERE owner_id = %u", get_table_postfix(), ch->GetPlayerID()));
		if (pMsg->Get()->uiNumRows == 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_OPTION_BLOCK"));
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

		long lMapIndex = 0;
		str_to_number(lMapIndex, row[0]);
		
		TPacketGGRemoveOfflineShop p;
		p.bHeader = HEADER_GG_REMOVE_OFFLINE_SHOP;
		p.lMapIndex = lMapIndex;

		char szNpcName[CHARACTER_NAME_MAX_LEN + 1];
		snprintf(szNpcName, sizeof(szNpcName), "%s", ch->GetName());
		strlcpy(p.szNpcName, szNpcName, sizeof(p.szNpcName));

		P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGRemoveOfflineShop));
		DBManager::instance().DirectQuery("UPDATE %soffline_shop_item SET status = 1 WHERE owner_id = %u and status = 0", get_table_postfix(), ch->GetPlayerID());
		DBManager::instance().DirectQuery("DELETE FROM %soffline_shop_item WHERE owner_id = %u and status = 2", get_table_postfix(), ch->GetPlayerID());
		DBManager::instance().DirectQuery("DELETE FROM %soffline_shop_npc WHERE owner_id = %u", get_table_postfix(), ch->GetPlayerID());
		TPacketUpdateOfflineShopsCount pCount;
		pCount.bIncrease = false;
		db_clientdesc->DBPacket(HEADER_GD_UPDATE_OFFLINESHOP_COUNT, 0, &pCount, sizeof(pCount));
	}
	else
	{		
		LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(dwVID);
		if (!npc)
			npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));

		if (!npc)
		{
			std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT mapIndex,channel FROM %soffline_shop_npc WHERE owner_id = %u", get_table_postfix(), ch->GetPlayerID()));
			if (pMsg->Get()->uiNumRows == 0)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_OPTION_BLOCK"));
				return;
			}

			MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

			long lMapIndex = 0;
			str_to_number(lMapIndex, row[0]);

			BYTE bChannel = 0;
			str_to_number(bChannel, row[1]);

			if (g_bChannel != bChannel)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_SAME_CHANNEL"), g_bChannel, bChannel);
				return;
			}

			char szName[CHARACTER_NAME_MAX_LEN + 1];
			snprintf(szName, sizeof(szName), "%s", ch->GetName());
			LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
			FFindOfflineShop offlineShop(szName);
			pMap->for_each(offlineShop);

			if (bDestroyAll)
			{
				DBManager::instance().DirectQuery("UPDATE %soffline_shop_item SET status = 1 WHERE owner_id = %u and status = 0", get_table_postfix(), ch->GetPlayerID());
				DBManager::instance().DirectQuery("DELETE FROM %soffline_shop_item WHERE owner_id = %u and status = 2", get_table_postfix(), ch->GetPlayerID());
				m_map_pkOfflineShopByNPC.erase(offlineShop.dwVID);
				m_Map_pkOfflineShopByNPC2.erase(offlineShop.dwRealOwner);
				DBManager::instance().DirectQuery("DELETE FROM %soffline_shop_npc WHERE owner_id = %u", get_table_postfix(), ch->GetPlayerID());
				return;
			}
		}

		LPOFFLINESHOP pkOfflineShop = FindOfflineShop(dwVID);
		if (!pkOfflineShop)
			pkOfflineShop = FindOfflineShop(FindMyOfflineShop(ch->GetPlayerID()));

		if (!pkOfflineShop)
			return;

		if (npc->GetOfflineShopChannel() != g_bChannel)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_SAME_CHANNEL"));
			return;
		}

		if (bDestroyAll)
		{
			DBManager::instance().DirectQuery("UPDATE %soffline_shop_item SET status = 1 WHERE owner_id = %u and status = 0", get_table_postfix(), ch->GetPlayerID());
			DBManager::instance().DirectQuery("DELETE FROM %soffline_shop_item WHERE owner_id = %u and status = 2", get_table_postfix(), ch->GetPlayerID());
			pkOfflineShop->Destroy(npc);
		}
		
		m_map_pkOfflineShopByNPC.erase(npc->GetVID());
		m_Map_pkOfflineShopByNPC2.erase(npc->GetOfflineShopRealOwner());
		M2_DELETE(pkOfflineShop);
		TPacketUpdateOfflineShopsCount pCount;
		pCount.bIncrease = false;
		db_clientdesc->DBPacket(HEADER_GD_UPDATE_OFFLINESHOP_COUNT, 0, &pCount, sizeof(pCount));
	}
}

void COfflineShopManager::AddItem(LPCHARACTER ch, BYTE bDisplayPos, TItemPos bPos, int iPrice, int iPrice2)
{
	if (!ch)
		return;

	if (bDisplayPos >= OFFLINE_SHOP_HOST_ITEM_MAX_NUM)
	{
		sys_err("Overflow offline shop slot count [%s]", ch->GetName());
		return;
	}
	
	{
		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT mapIndex,channel FROM %soffline_shop_npc WHERE owner_id = %u", get_table_postfix(), ch->GetPlayerID()));
		if (pMsg->Get()->uiNumRows == 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_OPTION_BLOCK"));
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
		
		long lMapIndex = 0;
		str_to_number(lMapIndex, row[0]);

		BYTE bChannel = 0;
		str_to_number(bChannel, row[1]);

		if (g_bChannel != bChannel)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_SAME_CHANNEL"));
			return;
		}	
	}
		
	LPITEM pkItem = ch->GetItem(bPos);
	if (!pkItem)
		return;
	
	const TItemTable * itemTable = pkItem->GetProto();
	if (IS_SET(itemTable->dwAntiFlags, ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_ITEM_NOT_SELL"));
		return;
	}
	
	if (pkItem->isLocked())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_LOCKED_ITEM"));
		return;
	}
	
#ifdef WJ_SOULBINDING_SYSTEM
	if (pkItem->IsBind() || pkItem->IsUntilBind())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_BINDEDL_ITEM"));
		return;
	}
#endif
	
	if (pkItem->IsEquipped())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_EQUIPED_ITEM"));
		return;
	}
	
	LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));
	if (!npc)
		return;

	LPOFFLINESHOP pkOfflineShop = FindOfflineShop(npc->GetVID());
	if (!pkOfflineShop)
		return;
	
	pkOfflineShop->AddItem(ch, pkItem, bDisplayPos, iPrice, iPrice2);
	pkOfflineShop->BroadcastUpdateItem(bDisplayPos, ch->GetPlayerID());
	LogManager::instance().ItemLog(ch, pkItem, "ADD ITEM OFFLINE SHOP", "");
}

void COfflineShopManager::RemoveItem(LPCHARACTER ch, BYTE bPos)
{
	if (!ch)
		return;

	if (bPos >= OFFLINE_SHOP_HOST_ITEM_MAX_NUM)
	{
		sys_log(0, "OfflineShopManager::RemoveItem - Overflow slot! [%s]", ch->GetName());
		return;
	}
	
	{
		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT mapIndex,channel FROM %soffline_shop_npc WHERE owner_id = %u", get_table_postfix(), ch->GetPlayerID()));
		if (pMsg->Get()->uiNumRows == 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_OPTION_BLOCK"));
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

		long lMapIndex = 0;
		str_to_number(lMapIndex, row[0]);

		BYTE bChannel = 0;
		str_to_number(bChannel, row[1]);

		if (g_bChannel != bChannel)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_SAME_CHANNEL"));
			return;
		}	
	}

	LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));

	if (!npc)
		return;
	
	LPOFFLINESHOP pkOfflineShop = npc->GetOfflineShop();

	if (!pkOfflineShop)
		return;
	
	pkOfflineShop->RemoveItem(ch, bPos);
}

void COfflineShopManager::Refresh(LPCHARACTER ch)
{
	if (!ch)
		return;

	LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));
	if (!npc)
		return;

	LPOFFLINESHOP pkOfflineShop = npc->GetOfflineShop();
	if (!pkOfflineShop)
		return;

	pkOfflineShop->Refresh(ch);
}

void COfflineShopManager::RefreshMoney(LPCHARACTER ch)
{
	if (!ch)
		return;
	
	LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));
	
	LPOFFLINESHOP pkOfflineShop = npc ? npc->GetOfflineShop() : NULL;
	if (!pkOfflineShop)
	{
#ifdef WJ_CHEQUE_SYSTEM
		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT shop_money,shop_cheque FROM player.player WHERE id = %u", ch->GetPlayerID()));
#else
		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT shop_money FROM player.player WHERE id = %u", ch->GetPlayerID()));
#endif
		TPacketGCShop p;
		TPacketGCOfflineShopMoney p2;

		p.header = HEADER_GC_OFFLINE_SHOP;
		p.subheader = SHOP_SUBHEADER_GC_REFRESH_MONEY;

		if (pMsg->Get()->uiNumRows == 0)
		{
			p2.llMoney = 0;
#ifdef WJ_CHEQUE_SYSTEM
			p2.dwCheque = 0;
#endif
			p.size = sizeof(p) + sizeof(p2);
			ch->GetDesc()->BufferedPacket(&p, sizeof(TPacketGCShop));
			ch->GetDesc()->Packet(&p2, sizeof(TPacketGCOfflineShopMoney));
		}
		else
		{
			MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
			str_to_number(p2.llMoney, row[0]);
#ifdef WJ_CHEQUE_SYSTEM
			str_to_number(p2.dwCheque, row[7]);
#endif
			p.size = sizeof(p) + sizeof(p2);
			ch->GetDesc()->BufferedPacket(&p, sizeof(TPacketGCShop));
			ch->GetDesc()->Packet(&p2, sizeof(TPacketGCOfflineShopMoney));
		}
	}
	else
	{
		TPacketGCShop p;
		TPacketGCOfflineShopMoney p2;

		p.header = HEADER_GC_OFFLINE_SHOP;
		p.subheader = SHOP_SUBHEADER_GC_REFRESH_MONEY;

		p2.llMoney = pkOfflineShop->GetOfflineShopGold();
#ifdef WJ_CHEQUE_SYSTEM
		p2.dwCheque = pkOfflineShop->GetOfflineShopCheque();
#endif
		p.size = sizeof(p) + sizeof(p2);
		ch->GetDesc()->BufferedPacket(&p, sizeof(TPacketGCShop));
		ch->GetDesc()->Packet(&p2, sizeof(TPacketGCOfflineShopMoney));
	}
}	

void COfflineShopManager::RefreshUnsoldItems(LPCHARACTER ch)
{
	TPacketGCShop pack;
	pack.header = HEADER_GC_OFFLINE_SHOP;
	pack.subheader = SHOP_SUBHEADER_GC_UPDATE_ITEM2;

	TPacketGCOfflineShopStart pack2;
	memset(&pack2, 0, sizeof(pack2));
	pack2.owner_vid = 0;

	char szQuery[1024];
#ifdef WJ_CHANGELOOK_SYSTEM
	snprintf(szQuery, sizeof(szQuery), "SELECT pos,count,vnum,price,price2,transmutation,socket0,socket1,socket2,socket3,socket4,socket5,attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6, applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, applytype3, applyvalue3, applytype4, applyvalue4, applytype5, applyvalue5, applytype6, applyvalue6, applytype7, applyvalue7 FROM %soffline_shop_item WHERE owner_id = %u and status = 1", get_table_postfix(), ch->GetPlayerID());
#else
	snprintf(szQuery, sizeof(szQuery), "SELECT pos,count,vnum,price,price2,socket0,socket1,socket2,socket3,socket4,socket5,attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6, applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, applytype3, applyvalue3, applytype4, applyvalue4, applytype5, applyvalue5, applytype6, applyvalue6, applytype7, applyvalue7 FROM %soffline_shop_item WHERE owner_id = %u and status = 1", get_table_postfix(), ch->GetPlayerID());
#endif

	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(szQuery));

	MYSQL_ROW row;
	while (NULL != (row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
	{
		BYTE bPos = 0;
		str_to_number(bPos, row[0]);

		str_to_number(pack2.items[bPos].count, row[1]);
		str_to_number(pack2.items[bPos].vnum, row[2]);
		str_to_number(pack2.items[bPos].price, row[3]);
		str_to_number(pack2.items[bPos].price2, row[4]);
#ifdef WJ_CHANGELOOK_SYSTEM
		str_to_number(pack2.items[bPos].transmutation, row[5]);
#endif

		DWORD alSockets[ITEM_SOCKET_MAX_NUM];
#ifdef WJ_CHANGELOOK_SYSTEM
		for (int i = 0, n = 6; i < ITEM_SOCKET_MAX_NUM; ++i, n++)
#else
		for (int i = 0, n = 7; i < ITEM_SOCKET_MAX_NUM; ++i, n++)
#endif
			str_to_number(alSockets[i], row[n]);

		TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
#ifdef WJ_CHANGELOOK_SYSTEM
		for (int i = 0, iStartType = 12, iStartValue = 13; i < ITEM_ATTRIBUTE_MAX_NUM; ++i, iStartType += 2, iStartValue += 2)
#else
		for (int i = 0, iStartType = 11, iStartValue = 12; i < ITEM_ATTRIBUTE_MAX_NUM; ++i, iStartType += 2, iStartValue += 2)
#endif
		{
			str_to_number(aAttr[i].bType, row[iStartType]);
			str_to_number(aAttr[i].sValue, row[iStartValue]);
		}

		thecore_memcpy(pack2.items[bPos].alSockets, alSockets, sizeof(pack2.items[bPos].alSockets));
		thecore_memcpy(pack2.items[bPos].aAttr, aAttr, sizeof(pack2.items[bPos].aAttr));
	}

	pack.size = sizeof(pack) + sizeof(pack2);
	ch->GetDesc()->BufferedPacket(&pack, sizeof(TPacketGCShop));
	ch->GetDesc()->Packet(&pack2, sizeof(TPacketGCOfflineShopStart));
}

void COfflineShopManager::TakeItem(LPCHARACTER ch, BYTE bPos)
{
	if (bPos >= OFFLINE_SHOP_HOST_ITEM_MAX_NUM)
		return;

	if (ch->GetOfflineShop() || ch->GetOfflineShopOwner())
		return;

	char szQuery[1024];
#ifdef WJ_CHANGELOOK_SYSTEM
	snprintf(szQuery, sizeof(szQuery), "SELECT id,pos,count,vnum,socket0,socket1,socket2,socket3,socket4,socket5,transmutation,attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6, applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, applytype3, applyvalue3, applytype4, applyvalue4, applytype5, applyvalue5, applytype6, applyvalue6, applytype7, applyvalue7 FROM %soffline_shop_item WHERE owner_id = %u and pos = %d and status = 1", get_table_postfix(), ch->GetPlayerID(), bPos);
#else
	snprintf(szQuery, sizeof(szQuery), "SELECT id,pos,count,vnum,socket0,socket1,socket2,socket4,socket4,socket5,attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6, applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, applytype3, applyvalue3, applytype4, applyvalue4, applytype5, applyvalue5, applytype6, applyvalue6, applytype7, applyvalue7 FROM %soffline_shop_item WHERE owner_id = %u and pos = %d and status = 1", get_table_postfix(), ch->GetPlayerID(), bPos);
#endif

	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(szQuery));
	if (pMsg->Get()->uiNumRows == 0)
	{
		sys_log(0, "OfflineShopManager::TakeItem - This slot is empty! [%s]", ch->GetName());
		return;
	}

	TPlayerItem item;
	int rows;
	if (!(rows = mysql_num_rows(pMsg->Get()->pSQLResult)))
		return;

	for (int i = 0; i < rows; ++i)
	{
		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
		int cur = 0;

		str_to_number(item.id, row[cur++]);
		str_to_number(item.pos, row[cur++]);
		str_to_number(item.count, row[cur++]);
		str_to_number(item.vnum, row[cur++]);
		str_to_number(item.alSockets[0], row[cur++]);
		str_to_number(item.alSockets[1], row[cur++]);
		str_to_number(item.alSockets[2], row[cur++]);
		str_to_number(item.alSockets[3], row[cur++]);
		str_to_number(item.alSockets[4], row[cur++]);
		str_to_number(item.alSockets[5], row[cur++]);
#ifdef WJ_CHANGELOOK_SYSTEM
		str_to_number(item.transmutation, row[cur++]);
#endif

		for (int j = 0; j < ITEM_ATTRIBUTE_MAX_NUM; j++)
		{
			str_to_number(item.aAttr[j].bType, row[cur++]);
			str_to_number(item.aAttr[j].sValue, row[cur++]);
		}
	}

	LPITEM pItem = ITEM_MANAGER::instance().CreateItem(item.vnum, item.count);
	if (!pItem)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_ERROR"));
		return;
	}

	pItem->SetID(item.id);
	pItem->SetAttributes(item.aAttr);
	pItem->SetSockets(item.alSockets);
#ifdef WJ_CHANGELOOK_SYSTEM
	pItem->SetTransmutation(item.transmutation);
#endif

	int iEmptyPos = 0;
	if (pItem->IsDragonSoul())
		iEmptyPos = ch->GetEmptyDragonSoulInventory(pItem);
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	else if (pItem->IsSkillBook())
		iEmptyPos = ch->GetEmptySkillBookInventory(pItem->GetSize());
	else if (pItem->IsUpgradeItem())
		iEmptyPos = ch->GetEmptyUpgradeItemsInventory(pItem->GetSize());
#endif
	else
		iEmptyPos = ch->GetEmptyInventory(pItem->GetSize());

	if (iEmptyPos < 0)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_NOUT_ENOUGH_SPACE"));
		return;
	}

	if (pItem->IsDragonSoul())
		pItem->AddToCharacter(ch, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyPos));
#ifdef WJ_SPLIT_INVENTORY_SYSTEM
	else if (pItem->IsSkillBook())
		pItem->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
	else if (pItem->IsUpgradeItem())
		pItem->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
#endif
	else
		pItem->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));

	DBManager::instance().DirectQuery("DELETE FROM %soffline_shop_item WHERE owner_id = %u and pos = %d and status = 1", get_table_postfix(), ch->GetPlayerID(), bPos);
	LogManager::instance().ItemLog(ch, pItem, "TAKE OFFLINE SHOP ITEM", "");
	// ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_ITEM_TAKED"), pItem->GetName());
}

DWORD COfflineShopManager::FindMyOfflineShop(DWORD dwPID)
{
	TOfflineShopMap::iterator it = m_Map_pkOfflineShopByNPC2.find(dwPID);
	if (m_Map_pkOfflineShopByNPC2.end() == it)
		return 0;
	
	return it->second;
}

void COfflineShopManager::ChangeOfflineShopTime(LPCHARACTER ch, int bTime)
{
	if (!ch)
		return;

	DWORD dwOfflineShopVID = FindMyOfflineShop(ch->GetPlayerID());
	if (!dwOfflineShopVID)
		return;

	LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));
	if (npc)
	{		
		int iTime = bTime*60;
		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("UPDATE %soffline_shop_npc SET time = time + %d WHERE owner_id = %u", get_table_postfix(), iTime, ch->GetPlayerID()));
		std::unique_ptr<SQLMsg> pMsg2(DBManager::instance().DirectQuery("UPDATE player.player SET offshop_time = offshop_time + %d WHERE owner_id = %u", iTime, ch->GetPlayerID()));
		npc->StopOfflineShopUpdateEvent();
		npc->SetOfflineShopTimer(iTime);
		npc->StartOfflineShopUpdateEvent();
		LogManager::instance().CharLog(ch, 0, "OFFLINE SHOP", "CHANGE TIME");
		// ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_TIME_CHANGED"));
	}
	else
	{
		TPacketGGChangeOfflineShopTime p;
		p.bHeader = HEADER_GG_CHANGE_OFFLINE_SHOP_TIME;
		// p.bTime = bTime;		
		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT mapIndex FROM %soffline_shop_npc WHERE owner_id = %u", get_table_postfix(), ch->GetPlayerID()));
		if (pMsg->Get()->uiNumRows == 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_OPTION_BLOCK"));
			return;
		}
		
		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
		str_to_number(p.lMapIndex, row[0]);		
		p.dwOwnerPID = ch->GetPlayerID();
		P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGChangeOfflineShopTime));
		// ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_TIME_CHANGED"));
	}
}

void COfflineShopManager::StopShopping(LPCHARACTER ch)
{
	LPOFFLINESHOP pkOfflineShop;
	if (!(pkOfflineShop = ch->GetOfflineShop()))
		return;

	pkOfflineShop->RemoveGuest(ch);
	sys_log(0, "OFFLINE_SHOP: END: %s", ch->GetName());

	TPacketGCShop pack;
	pack.header = HEADER_GC_OFFLINE_SHOP;
	pack.subheader = SHOP_SUBHEADER_GC_UPDATE_ITEM2;

	TPacketGCOfflineShopStart pack2;
	memset(&pack2, 0, sizeof(pack2));
	pack2.owner_vid = 0;
	
	for (BYTE i = 0; i < OFFLINE_SHOP_HOST_ITEM_MAX_NUM; ++i)
		pack2.items[i].vnum = 0;

	pack.size = sizeof(pack) + sizeof(pack2);
	ch->GetDesc()->BufferedPacket(&pack, sizeof(TPacketGCShop));
	ch->GetDesc()->Packet(&pack2, sizeof(TPacketGCOfflineShopStart));
}

void COfflineShopManager::Buy(LPCHARACTER ch, BYTE pos)
{
	if (!ch->GetOfflineShop() || !ch->GetOfflineShopOwner())
		return;

	if (DISTANCE_APPROX(ch->GetX() - ch->GetOfflineShopOwner()->GetX(), ch->GetY() - ch->GetOfflineShopOwner()->GetY()) > 1500)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_DISTANCE"));
		return;
	}
	
	LPOFFLINESHOP pkOfflineShop = ch->GetOfflineShop();
	if (!pkOfflineShop)
		return;

	int ret = pkOfflineShop->Buy(ch, pos);
	
	if (SHOP_SUBHEADER_GC_OK != ret)
	{
		TPacketGCShop pack;
		pack.header = HEADER_GC_OFFLINE_SHOP;
		pack.subheader	= ret;
		pack.size	= sizeof(TPacketGCShop);

		ch->GetDesc()->Packet(&pack, sizeof(pack));
	}
}

void COfflineShopManager::WithdrawMoney(LPCHARACTER ch, DWORD llRequiredMoney)
{
	if (!ch)
		return;

	if (llRequiredMoney < 0)
		return;

	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT shop_money FROM player.player WHERE id = %u", ch->GetPlayerID()));
	if (pMsg->Get()->uiNumRows == 0)
		return;

	int llCurrentMoney = 0;
	MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
	str_to_number(llCurrentMoney, row[0]);

	if (llRequiredMoney > llCurrentMoney)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_PRICE_BIG"));
		return;
	}

	bool isOverFlow = ch->GetGold() + llRequiredMoney > GOLD_MAX - 1 ? true : false;
	if (isOverFlow)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_PRICE_ERROR"));
		return;
	}
	
	LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));
	LPOFFLINESHOP pkOfflineShop = npc ? npc->GetOfflineShop() : NULL;
	if (pkOfflineShop)
		pkOfflineShop->SetOfflineShopGold(pkOfflineShop->GetOfflineShopGold()-llRequiredMoney);
	
	ch->PointChange(POINT_GOLD, llRequiredMoney, false);
	// ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_YANG_TAKED"), llRequiredMoney);
	DBManager::instance().DirectQuery("UPDATE player.player SET shop_money = shop_money - %d WHERE id = %u", llRequiredMoney, ch->GetPlayerID());
	LogManager::instance().CharLog(ch, 0, "OFFLINE SHOP", "WITHDRAW MONEY");
}

#ifdef WJ_CHEQUE_SYSTEM
void COfflineShopManager::WithdrawCheque(LPCHARACTER ch, DWORD dwRequiredCheque)
{
	if (!ch)
		return;	
	
	if (dwRequiredCheque < 0)
		return;
	
	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT shop_cheque FROM player.player WHERE id = %u", ch->GetPlayerID()));
	if (pMsg->Get()->uiNumRows == 0)
		return;
	
	DWORD dwCurrentCheque = 0;
	MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
	str_to_number(dwCurrentCheque, row[0]);

	if (dwRequiredCheque > dwCurrentCheque)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_PRICE_BIG"));
		return;
	}
	
	bool isOverFlow = ch->GetCheque() + dwRequiredCheque > CHEQUE_MAX - 1 ? true : false;
	if (isOverFlow)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_PRICE_ERROR"));
		return;
	}
	
	LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));
	LPOFFLINESHOP pkOfflineShop = npc ? npc->GetOfflineShop() : NULL;
	if (pkOfflineShop)
		pkOfflineShop->SetOfflineShopCheque(pkOfflineShop->GetOfflineShopCheque()-dwRequiredCheque);
	
	ch->PointChange(POINT_CHEQUE, dwRequiredCheque, false);
	// ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OFFLINE_SHOP_CHEQUE_TAKED"), dwRequiredCheque);
	DBManager::instance().DirectQuery("UPDATE player.player SET shop_cheque = shop_cheque - %d WHERE id = %u", dwRequiredCheque, ch->GetPlayerID());
	LogManager::instance().CharLog(ch, 0, "OFFLINE SHOP", "WITHDRAW CHEQUE");
}
#endif

BYTE COfflineShopManager::LeftItemCount(LPCHARACTER ch)
{
	if (!ch)
		return -1;
	
	LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));
	if (!npc)
		return -1;
	
	LPOFFLINESHOP pkOfflineShop = npc->GetOfflineShop();
	if (!pkOfflineShop)
		return -1;
	
	BYTE ret = pkOfflineShop->GetLeftItemCount();
	return ret; 
}

long COfflineShopManager::GetMapIndex(LPCHARACTER ch)
{
	if (!ch)
		return -1;
	
	LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));
	if (!npc)
		return -1;
	
	LPOFFLINESHOP pkOfflineShop = npc->GetOfflineShop();
	if (!pkOfflineShop)
		return -1;

	long ret = pkOfflineShop->GetOfflineShopMapIndex();
	return ret;
}

int COfflineShopManager::GetLeftTime(LPCHARACTER ch)
{
	if (!ch)
		return -1;
	
	LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(FindMyOfflineShop(ch->GetPlayerID()));
	if (!npc)
		return -1;
	
	LPOFFLINESHOP pkOfflineShop = npc->GetOfflineShop();
	if (!pkOfflineShop)
		return -1;

	int ret = pkOfflineShop->GetOfflineShopTime();
	return ret;
}

bool COfflineShopManager::HasOfflineShop(LPCHARACTER ch)
{
	BYTE bHasOfflineShop = 0;
	TPacketGCShop p;
	p.header = HEADER_GC_OFFLINE_SHOP;
	p.subheader = SHOP_SUBHEADER_GC_CHECK_RESULT;
	p.size = sizeof(p);

	if (ch->GetOfflineShopVID())
	{
		bHasOfflineShop = 1;
		if (ch->GetDesc())
		{
			ch->GetDesc()->Packet(&p, sizeof(p));
			ch->GetDesc()->Packet(&bHasOfflineShop, sizeof(BYTE));
		}
		return true;
	}
	else
	{
		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT owner_id FROM %soffline_shop_npc WHERE owner_id = %u", get_table_postfix(), ch->GetPlayerID()));
		if (pMsg->Get()->uiNumRows == 0)
		{
			if (ch->GetDesc())
			{
				ch->GetDesc()->Packet(&p, sizeof(p));
				ch->GetDesc()->Packet(&bHasOfflineShop, sizeof(BYTE));
			}
			return false;
		}
		else
		{
			bHasOfflineShop = 1;
			if (ch->GetDesc())
			{
				ch->GetDesc()->Packet(&p, sizeof(p));
				ch->GetDesc()->Packet(&bHasOfflineShop, sizeof(BYTE));
			}
			return true;
		}
	}
	return false;
}

LPOFFLINESHOP COfflineShopManager::FindShop(DWORD dwVID)
{
	TShopMap::iterator it = m_map_pkOfflineShopByNPC.find(dwVID);

	if (it == m_map_pkOfflineShopByNPC.end())
		return NULL;

	return it->second;
}

bool COfflineShopManager::AddGuest(LPCHARACTER ch, DWORD dwVID)
{
	LPOFFLINESHOP pOfflineShop = FindShop(dwVID);
	LPCHARACTER npc = pOfflineShop->IsOfflineShopNPC() ? pOfflineShop->GetOfflineShopNPC() : 0;
	if (!npc)
		return false;
	npc->SetOfflineShopOwner(ch);
	return pOfflineShop->AddGuest(ch, npc);
}

LPCHARACTER COfflineShopManager::GetOfflineShopNPC(DWORD dwVID)
{
	LPOFFLINESHOP pOfflineShop = FindShop(dwVID);
	if (pOfflineShop)
		return pOfflineShop->IsOfflineShopNPC() ? pOfflineShop->GetOfflineShopNPC() : 0;
	else
		return 0;
}

