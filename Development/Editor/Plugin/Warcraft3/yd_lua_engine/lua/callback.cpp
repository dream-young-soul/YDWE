#include "../lua/callback.h"
#include "../lua/jassbind.h"
#include "../main/runtime.h"
#include <base/warcraft3/jass/trampoline_function.h>
#include <Windows.h>

namespace base { namespace warcraft3 { namespace lua_engine {

	uintptr_t jass_read(jassbind* lj, jass::variable_type vt, int idx);

	lua::state* get_mainthread(lua::state* thread)
	{
		lua_rawgeti(thread->self(), LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
		lua::state* ml = (lua::state*)thread->tothread(-1);
		thread->pop(1);
		return ml;
	}

	int safe_call_not_sleep(lua::state* ls, int nargs, int nresults)
	{
		int error_handle = 0;
		if (runtime::error_handle != 0) 
		{
			error_handle = 1;
			runtime::get_function(runtime::error_handle, ls->self());
			ls->insert(error_handle);
		}

		int error = lua_pcall(ls->self(), nargs, nresults, error_handle);

		if (error_handle == 0)
		{
			switch (error)
			{
			case LUA_OK:
				break;
			case LUA_ERRRUN:
			case LUA_ERRMEM:
			case LUA_ERRERR:
				printf("Error(%d): %s\n", error, ls->tostring(-1));
				ls->pop(1);
				break;
			default:
				printf("Error(%d)\n", error);
				break;
			}
		}
		else
		{
			ls->remove(error_handle);
		}

		return error;
	}

	int safe_resume(lua::state* thread, lua::state* ls, int nargs, int& nresults)
	{
		if (!thread->checkstack(nargs + 1)) {
			ls->pushstring("too many arguments to resume");
			nresults = 1;
			return LUA_ERRERR;
		}
		ls->pushvalue(-nargs-1);
		ls->xmove(thread, 1);
		ls->xmove(thread, nargs);
		int error = lua_resume(thread->self(), ls->self(), nargs);

		if (error == LUA_OK || error == LUA_YIELD)
		{
			int nres = thread->gettop();
			if (!ls->checkstack(nres + 1)) {
				thread->pop(nres);
				ls->pushstring("too many results to resume");
				nresults = 1;
				return LUA_ERRERR;
			}
			thread->xmove(ls, nres);
			nresults = nres;
			return error;
		}
		else
		{
			thread->xmove(ls, 1);
			nresults = 1;
			return error;
		}
	}

	int safe_call_has_sleep(lua::state* ls, int nargs, int /*nresults*/)
	{
		lua::state* thread = ls->newthread();
		int base = ls->gettop() - (nargs + 1);
		ls->insert(base);

		int nresults = 0;
		int error = safe_resume(thread, ls, nargs, nresults);

		switch (error)
		{
		case LUA_OK:
		case LUA_YIELD:
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			printf("Error(%d): %s\n", error, ls->tostring(-1));
			ls->pop(1);
			break;
		default:
			printf("Error(%d)\n", error);
			break;
		}
		ls->remove(base);
		return error;
	}

	int safe_call(lua::state* ls, int nargs, int nresults)
	{
		if (runtime::sleep)
		{
			return safe_call_has_sleep(ls, nargs, nresults);
		}
		else
		{
			return safe_call_not_sleep(ls, nargs, nresults);
		}
	}

	uintptr_t safe_call_ref(lua::state* ls, uint32_t ref, size_t nargs, jass::variable_type result_vt)
	{
		int base = ls->gettop() - nargs + 1;
		ls->rawgeti(LUA_REGISTRYINDEX, ref);
		if (!ls->isfunction(-1))
		{
			printf("callback::call() attempt to call (not a function)\n");
			ls->pop(1);
			return false;
		}
		ls->insert(base);

		if (safe_call(ls, nargs, (result_vt != jass::TYPE_NOTHING) ? 1 : 0) != LUA_OK)
		{
			return 0;
		}
		
		if (result_vt == jass::TYPE_NOTHING)
		{
			return 0;
		}

		uintptr_t ret = jass_read((jassbind*)ls, result_vt, -1);
		ls->pop(1);
		return ret;
	}

	uint32_t __fastcall jass_callback(uint32_t ls, uint32_t param)
	{
		return safe_call_ref((lua::state*)ls, param, 0, jass::TYPE_BOOLEAN);
	}

	uint32_t cfunction_to_code(lua::state* ls, uint32_t index)
	{
		ls->pushvalue(index);
		if (runtime::sleep)
		{
			return jass::trampoline_create(jass_callback, (uintptr_t)get_mainthread(ls), (uintptr_t)luaL_ref(ls->self(), LUA_REGISTRYINDEX));
		}
		else
		{
			return jass::trampoline_create(jass_callback, (uintptr_t)ls->self(), (uintptr_t)luaL_ref(ls->self(), LUA_REGISTRYINDEX));
		}
	}
}}}
