
#include "stdafx.h"

#include "questmanager.h"
#include "DragonLair.h"
#include "char.h"
#include "guild.h"

namespace quest
{
	int dl_startRaid(lua_State* L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		long baseMapIndex = lua_tonumber(L, -1);

		CDragonLairManager::instance().Start(ch->GetMapIndex(), baseMapIndex, ch->GetGuild()->GetID());

		return 0;
	}
	
#ifdef WJ_DRAGON_HUNTERS_SYSTEM
	int dl_OpenRanking(lua_State* L)
	{
		CDragonLairManager::instance().OpenRanking(CQuestManager::instance().GetCurrentCharacterPtr());
		return 0;
	}
#endif

	void RegisterDragonLairFunctionTable()
	{
		luaL_reg dl_functions[] =
		{
			{	"startRaid",	dl_startRaid	},
#ifdef WJ_DRAGON_HUNTERS_SYSTEM
			{	"OpenRanking",	dl_OpenRanking	},
#endif
			{	NULL,			NULL			}
		};

		CQuestManager::instance(). AddLuaFunctionTable("DragonLair", dl_functions);
	}
}

