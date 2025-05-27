// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/SpanCast.hxx"

#include <sha1.h>

#include <array>
#include <cstddef> // for std::byte
#include <span>

using SHA1Digest = std::array<std::byte, SHA1_DIGEST_LENGTH>;

class SHA1State {
	SHA1_CTX ctx;

public:
	SHA1State() noexcept {
		SHA1Init(&ctx);
	}

	auto &Update(std::span<const std::byte> src) noexcept {
		SHA1Update(&ctx, (const uint8_t *)src.data(), src.size());
		return *this;
	}

	auto &Update(std::string_view src) noexcept {
		return Update(AsBytes(src));
	}

	void Final(SHA1Digest &result) noexcept {
		SHA1Final((uint8_t *)result.data(), &ctx);
	}

	SHA1Digest Final() noexcept {
		SHA1Digest result;
		Final(result);
		return result;
	}
};

[[gnu::pure]]
inline auto
SHA1(const auto &src) noexcept
{
	return SHA1State{}.Update(src).Final();
}
