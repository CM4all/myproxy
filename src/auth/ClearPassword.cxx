// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ClearPassword.hxx"
#include "MysqlSerializer.hxx"

#include <stdexcept>

namespace Mysql {

std::span<const std::byte>
ClearPassword::GenerateResponse(std::string_view password,
				std::span<const std::byte> password_sha1,
				[[maybe_unused]] std::span<const std::byte> data1,
				[[maybe_unused]] std::span<const std::byte> data2)
{
	if (password.empty() && !password_sha1.empty())
		throw std::invalid_argument{"Need clear-text password"};

	return AsBytes(password);
}

} // namespace Mysql
