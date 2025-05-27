// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/SpanCast.hxx"

#include <sha2.h>

#include <array>
#include <cstddef> // for std::byte
#include <span>

using SHA256Digest = std::array<std::byte, SHA256_DIGEST_LENGTH>;

class SHA256State {
	SHA2_CTX ctx;

public:
	SHA256State() noexcept {
		SHA256Init(&ctx);
	}

	auto &Update(std::span<const std::byte> src) noexcept {
		SHA256Update(&ctx, (const uint8_t *)src.data(), src.size());
		return *this;
	}

	auto &Update(std::string_view src) noexcept {
		return Update(AsBytes(src));
	}

	void Final(SHA256Digest &result) noexcept {
		SHA256Final((uint8_t *)result.data(), &ctx);
	}

	SHA256Digest Final() noexcept {
		SHA256Digest result;
		Final(result);
		return result;
	}
};

[[gnu::pure]]
inline auto
SHA256(const auto &src) noexcept
{
	return SHA256State{}.Update(src).Final();
}
