// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "AsyncResolver.hxx"
#include "event/systemd/ResolvedClient.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/PushCClosure.hxx"
#include "lua/Resume.hxx"
#include "lua/Util.hxx"
#include "lua/net/SocketAddress.hxx"
#include "util/Cancellable.hxx"

class LuaResolveHostnameRequest final : Systemd::ResolveHostnameHandler {
	lua_State *const L;

	CancellablePointer cancel_ptr;

public:
	explicit LuaResolveHostnameRequest(lua_State *_L) noexcept
		:L(_L) {}

	void Start(EventLoop &event_loop, std::string_view hostname) noexcept {
		ResolveHostname(event_loop, hostname, 3306, AF_UNSPEC,
				*this, cancel_ptr);
	}

	void Cancel() noexcept {
		cancel_ptr.Cancel();
	}

	/* virtual methods from ResolveHostnameHandler */
	void OnResolveHostname(std::span<const SocketAddress> address) noexcept override {
		Lua::NewSocketAddress(L, address.front());
		Lua::Resume(L, 1);
	}

	void OnResolveHostnameError(std::exception_ptr error) noexcept override {
		/* return [nil, error_message] for assert() */
		Lua::Push(L, nullptr);
		Lua::Push(L, error);
		Lua::Resume(L, 2);
	}
};

static constexpr char lua_resolve_hostname_request_class[] = "ResolveHostnameRequest";
using LuaResolveHostnameRequestClass =
	Lua::Class<LuaResolveHostnameRequest, lua_resolve_hostname_request_class>;

int
l_mysql_async_resolve(lua_State *L)
{
	auto &event_loop = *(EventLoop *)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_gettop(L) != 1)
		return luaL_error(L, "Invalid parameter count");

	const char *s = luaL_checkstring(L, 1);

	if (*s == '/' || *s == '@') {
		AllocatedSocketAddress address;
		address.SetLocal(s);
		Lua::NewSocketAddress(L, std::move(address));
		return 1;
	}

	auto *request = LuaResolveHostnameRequestClass::New(L, L);
	request->Start(event_loop, s);
	return lua_yield(L, 1);
}

void
InitAsyncResolver(EventLoop &event_loop, lua_State *L) noexcept
{
	LuaResolveHostnameRequestClass::Register(L);

	Lua::SetField(L, Lua::RelativeStackIndex{-1}, "__close", [](auto _L){
		auto &request = LuaResolveHostnameRequestClass::Cast(_L, 1);
		request.Cancel();
		return 0;
	});

	lua_pop(L, 1);

	Lua::SetGlobal(L, "mysql_resolve",
		       Lua::MakeCClosure(l_mysql_async_resolve,
					 Lua::LightUserData{&event_loop}));
}
