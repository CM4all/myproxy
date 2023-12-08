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
};

} // namespace Mysql
