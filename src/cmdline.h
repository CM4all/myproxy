/*
 * Command-line parser.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_CMDLINE_H
#define MYPROXY_CMDLINE_H

struct instance;

void
parse_cmdline(struct instance *instance, int argc, char **argv);

#endif
