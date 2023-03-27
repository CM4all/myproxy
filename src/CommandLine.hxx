/*
 * Command-line parser.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

struct Config;

void
parse_cmdline(Config &config, int argc, char **argv);
