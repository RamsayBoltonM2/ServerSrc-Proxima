#include "stdafx.h"
#include "constants.h"
#include "config.h"
#include "log.h"

#include "char.h"
#include "desc.h"
#include "item.h"
#include "locale_service.h"
#ifdef WJ_DRAGON_HUNTERS_SYSTEM
#include "db.h"
#endif

static char	__escape_hint[1024];

LogManager::LogManager() : m_bIsConnect(false)
{
}

LogManager::~LogManager()
{
}

bool LogManager::Connect(const char * host, const int port, const char * user, const char * pwd, const char * db)
{
	if (m_sql.Setup(host, user, pwd, db, g_stLocale.c_str(), false, port))
		m_bIsConnect = true;

	return m_bIsConnect;
}

void LogManager::Query(const char * c_pszFormat, ...)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);

	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	if (test_server)
		sys_log(0, "LOG: %s", szQuery);

	m_sql.AsyncQuery(szQuery);
}

SQLMsg * LogManager::DirectQuery(const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	if (test_server)
		sys_log(1, "LOG: %s", szQuery);

	return m_sql.DirectQuery(szQuery);
}

bool LogManager::IsConnected()
{
	return m_bIsConnect;
}

size_t LogManager::EscapeString(char* dst, size_t dstSize, const char *src, size_t srcSize)
{
	return m_sql.EscapeString(dst, dstSize, src, srcSize);
}

void LogManager::ItemLog(DWORD dwPID, DWORD x, DWORD y, DWORD dwItemID, const char * c_pszText, const char * c_pszHint, const char * c_pszIP, DWORD dwVnum)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), c_pszHint, strlen(c_pszHint));

	Query("INSERT DELAYED INTO log%s (type, time, who, x, y, what, how, hint, ip, vnum) VALUES('ITEM', NOW(), %u, %u, %u, %u, '%s', '%s', '%s', %u)",
			get_table_postfix(), dwPID, x, y, dwItemID, c_pszText, __escape_hint, c_pszIP, dwVnum);
}

void LogManager::ItemLog(LPCHARACTER ch, LPITEM item, const char * c_pszText, const char * c_pszHint)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	if (NULL == ch || NULL == item)
	{
		sys_err("character or item nil (ch %p item %p text %s)", get_pointer(ch), get_pointer(item), c_pszText);
		return;
	}

	ItemLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(), item->GetID(),
			NULL == c_pszText ? "" : c_pszText,
		   	c_pszHint, ch->GetDesc() ? ch->GetDesc()->GetHostName() : "",
		   	item->GetOriginalVnum());
}

void LogManager::ItemLog(LPCHARACTER ch, int itemID, int itemVnum, const char * c_pszText, const char * c_pszHint)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	ItemLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(), itemID, c_pszText, c_pszHint, ch->GetDesc() ? ch->GetDesc()->GetHostName() : "", itemVnum);
}

void LogManager::CharLog(DWORD dwPID, DWORD x, DWORD y, DWORD dwValue, const char * c_pszText, const char * c_pszHint, const char * c_pszIP)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), c_pszHint, strlen(c_pszHint));

	Query("INSERT DELAYED INTO log%s (type, time, who, x, y, what, how, hint, ip) VALUES('CHARACTER', NOW(), %u, %u, %u, %u, '%s', '%s', '%s')",
			get_table_postfix(), dwPID, x, y, dwValue, c_pszText, __escape_hint, c_pszIP);
}

void LogManager::CharLog(LPCHARACTER ch, DWORD dw, const char * c_pszText, const char * c_pszHint)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	if (ch)
		CharLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(), dw, c_pszText, c_pszHint, ch->GetDesc() ? ch->GetDesc()->GetHostName() : "");
	else
		CharLog(0, 0, 0, dw, c_pszText, c_pszHint, "");
}

void LogManager::LoginLog(bool isLogin, DWORD dwAccountID, DWORD dwPID, BYTE bLevel, BYTE bJob, DWORD dwPlayTime)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	Query("INSERT DELAYED INTO loginlog%s (type, time, channel, account_id, pid, level, job, playtime) VALUES (%s, NOW(), %d, %u, %u, %d, %d, %u)",
			get_table_postfix(), isLogin ? "'LOGIN'" : "'LOGOUT'", g_bChannel, dwAccountID, dwPID, bLevel, bJob, dwPlayTime);
}

void LogManager::MoneyLog(BYTE type, DWORD vnum, int gold)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MAX);
	if (type == MONEY_LOG_RESERVED || type >= MONEY_LOG_TYPE_MAX_NUM)
	{
		sys_err("TYPE ERROR: type %d vnum %u gold %d", type, vnum, gold);
		return;
	}

	Query("INSERT DELAYED INTO money_log%s VALUES (NOW(), %d, %d, %d)", get_table_postfix(), type, vnum, gold);
}

#ifdef WJ_CHEQUE_SYSTEM
void LogManager::ChequeLog(BYTE type, DWORD vnum, int cheque)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MAX);
	if (type == CHEQUE_LOG_RESERVED || type >= CHEQUE_LOG_TYPE_MAX_NUM)
	{
		sys_err("TYPE ERROR: type %d vnum %u cheque %d", type, vnum, cheque);
		return;
	}
	
	Query("INSERT DELAYED INTO cheque_log%s VALUE (NOW(), %d, %d, %d)", get_table_postfix(), type, vnum, cheque);
}
#endif

void LogManager::HackLog(const char * c_pszHackName, const char * c_pszLogin, const char * c_pszName, const char * c_pszIP)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MID);
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), c_pszHackName, strlen(c_pszHackName));

	Query("INSERT INTO hack_log (time, login, name, ip, server, why) VALUES(NOW(), '%s', '%s', '%s', '%s', '%s')", c_pszLogin, c_pszName, c_pszIP, g_stHostname.c_str(), __escape_hint);
}

void LogManager::HackLog(const char * c_pszHackName, LPCHARACTER ch)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MID);
	if (ch->GetDesc())
	{
		HackLog(c_pszHackName,
				ch->GetDesc()->GetAccountTable().login,
				ch->GetName(),
				ch->GetDesc()->GetHostName());
	}
}

void LogManager::HackCRCLog(const char * c_pszHackName, const char * c_pszLogin, const char * c_pszName, const char * c_pszIP, DWORD dwCRC)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MID);
	Query("INSERT INTO hack_crc_log (time, login, name, ip, server, why, crc) VALUES(NOW(), '%s', '%s', '%s', '%s', '%s', %u)", c_pszLogin, c_pszName, c_pszIP, g_stHostname.c_str(), c_pszHackName, dwCRC);
}

void LogManager::PCBangLoginLog(DWORD dwPCBangID, const char* c_szPCBangIP, DWORD dwPlayerID, DWORD dwPlayTime)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MAX);
	Query("INSERT INTO pcbang_loginlog (time, pcbang_id, ip, pid, play_time) VALUES (NOW(), %u, '%s', %u, %u)",
			dwPCBangID, c_szPCBangIP, dwPlayerID, dwPlayTime);
}

void LogManager::GoldBarLog(DWORD dwPID, DWORD dwItemID, GOLDBAR_HOW eHow, const char* c_pszHint)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	char szHow[32+1];

	switch (eHow)
	{
		case PERSONAL_SHOP_BUY:
			snprintf(szHow, sizeof(szHow), "'BUY'");
			break;

		case PERSONAL_SHOP_SELL:
			snprintf(szHow, sizeof(szHow), "'SELL'");
			break;

		case SHOP_BUY:
			snprintf(szHow, sizeof(szHow), "'SHOP_BUY'");
			break;

		case SHOP_SELL:
			snprintf(szHow, sizeof(szHow), "'SHOP_SELL'");
			break;

		case EXCHANGE_TAKE:
			snprintf(szHow, sizeof(szHow), "'EXCHANGE_TAKE'");
			break;

		case EXCHANGE_GIVE:
			snprintf(szHow, sizeof(szHow), "'EXCHANGE_GIVE'");
			break;

		case QUEST:
			snprintf(szHow, sizeof(szHow), "'QUEST'");
			break;

		default:
			snprintf(szHow, sizeof(szHow), "''");
			break;
	}

	Query("INSERT DELAYED INTO goldlog%s (date, time, pid, what, how, hint) VALUES(CURDATE(), CURTIME(), %u, %u, %s, '%s')",
			get_table_postfix(), dwPID, dwItemID, szHow, c_pszHint);
}

void LogManager::CubeLog(DWORD dwPID, DWORD x, DWORD y, DWORD item_vnum, DWORD item_uid, int item_count, bool success)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MID);
	Query("INSERT DELAYED INTO cube%s (pid, time, x, y, item_vnum, item_uid, item_count, success) "
			"VALUES(%u, NOW(), %u, %u, %u, %u, %d, %d)",
			get_table_postfix(), dwPID, x, y, item_vnum, item_uid, item_count, success?1:0);
}

void LogManager::SpeedHackLog(DWORD pid, DWORD x, DWORD y, int hack_count)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MID);
	Query("INSERT INTO speed_hack%s (pid, time, x, y, hack_count) "
			"VALUES(%u, NOW(), %u, %u, %d)",
			get_table_postfix(), pid, x, y, hack_count);
}

void LogManager::ChangeNameLog(DWORD pid, const char *old_name, const char *new_name, const char *ip)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	Query("INSERT DELAYED INTO change_name%s (pid, old_name, new_name, time, ip) "
			"VALUES(%u, '%s', '%s', NOW(), '%s') ",
			get_table_postfix(), pid, old_name, new_name, ip);
}

void LogManager::GMCommandLog(DWORD dwPID, const char* szName, const char* szIP, BYTE byChannel, const char* szCommand)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), szCommand, strlen(szCommand));

	Query("INSERT DELAYED INTO command_log%s (userid, server, ip, port, username, command, date ) "
			"VALUES(%u, 999, '%s', %u, '%s', '%s', NOW()) ",
			get_table_postfix(), dwPID, szIP, byChannel, szName, __escape_hint);
}

void LogManager::RefineLog(DWORD pid, const char* item_name, DWORD item_id, int item_refine_level, int is_success, const char* how)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), item_name, strlen(item_name));

	Query("INSERT INTO refinelog%s (pid, item_name, item_id, step, time, is_success, setType) VALUES(%u, '%s', %u, %d, NOW(), %d, '%s')",
			get_table_postfix(), pid, __escape_hint, item_id, item_refine_level, is_success, how);
}

void LogManager::ShoutLog(BYTE bChannel, BYTE bEmpire, const char * pszText)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), pszText, strlen(pszText));

	Query("INSERT INTO shout_log%s VALUES(NOW(), %d, %d,'%s')", get_table_postfix(), bChannel, bEmpire, __escape_hint);
}

void LogManager::LevelLog(LPCHARACTER pChar, unsigned int level, unsigned int playhour)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	DWORD aid = 0;

	if (NULL != pChar->GetDesc())
	{
		aid = pChar->GetDesc()->GetAccountTable().id;
	}

	Query("REPLACE INTO levellog%s (name, level, time, account_id, pid, playtime) VALUES('%s', %u, NOW(), %u, %u, %d)",
			get_table_postfix(), pChar->GetName(), level, aid, pChar->GetPlayerID(), playhour);
}

void LogManager::BootLog(const char * c_pszHostName, BYTE bChannel)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MIN);
	Query("INSERT INTO bootlog (time, hostname, channel) VALUES(NOW(), '%s', %d)",
			c_pszHostName, bChannel);
}

void LogManager::VCardLog(DWORD vcard_id, DWORD x, DWORD y, const char * hostname, const char * giver_name, const char * giver_ip, const char * taker_name, const char * taker_ip)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MAX);
	Query("INSERT DELAYED INTO vcard_log (vcard_id, x, y, hostname, giver_name, giver_ip, taker_name, taker_ip) VALUES(%u, %u, %u, '%s', '%s', '%s', '%s', '%s')",
			vcard_id, x, y, hostname, giver_name, giver_ip, taker_name, taker_ip);
}

void LogManager::FishLog(DWORD dwPID, int prob_idx, int fish_id, int fish_level, DWORD dwMiliseconds, DWORD dwVnum, DWORD dwValue)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MAX);
	Query("INSERT INTO fish_log%s VALUES(NOW(), %u, %d, %u, %d, %u, %u, %u)",
			get_table_postfix(),
			dwPID,
			prob_idx,
			fish_id,
			fish_level,
			dwMiliseconds,
			dwVnum,
			dwValue);
}

void LogManager::QuestRewardLog(const char * c_pszQuestName, DWORD dwPID, DWORD dwLevel, int iValue1, int iValue2)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MAX);
	Query("INSERT INTO quest_reward_log%s VALUES('%s',%u,%u,2,%u,%u,NOW())",
			get_table_postfix(),
			c_pszQuestName,
			dwPID,
			dwLevel,
			iValue1,
			iValue2);
}

void LogManager::DetailLoginLog(bool isLogin, LPCHARACTER ch)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MID);
	if (NULL == ch->GetDesc())
		return;

	if (true == isLogin)
	{
		Query("INSERT INTO loginlog2(type, is_gm, login_time, channel, account_id, pid, ip, client_version) "
				"VALUES('INVALID', %s, NOW(), %d, %u, %u, inet_aton('%s'), '%s')",
				ch->IsGM() == true ? "'Y'" : "'N'",
				g_bChannel,
				ch->GetDesc()->GetAccountTable().id,
				ch->GetPlayerID(),
				ch->GetDesc()->GetHostName(),
				ch->GetDesc()->GetClientVersion());
	}
	else
	{
		Query("SET @i = (SELECT MAX(id) FROM loginlog2 WHERE account_id=%u AND pid=%u)",
				ch->GetDesc()->GetAccountTable().id,
				ch->GetPlayerID());

		Query("UPDATE loginlog2 SET type='VALID', logout_time=NOW(), playtime=TIMEDIFF(logout_time,login_time) WHERE id=@i");
	}
}

void LogManager::DragonSlayLog(DWORD dwGuildID, DWORD dwDragonVnum, DWORD dwStartTime, DWORD dwEndTime)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MAX);
	Query( "INSERT INTO dragon_slay_log%s VALUES( %d, %d, FROM_UNIXTIME(%d), FROM_UNIXTIME(%d) )",
			get_table_postfix(),
			dwGuildID, dwDragonVnum, dwStartTime, dwEndTime);
}

#ifdef WJ_DRAGON_HUNTERS_SYSTEM
void LogManager::DragonLairRankingLog(DWORD dwPlayerID, const char* szPlayerName, DWORD dwPlayerEmpire, DWORD dwKillCount)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MAX);
	std::unique_ptr<SQLMsg> pMsg(DBManager::Instance().DirectQuery("SELECT player_name, player_empire, kill_count FROM log.dragonlair_dungeon%s WHERE player_id=%u;", get_table_postfix(), dwPlayerID));
	if (pMsg->Get()->uiNumRows == 0)
		Query("INSERT INTO dragonlair_dungeon%s (player_id, player_name, player_empire, kill_count, date) VALUES(%u, '%s', %u, %u, NOW())", get_table_postfix(), dwPlayerID, szPlayerName, dwPlayerEmpire, 1);
	else
	{
		const char* szPlayerNameR;
		DWORD dwPlayerEmpireR = 0;
		DWORD dwKillCountR = 0;
		MYSQL_ROW mRow;
		while (NULL != (mRow = mysql_fetch_row(pMsg->Get()->pSQLResult)))
		{
			int iCur = 0;
			szPlayerNameR = mRow[iCur++];
			str_to_number(dwPlayerEmpireR, mRow[iCur++]);
			str_to_number(dwKillCountR, mRow[iCur++]);
		}
		
		if ((dwKillCountR > 0))
			Query("UPDATE dragonlair_dungeon%s SET player_empire=%u, kill_count=%u, date=NOW() WHERE player_id=%u;", get_table_postfix(), dwPlayerEmpire, 1+dwKillCountR, dwPlayerID);
	}
}
#endif

#ifdef ENABLE_CHAT_LOGGING
void LogManager::ChatLog(DWORD where, DWORD who_id, const char* who_name, DWORD whom_id, const char* whom_name, const char* type, const char* msg, const char* ip)
{
	LOG_LEVEL_CHECK_N_RET(LOG_LEVEL_MAX);
	Query("INSERT DELAYED INTO `chat_log%s` (`where`, `who_id`, `who_name`, `whom_id`, `type`, `msg`, `when`, `ip`) "
		"VALUES (%u, %u, '%s', %u, '%s', '%s', NOW(), '%s');",
		get_table_postfix(),
		where, who_id, who_name, whom_id, type, msg, ip);
}
#endif

#ifdef WJ_MELEY_LAIR_DUNGEON
#include "db.h"

void LogManager::MeleyLog(DWORD dwGuildID, DWORD dwPartecipants, DWORD dwTime)
{
	std::unique_ptr<SQLMsg> pMsg(DBManager::Instance().DirectQuery("SELECT partecipants, time FROM log.meley_dungeon%s WHERE guild_id=%u;", get_table_postfix(), dwGuildID));
	if (pMsg->Get()->uiNumRows == 0)
		Query("INSERT INTO meley_dungeon%s (guild_id, partecipants, time, date) VALUES(%u, %u, %u, NOW())", get_table_postfix(), dwGuildID, dwPartecipants, dwTime);
	else
	{
		DWORD dwPartecipantsR = 0, dwTimeR = 0;
		MYSQL_ROW mRow;
		while (NULL != (mRow = mysql_fetch_row(pMsg->Get()->pSQLResult)))
		{
			int iCur = 0;
			str_to_number(dwPartecipantsR, mRow[iCur++]);
			str_to_number(dwTimeR, mRow[iCur]);
		}
		
		if ((dwTimeR == dwTime) && (dwPartecipantsR < dwPartecipants))
			Query("UPDATE meley_dungeon%s SET partecipants=%u, time=%u, date=NOW() WHERE guild_id=%u;", get_table_postfix(), dwPartecipants, dwTime, dwGuildID);
		else if (dwTimeR > dwTime)
			Query("UPDATE meley_dungeon%s SET partecipants=%u, time=%u, date=NOW() WHERE guild_id=%u;", get_table_postfix(), dwPartecipants, dwTime, dwGuildID);
	}
}
#endif

#ifdef WJ_OKEY_CARD_EVENT
void LogManager::OkayEventLog(int dwPID, const char * c_pszText, int points)
{
	Query("INSERT INTO okay_event%s (pid, name, points) VALUES(%d, '%s', %d)",
			get_table_postfix(), dwPID, c_pszText, points);
			
}
#endif

#ifdef WJ_NEW_EXCHANGE_WINDOW
#ifdef WJ_CHEQUE_SYSTEM
DWORD LogManager::ExchangeLog(int type, DWORD dwPID1, DWORD dwPID2, long x, long y, int goldA /*=0*/, int goldB /*=0*/, int chequeA, int chequeB)
#else
DWORD LogManager::ExchangeLog(int type, DWORD dwPID1, DWORD dwPID2, long x, long y, int goldA /*=0*/, int goldB /*=0*/)
#endif
{
	char szQuery[512];
#ifdef WJ_CHEQUE_SYSTEM
	snprintf(szQuery, sizeof(szQuery), "INSERT INTO exchanges (type, playerA, playerB, goldA, goldB, chequeA, chequeB, x, y, date) VALUES (%d, %lu, %lu, %d, %d, %d, %d, %ld, %ld, NOW())", type, dwPID1, dwPID2, goldA, goldB, chequeA, chequeB, x, y);
#else
	snprintf(szQuery, sizeof(szQuery), "INSERT INTO exchanges (type, playerA, playerB, goldA, goldB, x, y, date) VALUES (%d, %lu, %lu, %d, %d, %ld, %ld, NOW())", type, dwPID1, dwPID2, goldA, goldB, x, y);	
#endif
	std::unique_ptr<SQLMsg> msg(DirectQuery(szQuery));

	if (!msg || msg->Get()->uiAffectedRows == 0 || msg->Get()->uiAffectedRows == (my_ulonglong)-1) {
		sys_err("Issue logging trade. Query: %s", szQuery);
		return 0;
	}

	return (DWORD)msg->Get()->uiInsertID;
}

void LogManager::ExchangeItemLog(DWORD tradeID, LPITEM item, const char * player)
{
	/*
	 *"exchange_items"
	 Structure: trade_id, toPlayer, item_id, vnum, count, socket0..5, attrtype0...6, attrvalue0...6*/

	if (!tradeID)
	{
		sys_err("Lost trade due to mysql error (tradeID = 0)");
		return;
	}

	Query("INSERT INTO exchange_items" \
		"(trade_id, `toPlayer`, `item_id`, `vnum`, count, socket0, socket1, socket2," \
		" attrtype0, attrtype1, attrtype2, attrtype3, attrtype4, attrtype5, attrtype6," \
		" attrvalue0, attrvalue1, attrvalue2, attrvalue3, attrvalue4, attrvalue5, attrvalue6,"
		" date)" \
		"VALUES ("\
		"%lu,'%s',%lu,%lu,%lu,"\
		"%ld,%ld,%ld,"\
		"%d,%d,%d,%d,"\
		"%d,%d,%d,%d,%d,"\
		"%d,%d,%d,%d,%d,"\
		"NOW())"
		, tradeID, player, item->GetID(), item->GetVnum(), item->GetCount()
		, item->GetSocket(0), item->GetSocket(1), item->GetSocket(2)
		, item->GetAttributeType(0), item->GetAttributeType(1), item->GetAttributeType(2), item->GetAttributeType(3)
		, item->GetAttributeType(4), item->GetAttributeType(5), item->GetAttributeType(6), item->GetAttributeValue(0), item->GetAttributeValue(1)
		, item->GetAttributeValue(2), item->GetAttributeValue(3), item->GetAttributeValue(4), item->GetAttributeValue(5), item->GetAttributeValue(6)
	);
}
#endif