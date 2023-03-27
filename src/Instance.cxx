// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Connection.hxx"

#include <cstddef>

Instance::Instance(const Config &_config)
	:config(_config)
{
}

Instance::~Instance() noexcept
{
	event_base_free(event_base);
}
