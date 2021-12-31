# Swarm - A naive distribute task dealer

Swarm is a simple and naive approach to parallelize tasks in a distributed manner.

The library is based on OpenSSH. It uses SSH channels to execute remote commands in the remote host pool and SCP to copy
files between the local and the remote host.

## Download and compile

The library requires `libssh`, `libssh` and `pthreads` in order to compile. You can install the libraries in debian
based hosts by:

```
sudo apt install libssh-dev libssl-dev
```

To download and compile:

```
git clone https://github.com/xarteaga/swarm.git
cd swarm
mkdir build
cmake ..
make
make install
```

## SSH environment

Before running any application, it is required to have copied local keys into the remote hosts:

```
ssh-copy-id <host-name>
```

Also, it can be useful tweaking SSH configuration file in `~/.ssh/config` to use a master connection and compression for
all SSH sessions:

```
host *
    ControlMaster yes
    ControlPath /tmp/ssh-controlmaster-%r@%h:%p
    ControlPersist 1h
    Compression yes
```

To maximize the maximum number of connections a host supports, one can append to `/etc/ssh/sshd_config` (for remote
hosts):

```
MaxSessions 60
MaxStartups 1024
```

## Use cases

Currently, there is only one working example `swarm-cc` which allows distributed C and C++ compilation in a distributed
manner.

### C/C++ compiler

Once `swarm` is built and installed, one can start using `swarm-cc`.

`swarm-cc` can be used in `cmake` based projects by adding the following options:

```
cmake .. \
  -DCMAKE_C_COMPILER_LAUNCHER="swarm-cc"\
  -DCMAKE_CXX_COMPILER_LAUNCHER="swarm-cc"
```

For `Makefile` projects append to make command the compiler after `swarm-cc`. For example:

```
cmake .. \
  CC="swarm-cc gcc"\
  CXX="swarm-cc g++"\
```

Linux kernel example using two hosts:

```
git clone https://github.com/torvalds/linux.git
cd linux
make distclean
make defconfig
make menuconfig (and load whatever kernel configuration file you need)
SWARM_HOSTNAMES=localhost,remotehost time make -j28 CC="swarm-cc gcc"
```

This may result in higher time due to the host selection algorithm during SSH session connection. To improve the host
selection `swarm-lb` polls the CPU load from the host candidates to create a fitness parameter and through inter-process
communication provides the best fitted CPU.

## Task distribution process

## Load balancing

The load balancing is performed in the SSH session

## Current applications
