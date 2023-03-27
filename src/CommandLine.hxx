/*
 * Command-line parser.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

struct Instance;

void
parse_cmdline(Instance *instance, int argc, char **argv);
