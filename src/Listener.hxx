// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Connection.hxx"
#include "LHandler.hxx"
#include "event/net/TemplateServerSocket.hxx"

#include <memory>

struct Stats;

using MyProxyListener =
	TemplateServerSocket<Connection, EventLoop &, Stats &,
			     std::shared_ptr<LuaHandler>>;
