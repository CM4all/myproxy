// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Ed25519.hxx"
#include "MysqlSerializer.hxx"
#include "lib/sodium/SHA512.hxx"
#include "util/SpanCast.hxx"

#include <sodium/crypto_sign_ed25519.h>
#include <sodium/crypto_scalarmult_ed25519.h>
#include <sodium/crypto_core_ed25519.h>

#include <stdexcept>

[[gnu::pure]]
static auto
SHA512(std::span<const std::byte> a, std::span<const std::byte> b) noexcept
{
	SHA512State state;
	state.Update(a);
	state.Update(b);
	return state.Final();
}

[[gnu::pure]]
static auto
SHA512(std::span<const std::byte> a, std::span<const std::byte> b,
       std::span<const std::byte> c) noexcept
{
	SHA512State state;
	state.Update(a);
	state.Update(b);
	state.Update(c);
	return state.Final();
}

[[gnu::pure]]
static std::array<std::byte, crypto_core_ed25519_SCALARBYTES>
crypto_core_ed25519_scalar_reduce(std::span<const std::byte, crypto_core_ed25519_NONREDUCEDSCALARBYTES> src) noexcept
{
	std::array<std::byte, crypto_core_ed25519_SCALARBYTES> result;
	crypto_core_ed25519_scalar_reduce(reinterpret_cast<unsigned char *>(result.data()),
					  reinterpret_cast<const unsigned char *>(src.data()));
	return result;
}

[[gnu::pure]]
static std::array<std::byte, crypto_core_ed25519_BYTES>
crypto_scalarmult_ed25519_base_noclamp(std::span<const std::byte, crypto_core_ed25519_SCALARBYTES> src) noexcept
{
	std::array<std::byte, crypto_core_ed25519_BYTES> result;
	crypto_scalarmult_ed25519_base_noclamp(reinterpret_cast<unsigned char *>(result.data()),
					       reinterpret_cast<const unsigned char *>(src.data()));
	return result;
}

[[gnu::pure]]
static std::array<std::byte, crypto_core_ed25519_SCALARBYTES>
crypto_core_ed25519_scalar_mul(std::span<const std::byte, crypto_core_ed25519_SCALARBYTES> a,
			       std::span<const std::byte, crypto_core_ed25519_SCALARBYTES> b) noexcept
{
	std::array<std::byte, crypto_core_ed25519_SCALARBYTES> result;
	crypto_core_ed25519_scalar_mul(reinterpret_cast<unsigned char *>(result.data()),
				       reinterpret_cast<const unsigned char *>(a.data()),
				       reinterpret_cast<const unsigned char *>(b.data()));
	return result;
}

[[gnu::pure]]
static std::array<std::byte, crypto_core_ed25519_SCALARBYTES>
crypto_core_ed25519_scalar_add(std::span<const std::byte, crypto_core_ed25519_SCALARBYTES> a,
			       std::span<const std::byte, crypto_core_ed25519_SCALARBYTES> b) noexcept
{
	std::array<std::byte, crypto_core_ed25519_SCALARBYTES> result;
	crypto_core_ed25519_scalar_add(reinterpret_cast<unsigned char *>(result.data()),
				       reinterpret_cast<const unsigned char *>(a.data()),
				       reinterpret_cast<const unsigned char *>(b.data()));
	return result;
}

[[gnu::pure]]
static std::array<std::byte, crypto_core_ed25519_SCALARBYTES>
crypto_core_ed25519_scalar_mul_add(std::span<const std::byte, crypto_core_ed25519_SCALARBYTES> a,
				   std::span<const std::byte, crypto_core_ed25519_SCALARBYTES> b,
				   std::span<const std::byte, crypto_core_ed25519_SCALARBYTES> c) noexcept
{
	const auto tmp = crypto_core_ed25519_scalar_mul(a, b);
	return crypto_core_ed25519_scalar_add(tmp, c);
}

namespace Mysql {

std::span<const std::byte>
ClientEd25519::GenerateResponse(std::string_view password,
				std::span<const std::byte> password_sha1,
				std::span<const std::byte> data1,
				std::span<const std::byte> data2)
{
	static_assert(sizeof(sig) == crypto_core_ed25519_SCALARBYTES * 2);

	if (password.empty() && !password_sha1.empty())
		throw std::invalid_argument{"Need clear-text password"};

	std::array<std::byte, 32> m;
	if (data1.size() + data2.size() != m.size())
		throw std::invalid_argument{"Malformed auth_plugin_data"};

	std::copy(data2.begin(), data2.end(),
		  std::copy(data1.begin(), data1.end(), m.begin()));

	auto az = SHA512(AsBytes(password));
	az[0] &= std::byte{248};
	az[31] &= std::byte{63};
	az[31] |= std::byte{64};

	const auto az_first = std::span{az}.first<32>();
	const auto az_second = std::span{az}.subspan<32>();

	const auto a = crypto_scalarmult_ed25519_base_noclamp(az_first);
	const auto nonce = crypto_core_ed25519_scalar_reduce(SHA512(az_second, m));
	const auto b = crypto_scalarmult_ed25519_base_noclamp(nonce);
	const auto hram = crypto_core_ed25519_scalar_reduce(SHA512(b, a, m));
	const auto c = crypto_core_ed25519_scalar_mul_add(hram, az_first, nonce);

	std::copy(c.begin(), c.end(), std::copy(b.begin(), b.end(), sig.begin()));
	return sig;
}

} // namespace Mysql
