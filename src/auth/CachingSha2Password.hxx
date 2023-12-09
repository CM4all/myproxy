// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Handler.hxx"

#include <array>

namespace Mysql {

class CachingSha2Password final : public AuthHandler {
	std::array<std::byte, 32> buffer;

public:
	constexpr CachingSha2Password() noexcept
		:AuthHandler("caching_sha2_password") {}

	// virtual methods from class AuthHandler
	std::span<const std::byte> GenerateResponse(std::string_view password,
						    std::span<const std::byte> password_sha1,
						    std::span<const std::byte> data1,
						    std::span<const std::byte> data2) override;
	bool HandlePacket(std::span<const std::byte> payload) override;
};

} // namespace Mysql
