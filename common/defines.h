#ifndef __DEFINES_SERVICE_H__
#define __DEFINES_SERVICE_H__

#define WJ_GUILD_LEADER_SYSTEM
#define WJ_ACCE_SYSTEM
#define WJ_SOULBINDING_SYSTEM
#define WJ_SHOW_MOB_INFO
#define WJ_CHANNEL_CHANGE_SYSTEM
#define WJ_INVENTORY_EX_SYSTEM
#define WJ_MOUNT_SYSTEM
#define WJ_EVOLUTION_SYSTEM
#define WJ_DRAGON_HUNTERS_SYSTEM
#define WJ_ELDER_ATTRIBUTE_SYSTEM
#define WJ_DRAGONBONE_SYSTEM
#define WJ_ENABLE_TRADABLE_ICON
#define WJ_NEW_DROP_DIALOG
#define WJ_HIGHLIGHT_SYSTEM
#define WJ_OFFLINESHOP_SYSTEM
#define WJ_SHOPSTYLE_SYSTEM
#define WJ_SHOPSOLD_SYSTEM
#define WJ_WOLFMAN_CHARACTER
#ifdef WJ_WOLFMAN_CHARACTER
	#define USE_MOB_BLEEDING_AS_POISON
	#define USE_MOB_CLAW_AS_DAGGER
	#define USE_RESIST_CLAW_AS_DAGGER
#endif
#define WJ_PLAYER_PER_ACCOUNT5
#define WJ_CHEQUE_SYSTEM
#define WJ_NEW_EXCHANGE_WINDOW
#define WJ_EXTENDED_SHOP_SYSTEM
#define WJ_QUEST_RECEIVE_SYSTEM
#define WJ_SECURITY_SYSTEM
#define WJ_GROWTH_PET_SYSTEM
#define WJ_NATIONAL_POINT_SYSTEM
#define WJ_CHANGELOOK_SYSTEM
#define WJ_BOSS_SECURITY_SYSTEM
#define WJ_MAGIC_REDUCION_BONUS
#define WJ_ITEM_COMBINATION_SYSTEM
#define WJ_WEAPON_COSTUME_SYSTEM
#define WJ_SKILLBOOK_SYSTEM
#define WJ_7AND8TH_SKILLS
#define WJ_TIME_LIMIT_WEAPON_SYSTEM
#define WJ_QUIVER_SYSTEM
#define WJ_ENCHANT_COSTUME_SYSTEM
#define WJ_UPGRADE_SOCKET_SYSTEM
#define WJ_DICE_SYSTEM
#define WJ_ONLINE_PLAYER_COUNT
#define WJ_EXTENDED_PET_SYSTEM
#define WJ_SPLIT_INVENTORY_SYSTEM
#define WJ_DUNGEON_FOR_GUILD
#ifdef WJ_DUNGEON_FOR_GUILD
	#define WJ_MELEY_LAIR_DUNGEON
	#ifdef WJ_MELEY_LAIR_DUNGEON
		#define __DESTROY_INFINITE_STATUES_GM__
		#define __LASER_EFFECT_ON_75HP__
		#define __LASER_EFFECT_ON_50HP__
	#endif
#endif
#define WJ_TEMPLE_OCHAO
#ifdef WJ_TEMPLE_OCHAO
	#define HEALING_SKILL_VNUM 265
#endif
#define WJ_OKEY_CARD_EVENT
#define WJ_SELLING_DRAGONSOUL
#define WJ_CHANGE_ATTRIBUTE_PLUS
#define WJ_CHANGE_ATTRIBUTE_MINUS
#define WJ_SHOP_SEARCH_SYSTEM
#define WJ_GEM_SYSTEM

#define ENABLE_NEWSTUFF
#define ENABLE_CHAT_COLOR_SYSTEM
#define ENABLE_CHECK_GHOSTMODE
#define ENABLE_PARTYKILL
#define ENABLE_GOHOME_IFNOMAP
#define ENABLE_CMD_PLAYER
#define ENABLE_EXPTABLE_FROMDB
#define ENABLE_EFFECT_PENETRATE
#define ENABLE_NEWEXP_CALCULATION
#define ENABLE_EFFECT_EXTRAPOT
#define ENABLE_FORCE2MASTERSKILL
#define ENABLE_CHAT_LOGGING
#define ENABLE_FULL_NOTICE
#define ENABLE_SHOW_QUIZ_NUMBER_OXEVENT
#define ENABLE_IP_BAN_SYSTEM
#define ENABLE_PORT_SECURITY
#define ENABLE_PVP_EFFECT
#define ENABLE_SMITH_EFFECT
#define ENABLE_EXTENDED_RELOAD_COMMANDS
#define ENABLE_NEW_RETARDED_GF_START_POSITION
#define ENABLE_SWITCH_ALL
#define ENABLE_CMD_IPURGE_EX

/***
	#define ENABLE_HACK_TELEPORT_LOG
	#define ENABLE_MOUNTSKILL_CHECK
	#define ENABLE_BRAZIL_AUTH_FEATURE
	#define ENABLE_PCBANG_FEATURE
	#define ENABLE_MONARCH_MOB_CMD_MAP_CHECK
	#define ENABLE_PASSPOD_FEATURE
	#define ENABLE_FLUSH_CACHE_FEATURE
	#define ENABLE_SHOP_BLACKLIST
	#define ENABLE_FIREWORK_STUN
	#define ENABLE_DROP_GOLD
	#define ENABLE_DROP_ITEM
	#define ENABLE_ACCOUNT_W_SPECIALCHARS
	#define ENABLE_BLOCK_CMD_SHORTCUT
***/

enum eDefines
{
	MAP_ALLOW_LIMIT = 64, // 32 default
};

#endif
