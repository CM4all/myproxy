// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Reload.hxx"
#include "lua/Assert.hxx"
#include "util/PrintException.hxx"

void
Reload::Start() noexcept
{
	switch (state) {
	case State::IDLE:
		break;

	case State::BUSY:
		state = State::AGAIN;
		return;

	case State::AGAIN:
		return;
	}

	const auto main_L = runner.GetMainState();
	const Lua::ScopeCheckStack check_stack{main_L};
	lua_getglobal(main_L, "reload");

	if (lua_isnil(main_L, -1)) {
		lua_pop(main_L, 1);
		return;
	}

	state = State::BUSY;

	const auto thread_L = runner.CreateThread(*this);
	lua_xmove(main_L, thread_L, 1);

	Lua::Resume(thread_L, 0);
}

void
Reload::OnLuaFinished(lua_State *) noexcept
{
	runner.Cancel();

	const bool again = state == State::AGAIN;
	state = State::IDLE;

	if (again)
		Start();
}

void
Reload::OnLuaError(lua_State *, std::exception_ptr e) noexcept
{
	PrintException(e);

	runner.Cancel();

	const bool again = state == State::AGAIN;
	state = State::IDLE;

	if (again)
		Start();
}
