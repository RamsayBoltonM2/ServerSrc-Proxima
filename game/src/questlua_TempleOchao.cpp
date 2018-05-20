#include "stdafx.h"
#include "questlua.h"
#include "questmanager.h"
#include "TempleOchao.h"

#undef sys_err
#ifndef __WIN32__
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, __VA_ARGS__)
#endif

namespace quest
{
	int temple_ochao_initialize(lua_State* L)
	{
		TempleOchao::CMgr::instance().Prepare();
		return 0;
	}
	
	void RegisterTempleOchaoFunctionTable()
	{
		luaL_reg temple_ochao_functions[] =
		{

			{"initialize", temple_ochao_initialize},
			{NULL, NULL}
		};
		
		CQuestManager::instance().AddLuaFunctionTable("temple_ochao", temple_ochao_functions);
	}
}
