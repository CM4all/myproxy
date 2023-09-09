// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "lua/CoRunner.hxx"
#include "lua/Resume.hxx"

class Reload final : Lua::ResumeListener {
	Lua::CoRunner runner;

	enum class State {
		IDLE,
		BUSY,
		AGAIN,
	} state = State::IDLE;

public:
	explicit Reload(lua_State *L) noexcept
		:runner(L) {}

	void Start() noexcept;

private:
	/* virtual methods from Lua::ResumeListener */
	void OnLuaFinished(lua_State *L) noexcept override;
	void OnLuaError(lua_State *L, std::exception_ptr e) noexcept override;
};
