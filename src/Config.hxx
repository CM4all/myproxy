// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"
#include "net/SocketConfig.hxx"

struct Config {
	SocketConfig listener;

	AllocatedSocketAddress server_address;
};
