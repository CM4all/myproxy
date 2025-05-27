// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Handler.hxx"

#include <array>

namespace Mysql {

/**
 * @see https://mariadb.com/kb/en/authentication-plugin-ed25519/
 */
class ClientEd25519 final : public AuthHandler {
	std::array<std::byte, 64> sig;

public:
	constexpr ClientEd25519() noexcept
		:AuthHandler("ed25519") {}

	// virtual methods from class AuthHandler
	std::span<const std::byte> GenerateResponse(std::string_view password,
						    std::span<const std::byte> password_sha1,
						    std::span<const std::byte> data1,
						    std::span<const std::byte> data2) override;
};

} // namespace Mysql
