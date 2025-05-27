// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <memory>
#include <string_view>

namespace Mysql {

class AuthHandler;

std::unique_ptr<AuthHandler>
MakeAuthHandler(std::string_view plugin_name, bool strict) noexcept;

} // namespace Mysql
