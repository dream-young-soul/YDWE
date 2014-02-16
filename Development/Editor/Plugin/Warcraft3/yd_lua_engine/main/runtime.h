#pragma once

#include <base/lua/state.h>

namespace base { namespace warcraft3 { namespace lua_engine {
	namespace runtime
	{
		extern int error_handle;
		extern int handle_level;
		extern int handle_ud_table;
		extern bool sleep;
		extern bool catch_crash;

		void initialize();
		void set_function(int& result, lua_State* L, int index);
		void get_function(int result, lua_State* L);
	}
}}}
