// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "CommandLine.hxx"
#include "LResolver.hxx"
#include "Policy.hxx"
#include "LConnection.hxx"
#include "lua/Error.hxx"
#include "lua/PushCClosure.hxx"
#include "lua/RunFile.hxx"
#include "lua/Util.hxx"
#include "lua/net/SocketAddress.hxx"
#include "lua/event/Init.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "system/SetupProcess.hxx"
#include "util/PrintException.hxx"
#include "util/ScopeExit.hxx"
#include "config.h"

#ifdef HAVE_PG
#include "lua/pg/Init.hxx"
#endif

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <systemd/sd-daemon.h>

#include <stddef.h>
#include <stdlib.h>

static int
l_mysql_listen(lua_State *L)
try {
	auto &instance = *(Instance *)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameter count");

	auto handler = std::make_shared<Lua::Value>(L, Lua::StackIndex(2));

	if (lua_isstring(L, 1)) {
		const char *address_string = lua_tostring(L, 1);

		AllocatedSocketAddress address;
		address.SetLocal(address_string);

		instance.AddListener(address, std::move(handler));
	} else
		luaL_argerror(L, 1, "path expected");

	return 0;
} catch (...) {
	Lua::RaiseCurrent(L);
}

static void
SetupConfigState(lua_State *L, Instance &instance)
{
	luaL_openlibs(L);

	Lua::InitEvent(L, instance.GetEventLoop());

#ifdef HAVE_PG
	Lua::InitPg(L, instance.GetEventLoop());
#endif

	Lua::InitSocketAddress(L);
	RegisterLuaResolver(L);

	Lua::SetGlobal(L, "mysql_listen",
		       Lua::MakeCClosure(l_mysql_listen,
					 Lua::LightUserData(&instance)));
}

static void
SetupRuntimeState(lua_State *L)
{
	Lua::SetGlobal(L, "mysql_listen", nullptr);

	UnregisterLuaResolver(L);
	RegisterLuaConnection(L);
}

int
main(int argc, char **argv) noexcept
try {
	Config config;
	parse_cmdline(config, argc, argv);

	SetupProcess();

	/* force line buffering so Lua "print" statements are flushed
	   even if stdout is a pipe to systemd-journald */
	setvbuf(stdout, nullptr, _IOLBF, 0);
	setvbuf(stderr, nullptr, _IOLBF, 0);

	Instance instance{config};
	SetupConfigState(instance.GetLuaState(), instance);

	Lua::RunFile(instance.GetLuaState(), config.config_path);

	instance.Check();

	SetupRuntimeState(instance.GetLuaState());

	policy_init();

	/* tell systemd we're ready */
	sd_notify(0, "READY=1");

	instance.GetEventLoop().Run();

	policy_deinit();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
