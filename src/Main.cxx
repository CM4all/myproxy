// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "CommandLine.hxx"
#include "LHandler.hxx"
#include "LResolver.hxx"
#include "Policy.hxx"
#include "LClient.hxx"
#include "LAction.hxx"
#include "lua/Error.hxx"
#include "lua/PushCClosure.hxx"
#include "lua/RunFile.hxx"
#include "lua/Util.hxx"
#include "lua/StringView.hxx"
#include "lua/net/SocketAddress.hxx"
#include "lua/sodium/Init.hxx"
#include "lua/event/Init.hxx"
#include "lua/io/XattrTable.hxx"
#include "lua/io/CgroupInfo.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "memory/fb_pool.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/LocalSocketAddress.hxx"
#include "net/Parser.hxx"
#include "system/SetupProcess.hxx"
#include "util/PrintException.hxx"
#include "util/ScopeExit.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include "AsyncResolver.hxx"
#endif

#ifdef HAVE_PG
#include "lua/pg/Init.hxx"
#endif

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <stdexcept>

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h> // for chdir()

#ifdef HAVE_LIBSYSTEMD

/**
 * A "magic" pointer used to identify our artificial "systemd" Lua
 * keyword, which is wrapped as "light user data".
 */
static int systemd_magic = 42;

static bool
IsSystemdMagic(lua_State *L, int idx)
{
	return lua_islightuserdata(L, idx) &&
		lua_touserdata(L, idx) == &systemd_magic;
}

#endif // HAVE_LIBSYSTEMD

static auto
ParameterToLuaHandler(lua_State *L, int idx)
try {
	return std::make_shared<LuaHandler >(L, Lua::StackIndex{idx});
} catch (const std::invalid_argument &e) {
	luaL_argerror(L, idx, e.what());
	gcc_unreachable();
}

static int
l_mysql_listen(lua_State *L)
try {
	auto &instance = *(Instance *)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameter count");

	auto handler = ParameterToLuaHandler(L, 2);

	if (lua_isstring(L, 1)) {
		const auto address_string = Lua::ToStringView(L, 1);

		instance.AddListener(LocalSocketAddress{address_string}, std::move(handler));
#ifdef HAVE_LIBSYSTEMD
	} else if (IsSystemdMagic(L, 1)) {
		instance.AddSystemdListener(std::move(handler));
#endif // HAVE_LIBSYSTEMD
	} else
		luaL_argerror(L, 1, "path expected");

	return 0;
} catch (...) {
	Lua::RaiseCurrent(L);
}

static int
l_prometheus_listen(lua_State *L)
try {
	auto &instance = *(Instance *)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_gettop(L) != 1)
		return luaL_error(L, "Invalid parameter count");

	if (lua_isstring(L, 1)) {
		instance.AddPrometheusListener(ParseSocketAddress(lua_tostring(L, 1), 9100, true));
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

	Lua::InitSodium(L);

	Lua::InitEvent(L, instance.GetEventLoop());

#ifdef HAVE_PG
	Lua::InitPg(L, instance.GetEventLoop());
#endif

	Lua::InitSocketAddress(L);
	RegisterLuaResolver(L, instance.GetEventLoop(), instance.GetStats());

#ifdef HAVE_LIBSYSTEMD
	Lua::SetGlobal(L, "systemd", Lua::LightUserData(&systemd_magic));
#endif // HAVE_LIBSYSTEMD

	Lua::SetGlobal(L, "mysql_listen",
		       Lua::MakeCClosure(l_mysql_listen,
					 Lua::LightUserData(&instance)));

	Lua::SetGlobal(L, "prometheus_listen",
		       Lua::MakeCClosure(l_prometheus_listen,
					 Lua::LightUserData(&instance)));
}

static void
ChdirContainingDirectory(const char *path)
{
	const char *slash = strrchr(path, '/');
	if (slash == nullptr || slash == path)
		return;

	const std::string parent{path, slash};
	if (chdir(parent.c_str()) < 0)
		throw FmtErrno("Failed to change to {}", parent);
}

static void
LoadConfigFile(lua_State *L, const char *path)
{
	ChdirContainingDirectory(path);
	Lua::RunFile(L, path);

	if (chdir("/") < 0)
		throw FmtErrno("Failed to change to {}", "/");
}

static void
SetupRuntimeState(lua_State *L)
{
	Lua::SetGlobal(L, "mysql_listen", nullptr);
	Lua::SetGlobal(L, "prometheus_listen", nullptr);

	Lua::InitXattrTable(L);

	UnregisterLuaResolver(L);
	LClient::Register(L);
	Lua::RegisterCgroupInfo(L);
	RegisterLuaAction(L);
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

	const ScopeFbPoolInit fb_pool_init;

	Instance instance;
	SetupConfigState(instance.GetLuaState(), instance);

	LoadConfigFile(instance.GetLuaState(), config.config_path);

	instance.Check();

	SetupRuntimeState(instance.GetLuaState());

#ifdef HAVE_LIBSYSTEMD
	InitAsyncResolver(instance.GetEventLoop(),
			  instance.GetLuaState());
#endif

	policy_init();

#ifdef HAVE_LIBSYSTEMD
	/* tell systemd we're ready */
	sd_notify(0, "READY=1");
#endif

	instance.GetEventLoop().Run();

	policy_deinit();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
