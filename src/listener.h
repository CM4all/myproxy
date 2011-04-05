/*
 * TCP listener.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_LISTENER_H
#define MYPROXY_LISTENER_H

struct instance;

void
listener_init(struct instance *instance, unsigned port);

void
listener_deinit(struct instance *instance);

#endif
