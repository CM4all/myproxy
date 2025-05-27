// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CachingSha2Password.hxx"
#include "SHA256.hxx"
#include "lib/openssl/Error.hxx"
#include "lib/openssl/UniqueBIO.hxx"
#include "lib/openssl/UniqueEVP.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/SpanCast.hxx"

#include <openssl/pem.h>

#include <stdexcept>

namespace Mysql {

CachingSha2Password::~CachingSha2Password() noexcept
{
	// erase clear-text password from memory
	std::fill(last_password.begin(), last_password.end(), '\0');
}

std::span<const std::byte>
CachingSha2Password::GenerateResponse(std::string_view password,
				      std::span<const std::byte> password_sha1,
				      std::span<const std::byte> data1,
				      std::span<const std::byte> data2)
{
	static_assert(sizeof(buffer) == SHA256_DIGEST_LENGTH);

	if (password.empty() && !password_sha1.empty())
		throw std::invalid_argument{"Need clear-text password"};

	if (auto &last = data2.empty() ? data1 : data2;
	    data1.size() + data2.size() != 21 || last.back() != std::byte{})
		// null byte is missing
		throw std::invalid_argument{"Malformed auth_plugin_data"};
	else
		// strip the null byte
		last = last.first(last.size() - 1);

	const auto password_sha256 = SHA256(password);
	const auto password_sha256_sha256 = SHA256(password_sha256);

	SHA256State state;
	state.Update(password_sha256_sha256);
	state.Update(data1);
	state.Update(data2);
	state.Final(buffer);

	for (std::size_t i = 0; i < buffer.size(); ++i)
		buffer[i] ^= password_sha256[i];

	/* remember the clear-text password, just in case we need it
	   for the slow path */
	last_password = password;

	/* remember the auth_plugin_data for the same reason (only the
	   first 20 bytes, without the trailing null byte */
	if (data1.size() >= last_auth_plugin_data.size()) {
		std::copy_n(data1.begin(), last_auth_plugin_data.size(),
			    last_auth_plugin_data.begin());
	} else {
		auto i = std::copy(data1.begin(), data1.end(),
				   last_auth_plugin_data.begin());
		std::copy_n(data2.begin(),
			    last_auth_plugin_data.size() - data1.size(),
			    i);
	}

	return buffer;
}

static UniqueEVP_PKEY
ParsePublicKey(std::span<const std::byte> pem)
{
	UniqueBIO bio{BIO_new_mem_buf(pem.data(), pem.size())};
	EVP_PKEY *pkey = PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr);
	if (pkey == nullptr)
		throw SslError{"PEM_read_bio_PUBKEY() failed"};

	return UniqueEVP_PKEY{pkey};
}

static AllocatedArray<std::byte>
Encrypt(EVP_PKEY &key, std::span<const std::byte> src)
{
	const std::size_t key_size = (std::size_t)EVP_PKEY_size(&key);
	if (src.size() + 40 >= key_size)
		throw std::invalid_argument{"Password is too long"};

	AllocatedArray<std::byte> result{key_size};

	UniqueEVP_PKEY_CTX ctx{EVP_PKEY_CTX_new(&key, nullptr)};
	if (!ctx)
		throw SslError{"EVP_PKEY_CTX_new() failed"};

	if (!EVP_PKEY_encrypt_init(ctx.get()))
		throw SslError{"EVP_PKEY_encrypt_init() failed"};

	EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING);

	std::size_t result_size = result.size();
	if (EVP_PKEY_encrypt(ctx.get(),
			     reinterpret_cast<unsigned char *>(result.data()),
			     &result_size,
			     reinterpret_cast<const unsigned char *>(src.data()),
			     src.size()) <= 0)
		throw SslError{"EVP_PKEY_encrypt() failed"};

	result.SetSize(result_size);
	return result;
}

/**
 * Generate the bit-wise XOR of the clear-text password and the
 * auth_plugin_data into a newly allocated buffer.
 */
static AllocatedArray<std::byte>
XorPassword(std::string_view password,
	    std::span<const std::byte, 20> auth_plugin_data) noexcept
{
	AllocatedArray<std::byte> result{password.size() + 1};

	for (std::size_t i = 0; i < password.size(); ++i)
		result[i] = static_cast<std::byte>(password[i]) ^ auth_plugin_data[i % auth_plugin_data.size()];

	result[password.size()] = auth_plugin_data[password.size() % auth_plugin_data.size()];
	return result;
}

static AllocatedArray<std::byte>
MakeEncryptedPassword(std::string_view password,
		      std::span<const std::byte, 20> auth_plugin_data,
		      std::span<const std::byte> public_key_pem)
{
	const auto public_key = ParsePublicKey(public_key_pem);
	const auto xor_password = XorPassword(password, auth_plugin_data);

	return Encrypt(*public_key, xor_password);
}

std::span<const std::byte>
CachingSha2Password::HandlePacket(std::span<const std::byte> payload)
{
	if (payload.empty() || payload.front() != std::byte{0x01})
		return {};

	if (public_key_requested) {
		/* this payload contains the server's
		   public key */

		public_key_requested = false;

		encrypted_password = MakeEncryptedPassword(last_password,
							   last_auth_plugin_data,
							   payload.subspan(1));
		return encrypted_password;
	}

	/* this is "fast auth result" which is the first response
	   packet after "caching_sha2_password" */
	if (payload.size() != 2)
		throw SocketProtocolError{"Bad fast auth result packet from server"};

	const auto result = payload[1];

	if (result == std::byte{3}) {
		/* fast auth success - ignore this one and forward the
		   "OK" packet that will follow */
		static constexpr std::byte dummy{};
		return {&dummy, 0};
	} else if (result == std::byte{4}) {
		/* fast auth failed - request the server public key;
		   once we receive it, we can encrypt the clear-text
		   password with it */
		// TODO cache the server public key

		public_key_requested = true;
		static constexpr std::array request_public_key{std::byte{0x02}};
		return request_public_key;
	} else
		throw SocketProtocolError{"Bad fast auth result code from server"};
}

} // namespace Mysql
