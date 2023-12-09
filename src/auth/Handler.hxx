// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace Mysql {

class PacketSerializer;

class AuthHandler {
	std::string_view name;

protected:
	explicit constexpr AuthHandler(std::string_view _name) noexcept
		:name(_name) {}

public:
	virtual ~AuthHandler() noexcept = default;

	constexpr std::string_view GetName() const noexcept {
		return name;
	}

	virtual std::span<const std::byte> GenerateResponse(std::string_view password,
							    std::span<const std::byte> password_sha1,
							    std::span<const std::byte> data1,
							    std::span<const std::byte> data2) = 0;

	/**
	 * Give this object a chance to handle a raw packet during
	 * authentication.
	 *
	 * @return nullptr if the packet was not handled, a non-empty
	 * span if a packet shall be sent to the server, an empty
	 * (non-nullptr) span if the packet was handled by nothing
	 * should be sent to the server
	 */
	virtual std::span<const std::byte> HandlePacket(std::span<const std::byte> payload) {
		(void)payload;
		return {};
	}
};

} // namespace Mysql
