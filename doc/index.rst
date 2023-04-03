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

  mysql_listen('/run/cm4all/myproxy/myproxy.sock', function(client, handshake_response)
    return m:connect('192.168.1.99', handshake_response)
  end)

The first parameter is the socket path to listen on.  Passing the
global variable :envvar:`systemd` (not the string literal
:samp:`"systemd"`) will listen on the sockets passed by systemd::

  mysql_listen(systemd, function(client, handshake_response) ...

To use this socket from within a container, move it to a dedicated
directory and bind-mount this directory into the container.  Mounting
just the socket doesn't work because a daemon restart must create a
new socket, but the bind mount cannot be refreshed.

The second parameter is a callback function which shall decide what to
do with a login attempt by a client.  This function receives a client
object which can be inspected and the contents of the
``HandshakeResponse`` packet received from the client.

It is important that the function finishes quickly.  It must never
block, because this would block the whole daemon process.  This means
it must not do any network I/O, launch child processes, and should
avoid anything but querying the parameters.


Inspecting Client Connections
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following attributes of the ``client`` parameter can be queried:

* :samp:`pid`: The client's process id.

* :samp:`uid`: The client's user id.

* :samp:`gid`: The client's group id.

* :samp:`cgroup`: The control group path of the client process as
  noted in :file:`/proc/self/cgroup`,
  e.g. :file:`/user.slice/user-1000.slice/session-42.scope`


Login Callback Actions
^^^^^^^^^^^^^^^^^^^^^^

The login callback (i.e. ``HandshakeResponse``) can return one of
these actions:

* ``client:connect(address, handshake_response)`` connects to the
  specified address and proxies all queries to it.  Parameters:

  - ``address``: a ``SocketAddress`` object.

  - ``handshake_response``: a table containing the keys ``username``,
    ``password`` and ``database``.  The ``handshake_response``
    parameter passed to the callback function can be used here (the
    function is allowed to modify it).

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