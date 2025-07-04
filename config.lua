server = mysql_resolve('127.33.0.6')

handler = {}

function handler.on_handshake_response(client, handshake_response)
  return client:connect(server, handshake_response)
end

function handler.on_init_db(client, database)
  return client:init_db(database)
end

mysql_listen(systemd, handler)
