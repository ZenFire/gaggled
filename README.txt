.toc

# TODO

* Currently, the remote ZeroMQ code cannot handle situations where there are more than 1024 programs defined.  Fix!  This is really just a memory consumption issue, but the protocol must be changed to account for it.

# Purpose
**gaggled** is a process manager focused on inter-process dependencies within a system with an eye to developing cluster-level process management later (with equivalent semantics for process dependency).  It is currently in development and bug reports are welcome at https://github.com/ZenFire/gaggled if you find anything wrong.  Reports of working/not working status on various platforms are also welcomed and encouraged.

# Building

To build gaggled, you must create a build directory, run cmake, and use make to build.  How to use the rpm build setup and how to install are to be covered later. Watch this space.

    mkdir build
    cmake ..
    make -j4

You may be clever enough to just use **gaggled** right after building, but you're advised **for security reasons** to read this entire document, at the very least the Caveats section.

# Platforms

**gaggled** has been tested on a few platforms, and is expected to work on all relatively recent Linux distributions.

## Known to Work

* Centos 5
* Scientific Linux 6
* Debian Sid (9/29/2011) or Debian Squeeze with backported ZeroMQ 2.1 packages from Sid.

## Might Work, Untested

* FreeBSD
* OSX

## Not Expected to Work

* Any Windows systems (although this may work under Cygwin)

# Dependencies

* CMake 2.6 or later
* GCC 4.4 or later
* Boost 1.42.0 or later
* ZeroMQ 2.1 with C++ support. 2.0 does not work, but will build.

# Concepts / Configuration

## Global Settings

* The section labelled `gaggled` contains several settings, all of which are optional.  The section itself is optional.
 * `killwait`: milliseconds to wait after sending SIGTERM to shut down a process before assuming it won't die and using SIGKILL. Default: 10000.
 * `startwait`: milliseconds to defer trying to start a program again after an instance where **gaggled** noted that the program's dependencies were not yet satisfied. Default: 100.
 * `tick`: the event loop timer: how long to sleep after an event loop in which no events were processed. Default: 10.
 * `path`: to prepend to $PATH for searching for programs. Do not end with a : unless you want to add the current directory to the search path. Optional.
 * `env`: an optional environment section.  Overlays over the global environment that **gaggled** was run in.
 * `eventurl`: a ZeroMQ URL to bind to, to publish up/down events to.
 * `controlurl`: a ZeroMQ URL to bind to, to process control and state-dump requests from.

* A *program* is a settings section.  It can have any arbitrary title, which is the name of the *program*.
 * `command`: for instance, `/usr/bin/sleep`. If it contains no slashes, [path:]$PATH will be searched for the command.
 * `argv`: for instance, `8h`. Defaults to empty.
 * `wd`: chdir to this directory before starting this program. Optional; if not set, whatever working directory `gaggled` starts in will be used.
 * `env`: config section, overlays the list of assignments onto gaggled's env for the program.
 * `respawn`: `true` if it should be restarted if it dies.  This defaults to `true`.
 * `enabled`: `true` if it should be started, `false` if it's disabled.  Defaults to `true`.
* A *dependency* represents that a *program* will start, not start, stop, or restart depending on the state of another *program*. A program will start if and when all dependencies are satisfied.  This is expressed as a collection of settings; dependency sections should be under the `depends` section under the program section they are dependencies of.  The name of a dependency section is the name of the *program* the dependency is `on`.
 * `delay`: the program will not start until `on` has been running for `delay` milliseconds. Defaults to `0`.  Negative numbers or numbers over 2147483647 result in undefined behaviour.
 * `propagate`: if `on` restarts, `of` should restart as well.  Stop is initiated as soon as possible after `on` dies.  Starts as a result of this feature will obey `delay`.  Defaults to `false`.  Of course, if `on` has `respawn` turned off, `of` will not get started as `on` won't come back up.

Some examples of **gaggled** config files are in the contrib/ directory.

## Dependency Rules: Disallowed Configurations

There are some dependency configurations that are valid syntax, you may not express in the configuration of **gaggled**.  Doing so will result in **gaggled** failing to run or configest.

* dependency of an `enabled` *program* on a non-`enabled` program.
* missing any required setting
* circular dependencies: there can be no cycles in the dependency graph.  The graph is directed (`of` -> `on`), so it's possible to have a dependency set like (a -> b), (b -> c), (a -> c) which is not considered a cycle.  If you have one, then the programs in it would never start.

# Internals

## Stopping Programs

During **gaggled** shutdown and in other scenarios, sometimes a program must be shut down.  This will work the same way **init** does it: use `SIGTERM`, if the process does not die within 10 seconds, `SIGKILL` will be sent.

# Usage

* `-c` $FILE will use a specific config file.  This argument is required.
* `-t` will check dependency rules and existence of all programs but not start **gaggled**
* `-h` to display help instead of running.

# Listener, Controller, and SMTP Gate

There are some utility programs included with **gaggled**.

## Listener

The program **gaggled_listener** will connect to a gaggled instance that is configured with the `eventurl` option and display incoming program state changes messages as needed.  Use the -h option for help.

## Controller

The program **gaggled_controller** will connect to a gaggled instance that is configured with the `controlurl` option and print program state, shut down programs, or start them up.  Use the `-h` option for help.

## SMTP Gate

The program **gaggled_smtpgate** will connect to a gaggled instance that is configured with the `eventurl` and `controlurl` options and forward incoming program state changes messages via status emails to SMTP.  Other options are also required.  Use the -h option for help and required options.

The SMTP gateway must be restarted for each gaggled restart, as sequence numbers start over and the set of programs can change, resulting in undefined behaviour.  Shut down the SMTP gateway before shutting down gaggled, and start gaggled before starting the SMTP gateway.  The easiest and most reliable way to do this is run the SMTP gateway from within the gaggled it is connecting to.

# Caveats

* There is, at present, **no security** on the control or event channels. If you use `eventurl` or `controlurl`, transport restrictions such as firewalls, permissions on unix sockets, binding to localhost are the the only restriction on status information and up/down commands being interchanged.
* You must have a reliable system clock. startup sequences and SMTP notifications may not operate as expected if the system clock jumps forward or backward.  At the moment, timezone change/DST is untested.  Use UTC.  This is subject to change.
* using a large `startwait` or `tick` (compared to `delay` etc) can result in confusing behaviour (things taking longer to start or be recognized as stopped than expected)
* any program that uses the exit codes `EX_NOPERM`, `EX_DATAERR`, `EX_NOINPUT`, or `EX_UNAVAILABLE` (defined in sysexits.h for your system) may result in gaggled logging a specific type of execution failure that may not be accurate.
