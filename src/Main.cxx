#include "Instance.hxx"
#include "CommandLine.hxx"
#include "Policy.hxx"
#include "Listener.hxx"
#include "Connection.hxx"

#include <event.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>

static void
deinit_signals(Instance *instance)
{
    event_del(&instance->sigterm_event);
    event_del(&instance->sigint_event);
    event_del(&instance->sigquit_event);
}

static void
exit_event_callback([[maybe_unused]] int fd, [[maybe_unused]] short event, void *ctx)
{
    Instance *instance = (Instance*)ctx;

    if (instance->should_exit)
        return;

    instance->should_exit = true;
    deinit_signals(instance);
    listener_deinit(instance);

    while (!list_empty(&instance->connections)) {
        Connection *connection =
            (Connection *)instance->connections.next;
        connection_close(connection);
    }
}

static void
init_signals(Instance *instance)
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
    static Instance instance;
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
