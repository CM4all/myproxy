/*
 * Command-line parser.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_CMDLINE_HXX
#define MYPROXY_CMDLINE_HXX

struct instance;

void
parse_cmdline(struct instance *instance, int argc, char **argv);

#endif
