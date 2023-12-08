// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "NativePassword.hxx"
#include "SHA1.hxx"
#include "MysqlSerializer.hxx"
#include "util/SpanCast.hxx"

#include <stdexcept>

namespace Mysql {

std::span<const std::byte>
NativePassword::GenerateResponse(std::string_view password,
				 std::span<const std::byte> password_sha1,
				 std::span<const std::byte> data1,
				 std::span<const std::byte> data2)
{
	static_assert(sizeof(buffer) == SHA1_DIGEST_LENGTH);

	if (auto &last = data2.empty() ? data1 : data2;
	    data1.size() + data2.size() != 21 || last.back() != std::byte{})
		// null byte is missing
		throw std::invalid_argument{"Malformed auth_plugin_data"};
	else
		// strip the null byte
		last = last.first(last.size() - 1);

	SHA1Digest password_sha1_buffer;

	if (password_sha1.empty())
		password_sha1 = password_sha1_buffer = SHA1(password);

	assert(password_sha1.size() == password_sha1_buffer.size());

	const auto password_sha1_sha1 = SHA1(password_sha1);

	SHA1State state;
	state.Update(data1);
	state.Update(data2);
	state.Update(password_sha1_sha1);
	state.Final(buffer);

	for (std::size_t i = 0; i < buffer.size(); ++i)
		buffer[i] ^= password_sha1[i];

	return buffer;
}

} // namespace Mysql
