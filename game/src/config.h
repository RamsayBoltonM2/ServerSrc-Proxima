
#ifndef __INC_METIN_II_GAME_CONFIG_H__
#define __INC_METIN_II_GAME_CONFIG_H__

enum
{
	ADDRESS_MAX_LEN = 15
};

#ifdef ENABLE_NEWSTUFF
enum ItemDestroyTime {ITEM_DESTROY_TIME_AUTOGIVE, ITEM_DESTROY_TIME_DROPGOLD, ITEM_DESTROY_TIME_DROPITEM, ITEM_DESTROY_TIME_MAX};
#endif

void config_init(const std::string& st_localeServiceName); // default "" is CONFIG

extern char sql_addr[256];

extern WORD mother_port;
extern WORD p2p_port;

extern char db_addr[ADDRESS_MAX_LEN + 1];
extern WORD db_port;

extern char teen_addr[ADDRESS_MAX_LEN + 1];
extern WORD teen_port;

extern char passpod_addr[ADDRESS_MAX_LEN + 1];
extern WORD passpod_port;

extern int passes_per_sec;
extern int save_event_second_cycle;
extern int ping_event_second_cycle;
extern int test_server;
extern bool	guild_mark_server;
extern BYTE guild_mark_min_level;
extern bool	distribution_test_server;
extern bool	china_event_server;

extern bool	g_bNoMoreClient;
extern bool	g_bNoRegen;

extern bool	g_bTrafficProfileOn;		///< true 이면 TrafficProfiler 를 켠다.

extern BYTE	g_bChannel;

extern bool	map_allow_find(int index);
extern void	map_allow_copy(long * pl, int size);
extern bool	no_wander;

extern int	g_iUserLimit;
extern time_t	g_global_time;

const char *	get_table_postfix();

extern std::string	g_stHostname;
extern std::string	g_stLocale;
extern std::string	g_stLocaleFilename;

extern char		g_szPublicIP[16];
extern char		g_szInternalIP[16];

extern int (*is_twobyte) (const char * str);
extern int (*check_name) (const char * str);

extern bool		g_bSkillDisable;

extern int		g_iFullUserCount;
extern int		g_iBusyUserCount;
extern void		LoadStateUserCount();

extern bool	g_bEmpireWhisper;

extern BYTE	g_bAuthServer;
extern BYTE	g_bBilling;

extern BYTE	PK_PROTECT_LEVEL;

extern void	LoadValidCRCList();
extern bool	IsValidProcessCRC(DWORD dwCRC);
extern bool	IsValidFileCRC(DWORD dwCRC);

extern std::string	g_stAuthMasterIP;
extern WORD		g_wAuthMasterPort;

extern std::string	g_stClientVersion;
extern bool		g_bCheckClientVersion;
extern void		CheckClientVersion();

extern std::string	g_stQuestDir;
//extern std::string	g_stQuestObjectDir;
extern std::set<std::string> g_setQuestObjectDir;


extern std::vector<std::string>	g_stAdminPageIP;
extern std::string	g_stAdminPagePassword;

extern int	SPEEDHACK_LIMIT_COUNT;
extern int 	SPEEDHACK_LIMIT_BONUS;

extern int g_iSyncHackLimitCount;

extern int g_server_id;
extern std::string g_strWebMallURL;

extern int VIEW_RANGE;
extern int VIEW_BONUS_RANGE;

extern bool g_bCheckMultiHack;
extern bool g_protectNormalPlayer;      // 범법자가 "평화모드" 인 일반유저를 공격하지 못함
extern bool g_noticeBattleZone;         // 중립지대에 입장하면 안내메세지를 알려줌

extern DWORD g_GoldDropTimeLimitValue;
#ifdef ENABLE_NEWSTUFF
extern DWORD g_ItemDropTimeLimitValue;
extern DWORD g_BoxUseTimeLimitValue;
extern DWORD g_BuySellTimeLimitValue;
extern bool g_NoDropMetinStone;
extern bool g_NoMountAtGuildWar;
#endif

extern int gPlayerMaxLevel;
extern int gShutdownAge;
extern int gShutdownEnable;	// 기본 0. config에서 지정해야함.

// for new functions
extern int g_iItemStackCount;
extern int g_iItemOwnershipTime;
// for new functions end

extern bool g_BlockCharCreation;

#ifdef WJ_MOUNT_SYSTEM
extern void Mount_map_unallowed_add(int idx);
extern bool Mount_map_unallowed_find(int idx);
#endif

#ifdef WJ_EXTENDED_PET_SYSTEM
extern void Pet_map_unallowed_add(int idx);
extern bool Pet_map_unallowed_find(int idx);
#endif

#ifdef ENABLE_NEWSTUFF
extern bool	g_bEmpireShopPriceTripleDisable;
extern bool g_bGlobalShoutEnable;
extern bool g_bDisablePrismNeed;
extern bool g_bDisableEmotionMask;
extern bool g_bDisableItemBonusChangeTime;
extern bool	g_bEnableBootaryCheck;
extern bool	g_bGMHostCheck;
extern bool	g_bChinaIntoxicationCheck;
extern bool	g_bEnableSpeedHackCrash;
extern bool g_bBeltAllowItems;
extern bool g_bPackageEnable;
//////////////////////////////
extern int g_iStatusPointGetLevelLimit;
extern int g_iStatusPointSetMaxValue;
extern int g_iShoutLimitLevel;
extern int g_aiItemDestroyTime[ITEM_DESTROY_TIME_MAX];
extern int g_aiItemDestroyTime[ITEM_DESTROY_TIME_MAX];
extern int g_iDbLogLevel;
extern int g_iSysLogLevel;
extern int g_iMaxRankPoints;
extern int g_iTaxes;
extern BYTE g_bItemCountLimit;
#endif

#endif /* __INC_METIN_II_GAME_CONFIG_H__ */

