// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ClearPassword.hxx"
#include "util/SpanCast.hxx"

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

	/* the password must be null-terminated, but since we have
	   only a std::string_view, we have to copy it to a
	   std::string which implicitly appends a null terminator, and
	   then include this null terminator in the return value */
	buffer = password;
	const std::string_view terminated_password{buffer.data(), buffer.length() + 1};
	return AsBytes(terminated_password);
}

} // namespace Mysql
