// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string>
#include <string_view>

#include <sys/socket.h> // for struct ucred

struct lua_State;
class SocketAddress;
class SocketDescriptor;
namespace Lua { class AutoCloseList; }

class LClient {
	lua_State *const lua_state;

	Lua::AutoCloseList *auto_close;

	std::string server_version;

	const struct ucred peer_cred;

	std::string name_;

	std::string account;

public:
	LClient(lua_State *L, Lua::AutoCloseList &_auto_close,
		SocketDescriptor socket, SocketAddress _address,
		std::string_view server_version);

	lua_State *GetLuaState() const noexcept {
		return lua_state;
	}

	static void Register(lua_State *L);
	static LClient *New(lua_State *L, Lua::AutoCloseList &_auto_close,
			    SocketDescriptor socket, SocketAddress address,
			    std::string_view server_version);

	std::string_view GetName() const noexcept {
		return name_;
	}

	std::string_view GetAccount() const noexcept {
		return account;
	}

	std::string_view GetServerVersion() const noexcept {
		return server_version;
	}

	void SetServerVersion(std::string_view _server_version) noexcept {
		server_version = _server_version;
	}

private:
	int Index(lua_State *L);
	int NewIndex(lua_State *L);

	bool HavePeerCred() const noexcept {
		return peer_cred.pid >= 0;
	}
};
