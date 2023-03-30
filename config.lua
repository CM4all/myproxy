server = mysql_resolve('127.33.0.6')

mysql_listen('/run/cm4all/myrelay/socket', function(connection, handshake_response)
  return server
end)
