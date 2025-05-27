// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "MysqlProtocol.hxx"
#include "util/SpanCast.hxx"

#include <algorithm> // for std::find()
#include <span>

namespace Mysql {

/**
 * Exception class that gets thrown when #PacketDeserializer fails.
 */
struct MalformedPacket {};

class PacketDeserializer {
	std::span<const std::byte> payload;

public:
	constexpr PacketDeserializer(std::span<const std::byte> _payload) noexcept
		:payload(_payload) {}

	bool empty() const noexcept {
		return payload.empty();
	}

	void MustBeEmpty() const {
		if (!empty())
			throw MalformedPacket{};
	}

	std::span<const std::byte> ReadN(std::size_t size) {
		if (payload.size() < size)
			throw MalformedPacket{};

		const auto result = payload.first(size);
		payload = payload.subspan(size);
		return result;
	}

	template<typename T>
	const T &ReadT() {
		const auto s = ReadN(sizeof(T));
		return *reinterpret_cast<const T *>(s.data());
	}

	uint_least8_t ReadInt1() {
		return ReadT<uint8_t>();
	}

	uint_least16_t ReadInt2() {
		return ReadT<Int2>();
	}

	uint_least32_t ReadInt3() {
		return ReadT<Int3>();
	}

	uint_least32_t ReadInt4() {
		return ReadT<Int4>();
	}

	uint_least64_t ReadInt6() {
		return ReadT<Int6>();
	}

	uint_least64_t ReadInt8() {
		return ReadT<Int8>();
	}

	uint_least64_t ReadLengthEncodedInteger() {
		const auto first = ReadInt1();
		switch (first) {
		case 0xfc:
			return ReadInt2();

		case 0xfd:
			return ReadInt3();

		case 0xfe:
			return ReadInt8();
		}

		if (first >= 251)
			throw MalformedPacket{};

		return first;
	}

	std::string_view ReadNullTerminatedString() {
		auto nul = std::find(payload.begin(), payload.end(), std::byte{0});
		if (nul == payload.end())
			throw MalformedPacket{};

		const auto result = ToStringView(std::span{payload.begin(), nul});
		payload = {std::next(nul), payload.end()};
		return result;
	}

	std::string_view ReadVariableLengthString(std::size_t length) {
		if (length > payload.size())
			throw MalformedPacket{};

		const auto result = ToStringView(payload.first(length));
		payload = payload.subspan(length);
		return result;
	}

	std::string_view ReadLengthEncodedString() {
		return ReadVariableLengthString(ReadLengthEncodedInteger());
	}

	std::string_view ReadRestOfPacketString() noexcept {
		const auto result = ToStringView(payload);
		payload = {};
		return result;
	}
};

} // namespace Mysql
