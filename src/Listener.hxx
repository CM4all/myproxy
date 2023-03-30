// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Connection.hxx"
#include "event/net/TemplateServerSocket.hxx"
#include "net/AllocatedSocketAddress.hxx"

using MyProxyListener =
	TemplateServerSocket<Connection, EventLoop &, AllocatedSocketAddress>;
