// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "lua/Value.hxx"

#include <string>
#include <string_view>

#include <sys/socket.h> // for struct ucred

struct lua_State;
class SocketAddress;
class SocketDescriptor;

class LClient {
	Lua::Value address;

	std::string server_version;

	/**
	 * A table containing notes created by Lua code.
	 */
	Lua::Value notes;

	const struct ucred peer_cred;

public:
	LClient(lua_State *L, SocketDescriptor socket, SocketAddress _address,
		std::string_view server_version);

	static void Register(lua_State *L);
	static LClient *New(lua_State *L, SocketDescriptor socket, SocketAddress address,
			    std::string_view server_version);

	std::string_view GetServerVersion() const noexcept {
		return server_version;
	}

	void SetServerVersion(std::string_view _server_version) noexcept {
		server_version = _server_version;
	}

private:
	int Index(lua_State *L, const char *name);
	static int _Index(lua_State *L);

	int NewIndex(lua_State *L, const char *name, int value_idx);
	static int _NewIndex(lua_State *L);

	bool HavePeerCred() const noexcept {
		return peer_cred.pid >= 0;
	}
};
