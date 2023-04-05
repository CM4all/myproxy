// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/ForeignFifoBuffer.hxx"

#include <stdint.h>

/**
 * A frontend for #SliceFifoBuffer which allows to replace it with a
 * simple heap-allocated buffer when some client code gets copied to
 * another project.
 */
class DefaultFifoBuffer : public ForeignFifoBuffer<std::byte> {
	static constexpr size_t SIZE = 32768;

public:
	DefaultFifoBuffer() noexcept:ForeignFifoBuffer(nullptr) {}

	bool IsDefinedAndFull() const noexcept {
		return IsDefined() && IsFull();
	}

	void Allocate() noexcept {
		SetBuffer(new std::byte[SIZE], SIZE);
	}

	void Free() noexcept {
		delete[] GetBuffer();
		SetNull();
	}

	void AllocateIfNull() noexcept {
		if (IsNull())
			Allocate();
	}

	void FreeIfDefined() noexcept {
		Free();
	}

	void FreeIfEmpty() noexcept {
		if (empty())
			Free();
	}

	void CycleIfEmpty() noexcept {
	}
};
