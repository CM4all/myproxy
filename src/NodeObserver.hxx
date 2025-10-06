// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/IntrusiveList.hxx"

class Cluster;

/**
 * An interface which can be used to receive callbacks about the state
 * of a cluster node.
 */
class ClusterNodeObserver
	: public IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK, Cluster>
{
public:
	/**
	 * The node is now unavailable.  Prior to calling this, the
	 * observer is removed from the observer list, i.e. it will
	 * never be called again.
	 */
	virtual void OnClusterNodeUnavailable() noexcept = 0;

protected:
	void UnregisterClusterNodeObserver() noexcept {
		if (is_linked())
			unlink();
	}
};
