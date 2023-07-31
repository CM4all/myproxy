myproxy
=======

What is myproxy?
----------------

myproxy is a proxy server for MySQL.


Configuration
-------------

.. highlight:: lua

The file :file:`/etc/cm4all/myproxy/config.lua` is a `Lua
<http://www.lua.org/>`_ script which is executed at startup.  It
contains at least one :samp:`mysql_listen()` call, for example::

 handler = {}

 function handler.on_connect(client)
 end

 function handler.on_handshake_response(client, handshake_response)
    return m:connect('192.168.1.99', handshake_response)
 end

 mysql_listen('/run/cm4all/myproxy/myproxy.sock', handler)

The first parameter is the socket path to listen on.  Passing the
global variable :envvar:`systemd` (not the string literal
:samp:`"systemd"`) will listen on the sockets passed by systemd::

  mysql_listen(systemd, handler)

To use this socket from within a container, move it to a dedicated
directory and bind-mount this directory into the container.  Mounting
just the socket doesn't work because a daemon restart must create a
new socket, but the bind mount cannot be refreshed.

The second parameter is a table containing callback functions:

- ``on_connect(client)`` is invoked as soon as a client connects.
  This method may collect information about this client in the
  ``notes`` table or change the ``server_version`` attribute.

  To reject the client, return ``client:err("error message")``.

- ``on_handshake_response(client, handshake_response)`` decides what
  to do with a login attempt by a client.  This function receives a
  client object which can be inspected and the contents of the
  ``HandshakeResponse`` packet received from the client.

- ``on_command_phase(client)`` is invoked after successful login.

It is important that callback functions finish quickly.  They must
never block, because this would block the whole daemon process.  This
means they must not do any network I/O, launch child processes, and
should avoid anything but querying the parameters.


Inspecting Client Connections
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following attributes of the ``client`` parameter can be queried:

* :samp:`pid`: The client's process id.

* :samp:`uid`: The client's user id.

* :samp:`gid`: The client's group id.

* :samp:`account`: Set this to an identifier of the user account.
  This will be used in the log prefix and for choosing a cluster node.

* :samp:`cgroup`: The control group path of the client process as
  noted in :file:`/proc/self/cgroup`,
  e.g. :file:`/user.slice/user-1000.slice/session-42.scope`

* :samp:`server_version`: The server version string.  In
  ``on_connect``, this attribute may be modified to announce a
  different version to the client.  After a connection to the real
  server has been established, this attribute contains the version
  announced by that server.

* :samp:`notes`: a table where the Lua script can add arbitrary
  entries


Login Callback Actions
^^^^^^^^^^^^^^^^^^^^^^

The login callback (i.e. ``HandshakeResponse``) can return one of
these actions:

* ``client:connect(address, handshake_response)`` connects to the
  specified address and proxies all queries to it.  Parameters:

  - ``address``: a ``SocketAddress`` or a ``mysql_cluster`` object.

  - ``handshake_response``: a table containing the keys ``user``,
    ``password`` and ``database``.  The ``handshake_response``
    parameter passed to the callback function can be used here (the
    function is allowed to modify it).

    Instead of ``password``, ``password_sha1`` can be set to a string
    containing the SHA1 digest (20 bytes, raw, not hex).  This
    requires a server which supports ``mysql_native_password``, and
    works because that authentication method does not require knowing
    the cleartext password, only its SHA1 digest.

* ``client:err("Error message")`` fails the handshake with the
  specified message.


Addresses
^^^^^^^^^

It is recommended to create all ``SocketAddress`` objects during
startup, to avoid putting unnecessary pressure on the Lua garbage
collector, and to reduce the overhead for invoking the system resolver
(which blocks myproxy execution).  The function `mysql_resolve()`
creates such an `address` object::

  server1 = mysql_resolve('192.168.0.2')
  server2 = mysql_resolve('[::1]:4321')
  server3 = mysql_resolve('server1.local:1234')
  server4 = mysql_resolve('/run/server5.sock')
  server5 = mysql_resolve('@server4')

These examples do the following:

- convert a numeric IPv4 address to a ``SocketAddress`` object (port
  defaults to 3306, the MySQL standard port)
- convert a numeric IPv6 address with a non-standard port to an
  ``SocketAddress`` object
- invoke the system resolver to resolve a host name to an IP address
  (which blocks myproxy startup; not recommended)
- convert a path string to a "local" socket address
- convert a name to an abstract "local" socket address (prefix ``@``
  is converted to a null byte, making the address "abstract")

If you have a cluster of replicated MySQL servers, you can construct
it with ``mysql_cluster()``, passing an array of addresses to it::

  cluster = mysql_cluster({
    '192.168.0.2',
    '192.168.0.3',
    mysql_resolve('server1.local:1234'),
  })

An optional second parameter is a table of options:

- ``monitoring``: if ``true``, then myproxy will peridiocally connect
  to all servers to see whether they are available; failing servers
  will be excluded

- ``user`` and ``password``: if monitoring is enabled, try to log in
  with these credentials

- ``no_read_only``: if ``true``, then servers which are not read-only
  will be preferred; set this option if you want myproxy to select the
  active master instance automatically (depends on ``monitoring`` and
  ``user`` / ``password``)

When using such a cluster with ``client:connect()``, myproxy will
automatically choose a node using consistent hashing with the
``client.account`` attribute.


libsodium
^^^^^^^^^

There are some `libsodium <https://www.libsodium.org/>`__ bindings.

`Sealed boxes
<https://libsodium.gitbook.io/doc/public-key_cryptography/sealed_boxes>`__::

  pk, sk = sodium.crypto_box_keypair()
  ciphertext = sodium.crypto_box_seal('hello world', pk)
  message = sodium.crypto_box_seal_open(ciphertext, pk, sk)


PostgreSQL Client
^^^^^^^^^^^^^^^^^

The Lua script can query a PostgreSQL database.  First, a connection
should be established during initialization::

  db = pg:new('dbname=foo', 'schemaname')

In the handler function, queries can be executed like this (the API is
similar to `LuaSQL <https://keplerproject.github.io/luasql/>`__)::

  local result = assert(db:execute('SELECT id, name FROM bar'))
  local row = result:fetch({}, "a")
  print(row.id, row.name)

Query parameters are passed to ``db:execute()`` as an array after the
SQL string::

  local result = assert(
    db:execute('SELECT name FROM bar WHERE id=$1', {42}))

The functions ``pg:encode_array()`` and ``pg:decode_array()`` support
PostgreSQL arrays; the former encodes a Lua array to a PostgreSQL
array string, and the latter decodes a PostgreSQL array string to a
Lua array.


Examples
^^^^^^^^

TODO


About Lua
^^^^^^^^^

`Programming in Lua <https://www.lua.org/pil/1.html>`_ (a tutorial
book), `Lua 5.3 Reference Manual <https://www.lua.org/manual/5.3/>`_.

Note that in Lua, attributes are referenced with a dot
(e.g. :samp:`client.pid`), but methods are referenced with a colon
(e.g. :samp:`client:err()`).
