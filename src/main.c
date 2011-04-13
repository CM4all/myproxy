#include "instance.h"
#include "cmdline.h"
#include "policy.h"
#include "listener.h"
#include "connection.h"

#include <inline/compiler.h>

#include <event.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>

static void
deinit_signals(struct instance *instance)
{
    event_del(&instance->sigterm_event);
    event_del(&instance->sigint_event);
    event_del(&instance->sigquit_event);
}

static void
exit_event_callback(int fd __attr_unused, short event __attr_unused, void *ctx)
{
    struct instance *instance = (struct instance*)ctx;

    if (instance->should_exit)
        return;

    instance->should_exit = true;
    deinit_signals(instance);
    listener_deinit(instance);

    while (!list_empty(&instance->connections)) {
        struct connection *connection =
            (struct connection *)instance->connections.next;
        connection_close(connection);
    }
}

static void
init_signals(struct instance *instance)
{
    signal(SIGPIPE, SIG_IGN);

    event_set(&instance->sigterm_event, SIGTERM, EV_SIGNAL|EV_PERSIST,
              exit_event_callback, instance);
    event_add(&instance->sigterm_event, NULL);

    event_set(&instance->sigint_event, SIGINT, EV_SIGNAL|EV_PERSIST,
              exit_event_callback, instance);
    event_add(&instance->sigint_event, NULL);

    event_set(&instance->sigquit_event, SIGQUIT, EV_SIGNAL|EV_PERSIST,
              exit_event_callback, instance);
    event_add(&instance->sigquit_event, NULL);
}

int main(int argc, char **argv)
{
    static struct instance instance;
    instance_init(&instance);
    parse_cmdline(&instance, argc, argv);

    init_signals(&instance);

    policy_init();

    listener_init(&instance, 3306);

    event_dispatch();

    policy_deinit();

    instance_deinit(&instance);

    return EXIT_SUCCESS;
}
