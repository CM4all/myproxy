/*
 * Command-line parser.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_CMDLINE_HXX
#define MYPROXY_CMDLINE_HXX

struct Instance;

void
parse_cmdline(Instance *instance, int argc, char **argv);

#endif
