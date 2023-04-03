// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"

#include <string>

struct ErrAction {
	std::string msg;
};

struct ConnectAction {
	AllocatedSocketAddress address;

	std::string username, password, database;
};
