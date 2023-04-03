server = mysql_resolve('127.33.0.6')

mysql_listen(systemd, function(client, handshake_response)
  return client:connect(server, handshake_response)
end)
