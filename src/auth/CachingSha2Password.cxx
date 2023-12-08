// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CachingSha2Password.hxx"
#include "SHA256.hxx"
#include "MysqlSerializer.hxx"
#include "util/SpanCast.hxx"

#include <stdexcept>

namespace Mysql {

std::span<const std::byte>
CachingSha2Password::GenerateResponse(std::string_view password,
				      std::span<const std::byte> password_sha1,
				      std::span<const std::byte> data1,
				      std::span<const std::byte> data2)
{
	static_assert(sizeof(buffer) == SHA256_DIGEST_LENGTH);

	if (password.empty() && !password_sha1.empty())
		throw std::invalid_argument{"Need clear-text password"};

	if (auto &last = data2.empty() ? data1 : data2;
	    data1.size() + data2.size() != 21 || last.back() != std::byte{})
		// null byte is missing
		throw std::invalid_argument{"Malformed auth_plugin_data"};
	else
		// strip the null byte
		last = last.first(last.size() - 1);

	const auto password_sha256 = SHA256(password);
	const auto password_sha256_sha256 = SHA256(password_sha256);

	SHA256State state;
	state.Update(password_sha256_sha256);
	state.Update(data1);
	state.Update(data2);
	state.Final(buffer);

	for (std::size_t i = 0; i < buffer.size(); ++i)
		buffer[i] ^= password_sha256[i];

	return buffer;
}

} // namespace Mysql
