// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Handler.hxx"

namespace Mysql {

class ClearPassword final : public AuthHandler {
public:
	constexpr ClearPassword() noexcept
		:AuthHandler("mysql_clear_password") {}

	// virtual methods from class AuthHandler
	std::span<const std::byte> GenerateResponse(std::string_view password,
						    std::span<const std::byte> password_sha1,
						    std::span<const std::byte> data1,
						    std::span<const std::byte> data2) override;
};

} // namespace Mysql
