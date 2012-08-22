/*
 * TCP listener.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_LISTENER_HXX
#define MYPROXY_LISTENER_HXX

struct instance;

void
listener_init(struct instance *instance, unsigned port);

void
listener_deinit(struct instance *instance);

#endif
