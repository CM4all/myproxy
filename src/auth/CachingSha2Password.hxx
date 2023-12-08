// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Handler.hxx"
#include "util/AllocatedArray.hxx"

#include <array>
#include <string>

namespace Mysql {

class CachingSha2Password final : public AuthHandler {
	std::array<std::byte, 32> buffer;

	std::string last_password;

	std::array<std::byte, 20> last_auth_plugin_data;

	AllocatedArray<std::byte> encrypted_password;

	bool public_key_requested = false;

public:
	CachingSha2Password() noexcept
		:AuthHandler("caching_sha2_password") {}

	~CachingSha2Password() noexcept override;

	// virtual methods from class AuthHandler
	std::span<const std::byte> GenerateResponse(std::string_view password,
						    std::span<const std::byte> password_sha1,
						    std::span<const std::byte> data1,
						    std::span<const std::byte> data2) override;
	std::span<const std::byte> HandlePacket(std::span<const std::byte> payload) override;
};

} // namespace Mysql
