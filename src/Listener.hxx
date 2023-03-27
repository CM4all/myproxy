// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>

struct Instance;

void
listener_init(Instance *instance, uint16_t port);

void
listener_deinit(Instance *instance);
