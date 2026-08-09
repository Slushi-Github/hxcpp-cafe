# hxcpp with Wii U support...?

This fork of hxcpp (Based on commit ``???`` (I really don't remember)) based on [a private version of my hxcpp fork for the Nintendo Switch](https://github.com/Slushi-Github/hxcpp-nx) has all the modifications needed to compile Haxe for the Nintendo Wii U.

It compiles and runs, but needs more testing...

The hxcpp TLS and threads are really changed in this fork, and this fork has changes to only the Wii U, not the Switch, so you can only use this fork for the Wii U if you want compile to a console... For now.

# Problems:

- Missing SSL, WUT don't let you get the certificates for use with hxcpp for enable SSL support.

- Socket untested and aparently partially supported, but not fully functional I think. IPV6 stuff and ``_hx_std_host_local`` in ``src/hx/libs/std/Socket.cpp`` (perhaps related to ``sys.net.Host.localhost``?) are not supported. (maybe similar issue with SQLite?) (Same issue with hxcpp-nx)