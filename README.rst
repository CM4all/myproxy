myproxy
=======

*myproxy* is a proxy for MySQL.  It is configured and customized with
a Lua script.

For more information, `read the manual
<https://myproxy.readthedocs.io/en/latest/>`__ in the `doc` directory.


Building myproxy
----------------

You need:

- a C++20 compliant compiler (e.g. GCC or clang)
- `Meson 1.0 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__
- `libfmt <https://fmt.dev/>`__
- `libmd <https://www.hadrons.org/software/libmd/>`__
- `libsodium <https://www.libsodium.org/>`__
- `LuaJIT <http://luajit.org/>`__

Optional dependencies:

- `libpq <https://www.postgresql.org/>`__ for PostgreSQL support in
  Lua code
- `nlohmann_json <https://json.nlohmann.me/>`__ (for the
  systemd-resolved client)
- `OpenSSL <https://www.openssl.org/>`__ for ``caching_sha2_password``
  support
- `systemd <https://www.freedesktop.org/wiki/Software/systemd/>`__

Get the source code::

 git clone --recursive https://github.com/CM4all/myproxy.git

Run ``meson``::

 meson setup output

Compile and install::

 ninja -C output
 ninja -C output install
