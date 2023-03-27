/*
 * TCP listener.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_LISTENER_HXX
#define MYPROXY_LISTENER_HXX

struct Instance;

void
listener_init(Instance *instance, unsigned port);

void
listener_deinit(Instance *instance);

#endif
