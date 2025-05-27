// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Instance.hxx"
#include "net/ToString.hxx"
#include "time/Cast.hxx"
#include "util/PrintException.hxx"

#include <fmt/core.h>

std::string
Instance::OnPrometheusExporterRequest()
{
	auto s = fmt::format(R"(
# HELP myproxy_connections_accepted Number of accepted MySQL connections (including those that were rejected later)
# TYPE myproxy_connections_accepted counter

# HELP myproxy_connections_rejected Number of rejected MySQL connections
# TYPE myproxy_connections_rejected counter

# HELP myproxy_client_bytes_received Number of bytes received from clients
# TYPE myproxy_client_bytes_received counter

# HELP myproxy_client_packets_received Number of packets received from clients
# TYPE myproxy_client_packets_received counter

# HELP myproxy_client_malformed_packets Number of malformed packets received from clients
# TYPE myproxy_client_malformed_packets counter

# HELP myproxy_client_handshake_responses Number of handshake responses received from clients
# TYPE myproxy_client_handshake_responses counter

# HELP myproxy_client_auth_ok Number of successful authentications
# TYPE myproxy_client_auth_ok counter

# HELP myproxy_client_auth_err Number of failed authentication attempts
# TYPE myproxy_client_auth_err counter

# HELP myproxy_client_queries Number of queries received from clients
# TYPE myproxy_client_queries counter

# HELP myproxy_lua_errors Number of Lua errors
# TYPE myproxy_lua_errors counter

# HELP myproxy_server_state Monitoring state of the server
# TYPE myproxy_server_state gauge

# HELP myproxy_server_connects Number of connection attempts to this server
# TYPE myproxy_server_connects counter

# HELP myproxy_server_connect_errors Number of failed connection attempts to this server
# TYPE myproxy_server_connect_errors counter

# HELP myproxy_server_bytes_received Number of bytes received from this server
# TYPE myproxy_server_bytes_received counter

# HELP myproxy_server_packets_received Number of packets received from this server
# TYPE myproxy_server_packets_received counter

# HELP myproxy_server_malformed_packets Number of malformed packets received from this server
# TYPE myproxy_server_malformed_packets counter

# HELP myproxy_server_queries Number of queries sent to this server
# TYPE myproxy_server_queries counter

# HELP myproxy_server_query_errors Number of query warnigs received from this server
# TYPE myproxy_server_query_errors counter

# HELP myproxy_server_query_warnings Number of query warnings received from this server
# TYPE myproxy_server_query_warnings counter

# HELP myproxy_server_no_good_index_queries Number of queries which used no good index
# TYPE myproxy_server_no_good_index_queries counter

# HELP myproxy_server_no_index_queries Number of queries which used no index
# TYPE myproxy_server_no_index_queries counter

# HELP myproxy_server_slow_queries Number of slow queries
# TYPE myproxy_server_slow_queries counter

# HELP myproxy_server_affected_rows Number of affected rows by INSERT, UPDATE etc.
# TYPE myproxy_server_affected_rows counter

# HELP myproxy_server_query_wait Total wait time for query results
# TYPE myproxy_server_query_wait counter

myproxy_connections_accepted {}
myproxy_connections_rejected {}
myproxy_client_bytes_received {}
myproxy_client_packets_received {}
myproxy_client_malformed_packets {}
myproxy_client_handshake_responses {}
myproxy_client_auth_ok {}
myproxy_client_auth_err {}
myproxy_client_queries {}
myproxy_lua_errors {}
)",
			   stats.n_accepted_connections,
			   stats.n_rejected_connections,
			   stats.n_client_bytes_received,
			   stats.n_client_packets_received,
			   stats.n_client_malformed_packets,
			   stats.n_client_handshake_responses,
			   stats.n_client_auth_ok,
			   stats.n_client_auth_err,
			   stats.n_client_queries,
			   stats.n_lua_errors);

	for (const auto &[address, node] : stats.nodes) {
		const auto server = ToString(address);

		s += fmt::format(R"(
myproxy_server_connects{{server={:?}}} {}
myproxy_server_connect_errors{{server={:?}}} {}
myproxy_server_bytes_received{{server={:?}}} {}
myproxy_server_packets_received{{server={:?}}} {}
myproxy_server_malformed_packets{{server={:?}}} {}
myproxy_server_queries{{server={:?}}} {}
myproxy_server_query_errors{{server={:?}}} {}
myproxy_server_query_warnings{{server={:?}}} {}
myproxy_server_no_good_index_queries{{server={:?}}} {}
myproxy_server_no_index_queries{{server={:?}}} {}
myproxy_server_slow_queries{{server={:?}}} {}
myproxy_server_affected_rows{{server={:?}}} {}
myproxy_server_query_wait{{server={:?}}} {}
)",
				 server, node.n_connects,
				 server, node.n_connect_errors,
				 server, node.n_bytes_received,
				 server, node.n_packets_received,
				 server, node.n_malformed_packets,
				 server, node.n_queries,
				 server, node.n_query_errors,
				 server, node.n_query_warnings,
				 server, node.n_no_good_index_queries,
				 server, node.n_no_index_queries,
				 server, node.n_slow_queries,
				 server, node.n_affected_rows,
				 server, ToFloatSeconds(node.query_wait));

		if (node.state != nullptr)
			s += fmt::format("myproxy_server_state{{server={:?},state={:?}}} 1\n",
					 server, node.state);
	}

	return s;
}

void
Instance::OnPrometheusExporterError(std::exception_ptr error) noexcept
{
	PrintException(std::move(error));
}
