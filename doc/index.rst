myproxy
=======

What is myproxy?
----------------

myproxy is a proxy server for MySQL.  It is configured and customized
with a Lua script.


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

- ``on_init_db(client, database_name)`` decides what to do with an
  ``INIT_DB`` packet.  The function can then:

  - ``return client:init_db(database_name)`` to forward the
    ``INIT_DB`` packet to the server (possibly with a different
    database name).
  - ``return client:err("Error message")`` to reject the ``INIT_DB``
    request.

It is important that callback functions finish quickly.  They must
never block, because this would block the whole daemon process.  This
means they must not do any network I/O, launch child processes, and
should avoid anything but querying the parameters.


Global Variables
^^^^^^^^^^^^^^^^

- ``populate_io_buffers``: ``true`` populates all I/O buffers on
  startup.  This reduces waits for Linux kernel VM
  compaction/migration.


Control Listener
----------------

The ``control_listen()`` function creates a listener for control
datagrams that can be used to control certain behavior at runtime::

 control_listen(address [, options])

The ``address`` parameter specifies the socket address to bind to.
May be the wildcard ``*`` or an IPv4/IPv6 address followed by a
port. IPv6 addresses should be enclosed in square brackets to
disambiguate the port separator. Local sockets start with a slash
:file:`/`, and abstract sockets start with the symbol ``@``.

The second parameter is an optional table that has more options:

- ``multicast_group``: join this multicast group, which allows
  receiving multicast commands. Value is a multicast IPv4/IPv6
  address.  IPv6 addresses may contain a scope identifier after a
  percent sign (``%``).

- ``interface``: limit this listener to the given network interface.

Example::

 control_listen('@myproxy-control')
 control_listen('*', {multicast_group='224.0.0.42', interface='eth1'})
 control_listen('127.0.0.1:1234')

The control command ``DISCONNECT_DATABASE`` disconnects all
connections of the account specified in the payload.


Prometheus Exporter
^^^^^^^^^^^^^^^^^^^

The function ``prometheus_listen(ADDRESS)`` creates a simple HTTP
listener which exposes statistics in the `Prometheus format
<https://prometheus.io/docs/instrumenting/writing_exporters/>`__.
Example::

 prometheus_listen("*:8022")
 prometheus_listen("/run/cm4all/myproxy/prometheus_exporter.socket")


``SIGHUP``
^^^^^^^^^^

On ``systemctl reload cm4all-myproxy`` (i.e. ``SIGHUP``), myproxy
calls the Lua function ``reload`` if one was defined.  It is up to the
Lua script to define the exact meaning of this feature.


Inspecting Client Connections
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following attributes of the ``client`` parameter can be queried:

* :samp:`pid`: The client's process id.

* :samp:`uid`: The client's user id.

* :samp:`gid`: The client's group id.

* :samp:`account`: Set this to an identifier of the user account.
  This will be used in the log prefix and for choosing a cluster node.

* :samp:`cgroup`: The control group of the client process with the
  following attributes:

  * ``path``: the cgroup path as noted in :file:`/proc/self/cgroup`,
    e.g. :file:`/user.slice/user-1000.slice/session-42.scope`

  * ``xattr``: A table containing extended attributes of the
    control group.

  * ``parent``: Information about the parent of this cgroup; it is
    another object of this type (or ``nil`` if there is no parent
    cgroup).

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

* ``client:connect(address, handshake_response, [options])`` connects
  to the specified address and proxies all queries to it.  Parameters:

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

  - ``options``: an optional third parameter is a table of options:

    - ``read_only``: if ``true``, then servers which are read-only
      will be preferred (depends on cluster options ``monitoring`` and
      ``user`` / ``password``).

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

- ``disconnect_unavailable``: if ``true`` and a node becomes
  unavailable through monitoring, then all proxied connections to that
  node will be closed (if a node exists that is available)

When using such a cluster with ``client:connect()``, myproxy will
automatically choose a node using consistent hashing with the
``client.account`` attribute.


socket
^^^^^^

A simple low-level networking library.  Example::

  tcp = socket:connect('localhost:1234')
  udp = socket:connect('localhost:4321', {type='dgram'})
  multicast = socket:connect('[ff02::dead:beef]:2345', {type='dgram'})
  unix = socket:connect('/run/test.socket')
  abstract = socket:connect('@test', {type='seqpacket'})
  abstract:send('hello world')

The ``socket`` library has the following methods:

- ``connect(ADDRESS, [OPTIONS])``: Create a new socket connected to
  the specified address.  ``OPTIONS`` may be a table with the
  following keys:

  - ``type``: the socket type, one of ``stream`` (the default),
    ``dgram``, ``seqpacket``.

  Returns a new socket object on success or ``[nil,error]`` on error.

Socket objects have the following methods:

- ``close()``: Close the socket.

- ``send(DATA, [START], [END])``: Send data (i.e. a string) to the
  peer.  ``START`` and ``END`` are start and end position within the
  string with the same semantics as in ``string.sub()``.  Returns the
  number of bytes sent on success or ``[nil,error]`` on error.


control_client
^^^^^^^^^^^^^^

A client for the `beng-proxy control protocol
<https://beng-proxy.readthedocs.io/en/latest/control.html>`__.

During startup, create a ``control_client`` object::

  -- IPv4 (default port)
  c = control_client:new('224.0.0.42')

  -- IPv6 on default port
  c = control_client:new('ff02::dead:beef')

  -- IPv6 on non-default port (requires square brackets)
  c = control_client:new('[ff02::dead:beef]:1234')

  -- local socket
  c = control_client:new('/run/cm4all/workshop/control')

  -- abstract socket
  c = control_client:new('@bp-control')

The ``new()`` constructor returns ``nil,error`` on error (and thus the
call can be wrapped in ``assert()`` to raise a Lua error instead).

The method ``build()`` creates an object which can be used to build a
control datagram with one or more commands.  After that datagram has
been assembled, it can be sent with the ``send()`` method.  Example::

  c:send(c:build():fade_children('foo'):flush_http_cache('bar'))

The ``send()`` method returns ``nil,error`` on error.

The builder implements the following methods:

- ``cancel_job(PARTITION_NAME, JOB_ID)``
- ``discard_session(ID)``
- ``disconnect_database(ACCOUNT)``
- ``fade_children(TAG)``
- ``flush_filter_cache(TAG)``
- ``flush_http_cache(TAG)``
- ``reject_client(ADDRESS)``
- ``reset_limiter(ACCOUNT)``
- ``tarpit_client(ADDRESS)``
- ``terminate_children(TAG)``


libsodium
^^^^^^^^^

There are some `libsodium <https://www.libsodium.org/>`__ bindings.

`Helpers <https://doc.libsodium.org/helpers>`__::

  bin = sodium.hex2bin("deadbeef") -- returns "\xde\xad\xbe\xef"
  hex = sodium.bin2hex("A\0\xff") -- returns "4100ff"

`Generating random data
<https://doc.libsodium.org/generating_random_data>`__::

  key = sodium.randombytes(32)

`Sealed boxes
<https://libsodium.gitbook.io/doc/public-key_cryptography/sealed_boxes>`__::

  pk, sk = sodium.crypto_box_keypair()
  ciphertext = sodium.crypto_box_seal('hello world', pk)
  message = sodium.crypto_box_seal_open(ciphertext, pk, sk)

`Point*scalar multiplication
<https://doc.libsodium.org/advanced/scalar_multiplication>__::

  pk = sodium.crypto_scalarmult_base(sk)


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

To listen for `PostgreSQL notifications
<https://www.postgresql.org/docs/current/sql-notify.html>`__, invoke
the ``listen`` method with a callback function::

  db:listen('bar', function()
    print("Received a PostgreSQL NOTIFY")
  end)


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
