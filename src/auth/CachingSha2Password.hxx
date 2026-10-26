// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Handler.hxx"
#include "util/AllocatedArray.hxx"

#include <array>
#include <string>
#include <string_view>

namespace Mysql {

class CachingSha2Password final : public AuthHandler {
	std::array<std::byte, 32> buffer;

	std::string last_password;

	std::array<std::byte, 20> last_auth_plugin_data;

	AllocatedArray<std::byte> encrypted_password;

	/**
	 * The trusted public key (PEM) of the server; if not empty,
	 * it is used instead of requesting the key from the server.
	 */
	const std::string_view server_public_key;

	/**
	 * Are we allowed to request the server's (unauthenticated)
	 * public key for full authentication?
	 */
	const bool get_server_public_key;

	bool public_key_requested = false;

public:
	CachingSha2Password(std::string_view _server_public_key,
			    bool _get_server_public_key) noexcept
		:AuthHandler("caching_sha2_password"),
		 server_public_key(_server_public_key),
		 get_server_public_key(_get_server_public_key) {}

	~CachingSha2Password() noexcept override;

	// virtual methods from class AuthHandler
	std::span<const std::byte> GenerateResponse(std::string_view password,
						    std::span<const std::byte> password_sha1,
						    std::span<const std::byte> data1,
						    std::span<const std::byte> data2) override;
	std::span<const std::byte> HandlePacket(std::span<const std::byte> payload) override;
};

/**
 * Check whether the given PEM string is a public key which can be
 * used for "caching_sha2_password" full authentication.
 *
 * Throws on error.
 */
void
CheckCachingSha2PublicKey(std::string_view pem);

} // namespace Mysql
