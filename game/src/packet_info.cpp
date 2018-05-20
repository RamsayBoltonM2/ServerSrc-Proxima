#include "stdafx.h"
#include "../../common/stl.h"
#include "constants.h"
#include "packet_info.h"

CPacketInfo::CPacketInfo()
	: m_pCurrentPacket(NULL), m_dwStartTime(0)
{
}

CPacketInfo::~CPacketInfo()
{
	itertype(m_pPacketMap) it = m_pPacketMap.begin();
	for ( ; it != m_pPacketMap.end(); ++it) {
		M2_DELETE(it->second);
	}
}

void CPacketInfo::Set(int header, int iSize, const char * c_pszName, bool bSeq)
{
	if (m_pPacketMap.find(header) != m_pPacketMap.end())
		return;

	TPacketElement * element = M2_NEW TPacketElement;

	element->iSize = iSize;
	element->stName.assign(c_pszName);
	element->iCalled = 0;
	element->dwLoad = 0;

	element->bSequencePacket = bSeq;

	if (element->bSequencePacket)
		element->iSize += sizeof(BYTE);

	m_pPacketMap.insert(std::map<int, TPacketElement *>::value_type(header, element));
}

bool CPacketInfo::Get(int header, int * size, const char ** c_ppszName)
{
	std::map<int, TPacketElement *>::iterator it = m_pPacketMap.find(header);

	if (it == m_pPacketMap.end())
		return false;

	*size = it->second->iSize;
	*c_ppszName = it->second->stName.c_str();

	m_pCurrentPacket = it->second;
	return true;
}

bool CPacketInfo::IsSequence(int header)
{
	TPacketElement * pkElement = GetElement(header);
	return pkElement ? pkElement->bSequencePacket : false;
}

void CPacketInfo::SetSequence(int header, bool bSeq)
{
	TPacketElement * pkElem = GetElement(header);

	if (pkElem)
	{
		if (bSeq)
		{
			if (!pkElem->bSequencePacket)
				pkElem->iSize++;
		}
		else
		{
			if (pkElem->bSequencePacket)
				pkElem->iSize--;
		}

		pkElem->bSequencePacket = bSeq;
	}
}

TPacketElement * CPacketInfo::GetElement(int header)
{
	std::map<int, TPacketElement *>::iterator it = m_pPacketMap.find(header);

	if (it == m_pPacketMap.end())
		return NULL;

	return it->second;
}

void CPacketInfo::Start()
{
	assert(m_pCurrentPacket != NULL);
	m_dwStartTime = get_dword_time();
}

void CPacketInfo::End()
{
	++m_pCurrentPacket->iCalled;
	m_pCurrentPacket->dwLoad += get_dword_time() - m_dwStartTime;
}

void CPacketInfo::Log(const char * c_pszFileName)
{
	FILE * fp;

	fp = fopen(c_pszFileName, "w");

	if (!fp)
		return;

	std::map<int, TPacketElement *>::iterator it = m_pPacketMap.begin();

	fprintf(fp, "Name             Called     Load       Ratio\n");

	while (it != m_pPacketMap.end())
	{
		TPacketElement * p = it->second;
		++it;

		fprintf(fp, "%-16s %-10d %-10u %.2f\n",
				p->stName.c_str(),
				p->iCalled,
				p->dwLoad,
				p->iCalled != 0 ? (float) p->dwLoad / p->iCalled : 0.0f);
	}

	fclose(fp);
}
///---------------------------------------------------------

CPacketInfoCG::CPacketInfoCG()
{
	Set(HEADER_CG_TEXT,					sizeof(TPacketCGText),				"Text",					false);
	Set(HEADER_CG_HANDSHAKE,			sizeof(TPacketCGHandshake),			"Handshake",			false);
	Set(HEADER_CG_TIME_SYNC,			sizeof(TPacketCGHandshake),			"TimeSync",				false);
	Set(HEADER_CG_MARK_LOGIN,			sizeof(TPacketCGMarkLogin),			"MarkLogin",			false);
	Set(HEADER_CG_MARK_IDXLIST,			sizeof(TPacketCGMarkIDXList),		"MarkIdxList",			false);
	Set(HEADER_CG_MARK_CRCLIST,			sizeof(TPacketCGMarkCRCList),		"MarkCrcList",			false);
	Set(HEADER_CG_MARK_UPLOAD,			sizeof(TPacketCGMarkUpload),		"MarkUpload",			false);
#ifdef _IMPROVED_PACKET_ENCRYPTION_
	Set(HEADER_CG_KEY_AGREEMENT,		sizeof(TPacketKeyAgreement),		"KeyAgreement",			false);
#endif
	Set(HEADER_CG_GUILD_SYMBOL_UPLOAD,	sizeof(TPacketCGGuildSymbolUpload),	"SymbolUpload", 		false);
	Set(HEADER_CG_SYMBOL_CRC,			sizeof(TPacketCGSymbolCRC),			"SymbolCRC", 			false);
	Set(HEADER_CG_LOGIN,				sizeof(TPacketCGLogin),				"Login", 				false);
	Set(HEADER_CG_LOGIN2,				sizeof(TPacketCGLogin2),			"Login2", 				false);
	Set(HEADER_CG_LOGIN3,				sizeof(TPacketCGLogin3),			"Login3", 				false);
	Set(HEADER_CG_LOGIN5_OPENID,		sizeof(TPacketCGLogin5),			"Login5", 				false);	//OpenID
	Set(HEADER_CG_ATTACK,				sizeof(TPacketCGAttack),			"Attack", 				false);
	Set(HEADER_CG_CHAT,					sizeof(TPacketCGChat),				"Chat",					false);
	Set(HEADER_CG_WHISPER,				sizeof(TPacketCGWhisper),			"Whisper",				false);
	Set(HEADER_CG_CHARACTER_SELECT,		sizeof(TPacketCGPlayerSelect),		"Select",				false);
	Set(HEADER_CG_CHARACTER_CREATE,		sizeof(TPacketCGPlayerCreate),		"Create", 				false);
	Set(HEADER_CG_CHARACTER_DELETE,		sizeof(TPacketCGPlayerDelete),		"Delete", 				false);
	Set(HEADER_CG_ENTERGAME,			sizeof(TPacketCGEnterGame),			"EnterGame",			false);
	Set(HEADER_CG_ITEM_USE,				sizeof(TPacketCGItemUse),			"ItemUse", 				false);
	Set(HEADER_CG_ITEM_DROP,			sizeof(TPacketCGItemDrop),			"ItemDrop",				false);
	Set(HEADER_CG_ITEM_DROP2,			sizeof(TPacketCGItemDrop2),			"ItemDrop2", 			false);
#ifdef WJ_NEW_DROP_DIALOG
	Set(HEADER_CG_ITEM_DESTROY,			sizeof(TPacketCGItemDestroy),		"ItemDestroy", 			false);
	Set(HEADER_CG_ITEM_SELL,			sizeof(TPacketCGItemSell),			"ItemSell", 			false);
#endif
	Set(HEADER_CG_ITEM_MOVE,			sizeof(TPacketCGItemMove),			"ItemMove", 			false);
	Set(HEADER_CG_ITEM_PICKUP,			sizeof(TPacketCGItemPickup),		"ItemPickup", 			false);
	Set(HEADER_CG_QUICKSLOT_ADD,		sizeof(TPacketCGQuickslotAdd),		"QuickslotAdd",			false);
	Set(HEADER_CG_QUICKSLOT_DEL,		sizeof(TPacketCGQuickslotDel),		"QuickslotDel", 		false);
	Set(HEADER_CG_QUICKSLOT_SWAP,		sizeof(TPacketCGQuickslotSwap),		"QuickslotSwap", 		false);
	Set(HEADER_CG_SHOP,					sizeof(TPacketCGShop),				"Shop", 				false);
	Set(HEADER_CG_ON_CLICK,				sizeof(TPacketCGOnClick),			"OnClick", 				false);
	Set(HEADER_CG_EXCHANGE,				sizeof(TPacketCGExchange),			"Exchange", 			false);
	Set(HEADER_CG_CHARACTER_POSITION,	sizeof(TPacketCGPosition),			"Position", 			false);
	Set(HEADER_CG_SCRIPT_ANSWER,		sizeof(TPacketCGScriptAnswer),		"ScriptAnswer", 		false);
	Set(HEADER_CG_SCRIPT_BUTTON,		sizeof(TPacketCGScriptButton),		"ScriptButton", 		false);
	Set(HEADER_CG_QUEST_INPUT_STRING,	sizeof(TPacketCGQuestInputString),	"QuestInputString",	 	false);
	Set(HEADER_CG_QUEST_CONFIRM,		sizeof(TPacketCGQuestConfirm),		"QuestConfirm", 		false);
	Set(HEADER_CG_MOVE,					sizeof(TPacketCGMove),				"Move", 				false);
	Set(HEADER_CG_SYNC_POSITION,		sizeof(TPacketCGSyncPosition),		"SyncPosition", 		false);
	Set(HEADER_CG_FLY_TARGETING,		sizeof(TPacketCGFlyTargeting),		"FlyTarget", 			false);
	Set(HEADER_CG_ADD_FLY_TARGETING,	sizeof(TPacketCGFlyTargeting),		"AddFlyTarget",			false);
	Set(HEADER_CG_SHOOT,				sizeof(TPacketCGShoot),				"Shoot", 				false);
	Set(HEADER_CG_USE_SKILL,			sizeof(TPacketCGUseSkill),			"UseSkill", 			false);
	Set(HEADER_CG_ITEM_USE_TO_ITEM,		sizeof(TPacketCGItemUseToItem),		"UseItemToItem", 		false);
	Set(HEADER_CG_TARGET,				sizeof(TPacketCGTarget),			"Target", 				false);
	Set(HEADER_CG_WARP,					sizeof(TPacketCGWarp),				"Warp", 				false);
	Set(HEADER_CG_MESSENGER,			sizeof(TPacketCGMessenger),			"Messenger", 			false);
	Set(HEADER_CG_PARTY_REMOVE,			sizeof(TPacketCGPartyRemove),		"PartyRemove", 			false);
	Set(HEADER_CG_PARTY_INVITE,			sizeof(TPacketCGPartyInvite),		"PartyInvite", 			false);
	Set(HEADER_CG_PARTY_INVITE_ANSWER,	sizeof(TPacketCGPartyInviteAnswer), "PartyInviteAnswer",	false);
	Set(HEADER_CG_PARTY_SET_STATE,		sizeof(TPacketCGPartySetState),		"PartySetState", 		false);
	Set(HEADER_CG_PARTY_USE_SKILL,		sizeof(TPacketCGPartyUseSkill),		"PartyUseSkill", 		false);
	Set(HEADER_CG_PARTY_PARAMETER,		sizeof(TPacketCGPartyParameter),	"PartyParam", 			false);
	Set(HEADER_CG_EMPIRE,				sizeof(TPacketCGEmpire),			"Empire", 				false);
	Set(HEADER_CG_SAFEBOX_CHECKOUT,		sizeof(TPacketCGSafeboxCheckout),	"SafeboxCheckout", 		false);
	Set(HEADER_CG_SAFEBOX_CHECKIN,		sizeof(TPacketCGSafeboxCheckin),	"SafeboxCheckin", 		false);
	Set(HEADER_CG_SAFEBOX_ITEM_MOVE,	sizeof(TPacketCGItemMove),			"SafeboxItemMove",		false);
	Set(HEADER_CG_GUILD,				sizeof(TPacketCGGuild),				"Guild", 				false);
	Set(HEADER_CG_ANSWER_MAKE_GUILD,	sizeof(TPacketCGAnswerMakeGuild),	"AnswerMakeGuild",		false);
	Set(HEADER_CG_FISHING,				sizeof(TPacketCGFishing),			"Fishing",				false);
	Set(HEADER_CG_ITEM_GIVE,			sizeof(TPacketCGGiveItem),			"ItemGive",				false);
	Set(HEADER_CG_HACK,					sizeof(TPacketCGHack),				"Hack",					false);
	Set(HEADER_CG_MYSHOP,				sizeof(TPacketCGMyShop),			"MyShop",				false);
	Set(HEADER_CG_REFINE,				sizeof(TPacketCGRefine),			"Refine",				false);
	Set(HEADER_CG_CHANGE_NAME,			sizeof(TPacketCGChangeName),		"ChangeName",			false);
	Set(HEADER_CG_CLIENT_VERSION,		sizeof(TPacketCGClientVersion),		"Version",				false);
	Set(HEADER_CG_CLIENT_VERSION2,		sizeof(TPacketCGClientVersion2),	"Version",				false);
	Set(HEADER_CG_PONG,					sizeof(BYTE), 						"Pong",					false);
	Set(HEADER_CG_MALL_CHECKOUT,		sizeof(TPacketCGSafeboxCheckout),	"MallCheckout",			false);
	Set(HEADER_CG_SCRIPT_SELECT_ITEM,	sizeof(TPacketCGScriptSelectItem),	"ScriptSelectItem",		false);
	Set(HEADER_CG_PASSPOD_ANSWER,		sizeof(TPacketCGPasspod),			"PasspodAnswer",		false);
	Set(HEADER_CG_DRAGON_SOUL_REFINE,	sizeof(TPacketCGDragonSoulRefine),	"DragonSoulRefine",		false);
	Set(HEADER_CG_STATE_CHECKER,		sizeof(BYTE),						"ServerStateCheck",		false);
#ifdef WJ_OFFLINESHOP_SYSTEM
	Set(HEADER_CG_OFFLINE_SHOP,			sizeof(TPacketCGShop),				"OfflineShop",			false);
	Set(HEADER_CG_MY_OFFLINE_SHOP,		sizeof(TPacketCGMyOfflineShop),		"MyOfflineShop",		false);
#endif
#ifdef WJ_QUEST_RECEIVE_SYSTEM
	Set(HEADER_CG_QUEST_RECEIVE, 		sizeof(TPacketCGQuestRcv), 			"QuestReceive", 		false);
#endif
#ifdef WJ_CHANNEL_CHANGE_SYSTEM
	Set(HEADER_CG_CHANNEL_CHANGE, 		sizeof(TPacketCGChannelChange), 	"ChannelChange", 		false);
#endif
#ifdef WJ_GROWTH_PET_SYSTEM
	Set(HEADER_CG_PetSetName, 			sizeof(TPacketCGRequestPetName), 	"BraveRequestPetName", 	false);
#endif
#ifdef WJ_CHANGELOOK_SYSTEM
	Set(HEADER_CG_CL, 					sizeof(TPacketChangeLook), 			"ChangeLook", 			false);
#endif
#ifdef WJ_SHOP_SEARCH_SYSTEM
	Set(HEADER_CG_SHOP_SEARCH, 			sizeof(TPacketCGShopSearch), 		"ShopSearch", 			false);
	Set(HEADER_CG_SHOP_SEARCH_SUB, 		sizeof(TPacketCGShopSearch), 		"ShopSearchSub", 		false);
	Set(HEADER_CG_SHOP_SEARCH_BUY, 		sizeof(TPacketCGShopSearchBuy), 	"ShopSearchBuy", 		false);
#endif
#ifdef WJ_ACCE_SYSTEM
	Set(HEADER_CG_ACCE,					sizeof(TPacketCGAcce), 				"Acce", 				false);
#endif
}

CPacketInfoCG::~CPacketInfoCG()
{
	Log("packet_info.txt");
}

////////////////////////////////////////////////////////////////////////////////
CPacketInfoGG::CPacketInfoGG()
{
	Set(HEADER_GG_SETUP,					sizeof(TPacketGGSetup),					"Setup",				false);
	Set(HEADER_GG_LOGIN,					sizeof(TPacketGGLogin),					"Login",				false);
	Set(HEADER_GG_LOGOUT,					sizeof(TPacketGGLogout),				"Logout",				false);
	Set(HEADER_GG_RELAY,					sizeof(TPacketGGRelay),					"Relay",				false);
	Set(HEADER_GG_NOTICE,					sizeof(TPacketGGNotice),				"Notice",				false);
#ifdef ENABLE_FULL_NOTICE
	Set(HEADER_GG_BIG_NOTICE,				sizeof(TPacketGGNotice),				"BigNotice",			false);
#endif
	Set(HEADER_GG_SHUTDOWN,					sizeof(TPacketGGShutdown),				"Shutdown",				false);
	Set(HEADER_GG_GUILD,					sizeof(TPacketGGGuild),					"Guild",				false);
	Set(HEADER_GG_SHOUT,					sizeof(TPacketGGShout),					"Shout",				false);
	Set(HEADER_GG_DISCONNECT,	    		sizeof(TPacketGGDisconnect),			"Disconnect",			false);
	Set(HEADER_GG_MESSENGER_ADD,			sizeof(TPacketGGMessenger),				"MessengerAdd",			false);
	Set(HEADER_GG_MESSENGER_REMOVE,			sizeof(TPacketGGMessenger),				"MessengerRemove",		false);
	Set(HEADER_GG_FIND_POSITION,			sizeof(TPacketGGFindPosition),			"FindPosition",			false);
	Set(HEADER_GG_WARP_CHARACTER,			sizeof(TPacketGGWarpCharacter),			"WarpCharacter",		false);
	Set(HEADER_GG_MESSENGER_MOBILE,			sizeof(TPacketGGMessengerMobile),		"MessengerMobile",		false);
	Set(HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX, sizeof(TPacketGGGuildWarMapIndex), 		"GuildWarMapIndex",		false);
	Set(HEADER_GG_TRANSFER,					sizeof(TPacketGGTransfer),				"Transfer",				false);
	Set(HEADER_GG_XMAS_WARP_SANTA,			sizeof(TPacketGGXmasWarpSanta),			"XmasWarpSanta",		false);
	Set(HEADER_GG_XMAS_WARP_SANTA_REPLY,	sizeof(TPacketGGXmasWarpSantaReply),	"XmasWarpSantaReply",	false);
	Set(HEADER_GG_RELOAD_CRC_LIST,			sizeof(BYTE),							"ReloadCRCList",		false);
	Set(HEADER_GG_CHECK_CLIENT_VERSION,		sizeof(BYTE),							"CheckClientVersion",	false);
	Set(HEADER_GG_LOGIN_PING,				sizeof(TPacketGGLoginPing),				"LoginPing",			false);
	Set(HEADER_GG_BLOCK_CHAT,				sizeof(TPacketGGBlockChat),				"BlockChat",			false);
	Set(HEADER_GG_SIEGE,					sizeof(TPacketGGSiege),					"Siege",				false);
	Set(HEADER_GG_MONARCH_NOTICE,			sizeof(TPacketGGMonarchNotice),			"MonarchNotice",		false);
	Set(HEADER_GG_MONARCH_TRANSFER,			sizeof(TPacketMonarchGGTransfer),		"MonarchTransfer",		false);
	Set(HEADER_GG_PCBANG_UPDATE,			sizeof(TPacketPCBangUpdate),			"PCBangUpdate",			false);
	Set(HEADER_GG_CHECK_AWAKENESS,			sizeof(TPacketGGCheckAwakeness),		"CheckAwakeness",		false);
#ifdef WJ_OFFLINESHOP_SYSTEM
	Set(HEADER_GG_REMOVE_OFFLINE_SHOP,		sizeof(TPacketGGRemoveOfflineShop),		"RemoveOfflineShop",	false);
#endif
}

CPacketInfoGG::~CPacketInfoGG()
{
	Log("p2p_packet_info.txt");
}
