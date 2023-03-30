// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "MysqlProtocol.hxx"
#include "util/SpanCast.hxx"

#include <algorithm> // for std::copy()
#include <array>
#include <cstddef>
#include <span>

namespace Mysql {

struct PacketTooLarge {};

class PacketSerializer {
	std::array<std::byte, 1024> buffer;

	std::size_t position = 0;

public:
	explicit PacketSerializer(uint8_t sequence_id) noexcept {
		auto &header = WriteT<PacketHeader>();
		header.number = sequence_id;
	}

	std::span<std::byte> WriteN(std::size_t size) {
		std::size_t max_size = buffer.size() - position;
		if (size > max_size)
			throw PacketTooLarge{};

		const auto result = std::span{buffer}.subspan(position, size);
		position += size;
		return result;
	}

	/**
	 * Write a number of zero bytes.
	 */
	void WriteZero(std::size_t size) {
		auto s = WriteN(size);
		std::fill(s.begin(), s.end(), std::byte{});
	}

	void WriteN(std::span<const std::byte> src) {
		auto dest = WriteN(src.size());
		std::copy(src.begin(), src.end(), dest.begin());
	}

	template<typename T>
	T &WriteT() {
		return *reinterpret_cast<T *>(WriteN(sizeof(T)).data());
	}

	template<typename T>
	void WriteT(const T &src) {
		WriteT<T>() = src;
	}

	void WriteInt1(uint8_t value) {
		WriteT(value);
	}

	void WriteInt2(uint_least16_t value) {
		WriteT<Int2>(value);
	}

	void WriteInt3(uint_least32_t value) {
		WriteT<Int3>(value);
	}

	void WriteInt4(uint_least32_t value) {
		WriteT<Int4>(value);
	}

	void WriteInt6(uint_least64_t value) {
		WriteT<Int6>(value);
	}

	void WriteInt8(uint_least64_t value) {
		WriteT<Int8>(value);
	}

	void WriteLengthEncodedInteger(uint_least64_t value) {
		if (value < 251) {
			WriteInt1(value);
		} else if (value < uint_least64_t{1} << 16) {
			WriteInt1(0xfc);
			WriteInt2(value);
		} else if (value < uint_least64_t{1} << 24) {
			WriteInt1(0xfd);
			WriteInt3(value);
		} else {
			WriteInt1(0xfe);
			WriteInt8(value);
		}
	}

	void WriteNullTerminatedString(std::string_view src) {
		WriteN(AsBytes(src));
		WriteInt1(0);
	}

	void WriteVariableLengthString(std::string_view src) {
		WriteN(AsBytes(src));
	}

	void WriteLengthEncodedString(std::string_view src) {
		WriteLengthEncodedInteger(src.size());
		WriteVariableLengthString(src);
	}

	std::span<const std::byte> Finish() {
		auto &header = *reinterpret_cast<PacketHeader *>(buffer.data());
		header.length = position - sizeof(header);

		return std::span{buffer}.first(position);
	}
};

} // namespace Mysql
