
**<a href="#toc1-2">TODO</a>**

**<a href="#toc1-7">Purpose</a>**

**<a href="#toc1-11">Building</a>**

**<a href="#toc1-22">Platforms</a>**
&emsp;<a href="#toc2-27">Known to Work</a>
&emsp;<a href="#toc2-34">Might Work, Untested</a>
&emsp;<a href="#toc2-40">Not Expected to Work</a>

**<a href="#toc1-45">Dependencies</a>**

**<a href="#toc1-53">Concepts / Configuration</a>**
&emsp;<a href="#toc2-56">Global Settings</a>
&emsp;<a href="#toc2-81">Dependency Rules: Disallowed Configurations</a>

**<a href="#toc1-90">Internals</a>**
&emsp;<a href="#toc2-93">Stopping Programs</a>

**<a href="#toc1-98">Usage</a>**

**<a href="#toc1-105">Listener, Controller, and SMTP Gate</a>**
&emsp;<a href="#toc2-110">Listener</a>
&emsp;<a href="#toc2-115">Controller</a>
&emsp;<a href="#toc2-120">SMTP Gate</a>

**<a href="#toc1-127">Caveats</a>**

<A name="toc1-2" title="TODO" />
# TODO

* Currently, the remote ZeroMQ code cannot handle situations where there are more than 1024 programs defined.  Fix!  This is really just a memory consumption issue, but the protocol must be changed to account for it.

<A name="toc1-7" title="Purpose" />
# Purpose
**gaggled** is a process manager focused on inter-process dependencies within a system with an eye to developing cluster-level process management later (with equivalent semantics for process dependency).  It is currently in development and bug reports are welcome at https://github.com/ZenFire/gaggled if you find anything wrong.  Reports of working/not working status on various platforms are also welcomed and encouraged.

<A name="toc1-11" title="Building" />
# Building

To build gaggled, you must create a build directory, run cmake, and use make to build.  How to use the rpm build setup and how to install are to be covered later. Watch this space.

    mkdir build
    cmake ..
    make -j4

You may be clever enough to just use **gaggled** right after building, but you're advised **for security reasons** to read this entire document, at the very least the Caveats section.

<A name="toc1-22" title="Platforms" />
# Platforms

**gaggled** has been tested on a few platforms, and is expected to work on all relatively recent Linux distributions.

<A name="toc2-27" title="Known to Work" />
## Known to Work

* Centos 5
* Scientific Linux 6
* Debian Sid (9/29/2011) or Debian Squeeze with backported ZeroMQ 2.1 packages from Sid.

<A name="toc2-34" title="Might Work, Untested" />
## Might Work, Untested

* FreeBSD
* OSX

<A name="toc2-40" title="Not Expected to Work" />
## Not Expected to Work

* Any Windows systems (although this may work under Cygwin)

<A name="toc1-45" title="Dependencies" />
# Dependencies

* CMake 2.6 or later
* GCC 4.4 or later
* Boost 1.44
* ZeroMQ 2.1 with C++ support. 2.0 does not work with , but will build.

<A name="toc1-53" title="Concepts / Configuration" />
# Concepts / Configuration

<A name="toc2-56" title="Global Settings" />
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

<A name="toc2-81" title="Dependency Rules: Disallowed Configurations" />
## Dependency Rules: Disallowed Configurations

There are some dependency configurations that are valid syntax, you may not express in the configuration of **gaggled**.  Doing so will result in **gaggled** failing to run or configest.

* dependency of an `enabled` *program* on a non-`enabled` program.
* missing any required setting
* circular dependencies: there can be no cycles in the dependency graph.  The graph is directed (`of` -> `on`), so it's possible to have a dependency set like (a -> b), (b -> c), (a -> c) which is not considered a cycle.  If you have one, then the programs in it would never start.

<A name="toc1-90" title="Internals" />
# Internals

<A name="toc2-93" title="Stopping Programs" />
## Stopping Programs

During **gaggled** shutdown and in other scenarios, sometimes a program must be shut down.  This will work the same way **init** does it: use `SIGTERM`, if the process does not die within 10 seconds, `SIGKILL` will be sent.

<A name="toc1-98" title="Usage" />
# Usage

* `-c` $FILE will use a specific config file.  This argument is required.
* `-t` will check dependency rules and existence of all programs but not start **gaggled**
* `-h` to display help instead of running.

<A name="toc1-105" title="Listener, Controller, and SMTP Gate" />
# Listener, Controller, and SMTP Gate

There are two utility programs included with **gaggled**.

<A name="toc2-110" title="Listener" />
## Listener

The program **gaggled_listener** will connect to a gaggled instance that is configured with the `eventurl` option and display incoming program state changes messages as needed.  Use the -h option for help.

<A name="toc2-115" title="Controller" />
## Controller

The program **gaggled_controller** will connect to a gaggled instance that is configured with the `controlurl` option and print program state, shut down programs, or start them up.  Use the `-h` option for help.

<A name="toc2-120" title="SMTP Gate" />
## SMTP Gate

The program **gaggled_smtpgate** will connect to a gaggled instance that is configured with the `eventurl` and `controlurl` options and forward incoming program state changes messages via status emails to SMTP.  Other options are also required.  Use the -h option for help and required options.

The SMTP gateway must be restart for each gaggled restart, as sequence numbers start over and the set of programs can change, resulting in undefined behaviour.  Shut down the SMTP gateway before shutting down gaggled, and start gaggled before starting the SMTP gateway.  The easiest way to do this is run the SMTP gateway from within gaggled.

<A name="toc1-127" title="Caveats" />
# Caveats

* There is, at present, **no security** on the control or event channels. If you use `eventurl` or `controlurl`, transport restrictions such as firewalls and binding to localhost are the the only restriction on status information and up/down commands being interchanged.
* You must have a reliable system clock. startup sequences and SMTP notifications may not operate as expected if the system clock jumps forward or backward.  At the moment, timezone change/DST is untested.  Use UTC.  This is subject to change.
* using a large `startwait` or `tick` (compared to `delay` etc) can result in confusing behaviour (things taking longer to start or be recognized as stopped than expected)
* any program that uses the exit codes `EX_NOPERM`, `EX_DATAERR`, `EX_NOINPUT`, or `EX_UNAVAILABLE` (defined in sysexits.h for your system) may result in gaggled logging a specific type of execution failure that may not be accurate.
