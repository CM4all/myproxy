/*
 * TCP listener.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

struct Instance;

void
listener_init(Instance *instance, unsigned port);

void
listener_deinit(Instance *instance);
