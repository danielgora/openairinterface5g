<table style="border-collapse: collapse; border: none;">
  <tr style="border-collapse: collapse; border: none;">
    <td style="border-collapse: collapse; border: none;">
      <a href="http://www.openairinterface.org/">
         <img src="./images/oai_final_logo.png" alt="" border=3 height=50 width=150>
         </img>
      </a>
    </td>
    <td style="border-collapse: collapse; border: none; vertical-align: center;">
      <b><font size = "5">Running the OAI Basic Simulator without an EPC</font></b>
    </td>
  </tr>
</table>

This page is valid on the following branches:

- `master` starting from tag `v1.1.0`
- `develop` starting from tag `2019.w11`

# Overview

The **Basic Simulator** is an OAI virtual device which replaces the hardware based
radio heads (for example the USRP device) with a software simulation. It
allows connecting the OAI UE and the OAI eNodeB through a network interface
carrying the time-domain samples, getting rid of unpredictable perturbations
inherent in over-the-air transmission.

The **Basic Simulator** is the ideal tool to check signal processing
algorithms, protocol implementations, and performing debugging sessions
without the need for any hardware radio equipment.

The main limitations are:

- Only one OAI UE will connect to the OAI eNB.

- No channel noise.

The **Basic Simulator** can be run both with a connected EPC core network
or without an EPC using the so-called **noS1** mode.

It is recommended to first test the **Basic Simulator** without a connected
EPC using the **noS1** mode to confirm the basic connectivity as documented
here.  After that, the EPC can be introduced to allow the user to run the
entire LTE core network.  Running the **Basic Simulator** with an EPC is
described in the [BASIC_SIM](BASIC_SIM.md) document.

# System configuration

The simplest way to test the **Basic Simulator** is to configure two
separate systems, one running the eNodeB (eNB) and the other running the
User Equipment (UE).  These can be either virtual or physical machines.

Each system will need a high speed Ethernet interface to communicate with
the other system.  It is recommended that the user use a separate network
interface or IP address for linking the eNB and UE simulators from the
interface which is used to access the system itself.

When the **Basic Simulator** is run it will create virtual IP interfaces
on each machine and automatically assign IP addresses to these interfaces.

On the **eNodeB** it will create `oaitun_enb1` and `oaitun_enm1` interfaces.
On the **UE** it will create `oaitun_ue1` and `oaitun_uem1` interfaces.

The `--nas.noS1.NetworkPrefix` flag can be specified to the simulators to
specify the IP network prefix to be used for these interfaces.  This will
be described in more detail below.

After the eNodeB and UE simulators are running, the user can test the
communication using normal IP communication utilities (`ping`, `scp`,
`iperf`) in order to run user traffic over the simulated radio channel
between the `oaitun_enb1` and `oaitun_ue1` interfaces on the eNB and UE.

(ZZZ: What are the other interfaces for?)

## Provisioning test VMs with Vagrant

In the below example two Centos 7 virtual machines are created with Vagrant.
[Vagrant](www.vagrantup.com) provides a simple method to create and
provision virtual machines quickly with a single command.

The user can quickly provision both virtual machines on single physical
host using the command `vagrant up`.

The Vagrant configuration file `$OPENAIR_HOME/vagrant/centos/Vagrantfile`
will configure the virtual machines using VirtualBox and automatically
configure the host names and network interfaces necessary to run the tests.

After the virtual machines are created, the machines can be accessed using
the commands `vagrant ssh OAI-eNB` to access the eNodeB and `vagrant ssh
OAI-UE` to access the UE.

VM1: **OAI-eNB**

**eth0** - 10.0.2.15/24 - Vagrant NAT interface to physical host.

**eth1** - 10.20.0.131/24 - host-only local network to **UE** and physical host.

VM2: **OAI-UE**

**eth0** - 10.0.2.15/24 - Vagrant NAT interface to physical host.

**eth1** - 10.20.0.130/24 - host-only local network to **eNodeB** and physical host.

If `vagrant` is not used, then the IP interface names and IP addresses
may be different.

# Building the Basic Simulator

As described on the [build page](BUILD.md), the basic simulator *basicsim*
is built into the ``lte-softmodem`` and ``lte-uesoftmodem`` automatically
from the standard build, no additional flags are required to ``build_oai``.

The user should simply build the eNB and UE simulators as follows:

```bash
$ source oaienv
$ cd cmake_targets
$ ./build_oai -I --eNB --UE
```

Both the eNB simulator `lte-softmodem` and UE simulator `lte-uesoftmodem`
are present in the `cmake_targets/ran_build/build` folder.

More details are available on the [build page](BUILD.md).

When using the virtual machines specified in the Vagrantfile, this step
is not necessary as Vagrant will automatically download and compile the
source code when creating the virtual machine.

# Testing the simulators in noS1 mode.

## Configuring the eNB

The `lte-softmodem` eNB simulator can use one of the configuration files:

`$OPENAIR_HOME/ci-scripts/conf_files/lte-fdd-basic-sim.conf` - FDD testing.

`$OPENAIR_HOME/ci-scripts/conf_files/lte-tdd-basic-sim.conf` - TDD testing.

No modifications are necessary to these supplied configuration
files.

Since the eNB is being run in **noS1** mode, the S1 (and X2) interfaces are
not used.  Similarly, since the EPC is not being used the MME IP address
does not need to be specified.

## Configuring the UE

No configuration file is necessary for the UE simulator `lte-uesoftmodem`.
All of the configuration parameters are specified on the command line.

## Starting eNB

```bash
$ source oaienv
$ cd cmake_targets/ran_build/build

$ # Test the eNB with FDD radio traffic...
$ ENODEB=1 sudo -E ./lte-softmodem -O $OPENAIR_HOME/ci-scripts/conf_files/lte-fdd-basic-sim.conf \
    --basicsim --noS1 --nas.noS1.NetworkPrefix "11.0"

$ # or test the eNB with TDD radio traffic.
$ ENODEB=1 sudo -E ./lte-softmodem -O $OPENAIR_HOME/ci-scripts/conf_files/lte-tdd-basic-sim.conf \
    --basicsim --noS1 --nas.noS1.NetworkPrefix "11.0"
```

The `ENODEB=1` environment variable informs the `tcp_bridge_oai` library,
which is automatically loaded into the `lte-softmodem` application, to run
in server mode and to start a TCP listener on all IP addresses on port 4043.
This TCP port is the port on which the network traffic is received from
the UE.

The `-O <conf file>` specifies the configuration file to use.

The `--basicsim` flag configures the `lte-softmodem` eNB simulator to run
in **Basic Simulator** mode.

The `--noS1` flag configures the `lte-softmodem` eNB simulator to run in
**noS1** mode.

The `--nas.noS1.NetworkPrefix <IP prefix>` flag configures the IP address
prefix for the `oaitun_enb1` and `oaitun_enm1` virtual IP interfaces which
are created when `lte-softmodem` is run.  The default value is `"10.0"`,
which causes the `oaitun_enb1` interface to be assigned the IP address
`10.0.1.1/24` and the `oaitun_enm1` interface to be assigned the IP address
`10.0.2.1/24`.  Only the first two bytes of the IP network prefix can be
specified as a quoted string.

In this example, the IP interface used to access the virtual machine is
already using the `10.0.0.0/24` subnet so we must specify a different IP
prefix for the eNB and UE IP interfaces.  We specify `"11.0"` which causes
the `oaitun_enb1` interface to be assigned the IP address `11.0.1.1/24`
and the `oaitun_enm1` interface to be assigned the IP address `11.0.2.1/24`.

## Starting the UE

No configuration file is necessary for the UE simulator `lte-uesoftmodem`.
All of the configuration parameters are specified on the command line.

```bash
$ source oaienv
# Edit openair3/NAS/TOOLS/ue_eurecom_test_sfr.conf
$ cd cmake_targets/ran_build/build
$ TCPBRIDGE=10.20.0.131 sudo -E ./lte-uesoftmodem -C 2680000000 -r 25 --ue-rxgain 140 \
    --basicsim --noS1 --nas.noS1.NetworkPrefix "11.0"
```

The `TCPBRIDGE=<eNB IP address>` environment variable specifies the IP
address to use to connect to the `tcp_bridge_oai` listener port on the eNodeB.

The `-r <num DL RB>` specifies the number of downlink radio bearers.

This value must match the value specified in the eNB configuration
file by the parameter `N_RB_DL`.  In the `lte-fdd-basic-sim.conf` and
`lte-tdd-basic-sim.conf` eNodeB configuration files the default value is 25.

The `-C <DL freq> ` specifies the downlink frequency.

This value must match the `downlink_frequency` in the eNB
configuration file.  If `lte-fdd-basic-sim.conf` is used on the eNB
side, the default value is 2680000000. If `lte-tdd-basic-sim.conf`
is used on the eNB side, the default value is 2350000000.

The `--noS1` flag configures the `lte-uesoftmodem` UE simulator to run in
**noS1** mode.

The `--nas.noS1.NetworkPrefix <IP prefix>` flag configures the IP address
prefix for the `oaitun_ue1` and `oaitun_uem1` virtual IP interfaces which
are created when `lte-uesoftmodem` is run.  The default value is `"10.0"`,
which causes the `oaitun_ue1` interface to be assigned the IP address
`10.0.1.2/24` and the `oaitun_uem1` interface to be assigned the IP address
`10.0.2.2/24`.  Only the first two bytes of the IP network prefix can be
specified as a quoted string.

In this example the IP interface used to access the virtual machine is
already using the `10.0.0.0/24` subnet, so we must specify a different IP
prefix for the eNB and UE IP interfaces.  We specify `"11.0"` which causes
the `oaitun_ue1` interface to be assigned the IP address `11.0.1.2/24`
and the `oaitun_uem1` interface to be assigned the IP address `11.0.2.2/24`.

# Testing the data plane in noS1 mode

When the simulators are run in **noS1** mode, the IP addresses assigned
to the eNB and UE interfaces are fixed according to the network prefix
specified with the `--nas.noS1.NetworkPrefix` flag.

The IP interface addresses can be shown using the `ip` command:

On the eNB:

```bash
[vagrant@eNB ~]$ ip addr
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host
       valid_lft forever preferred_lft forever
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP group default qlen 1000
    link/ether 52:54:00:4d:77:d3 brd ff:ff:ff:ff:ff:ff
    inet 10.0.2.15/24 brd 10.0.2.255 scope global noprefixroute dynamic eth0
       valid_lft 72846sec preferred_lft 72846sec
    inet6 fe80::5054:ff:fe4d:77d3/64 scope link
       valid_lft forever preferred_lft forever
3: eth1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP group default qlen 1000
    link/ether 08:00:27:2a:c5:45 brd ff:ff:ff:ff:ff:ff
    inet 10.20.0.131/24 brd 10.20.0.255 scope global noprefixroute eth1
       valid_lft forever preferred_lft forever
    inet6 fe80::a00:27ff:fe2a:c545/64 scope link
       valid_lft forever preferred_lft forever
16: oaitun_enb1: <POINTOPOINT,MULTICAST,NOARP,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UNKNOWN group default qlen 500
    link/none
    inet 11.0.1.1/24 brd 11.0.1.255 scope global oaitun_enb1
       valid_lft forever preferred_lft forever
    inet6 fe80::e27c:1693:a100:8fee/64 scope link flags 800
       valid_lft forever preferred_lft forever
17: oaitun_enm1: <POINTOPOINT,MULTICAST,NOARP,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UNKNOWN group default qlen 500
    link/none
    inet 11.0.2.1/24 brd 11.0.2.255 scope global oaitun_enm1
       valid_lft forever preferred_lft forever
    inet6 fe80::d632:1712:20b4:a77f/64 scope link flags 800
       valid_lft forever preferred_lft forever
```

On the UE:

```bash
[vagrant@UE ~]$ ip addr
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host
       valid_lft forever preferred_lft forever
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP group default qlen 1000
    link/ether 52:54:00:4d:77:d3 brd ff:ff:ff:ff:ff:ff
    inet 10.0.2.15/24 brd 10.0.2.255 scope global noprefixroute dynamic eth0
       valid_lft 72894sec preferred_lft 72894sec
    inet6 fe80::5054:ff:fe4d:77d3/64 scope link
       valid_lft forever preferred_lft forever
3: eth1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP group default qlen 1000
    link/ether 08:00:27:b9:0c:88 brd ff:ff:ff:ff:ff:ff
    inet 10.20.0.130/24 brd 10.20.0.255 scope global noprefixroute eth1
       valid_lft forever preferred_lft forever
    inet6 fe80::a00:27ff:feb9:c88/64 scope link
       valid_lft forever preferred_lft forever
12: oaitun_ue1: <POINTOPOINT,MULTICAST,NOARP,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UNKNOWN group default qlen 500
    link/none
    inet 11.0.1.2/24 brd 11.0.1.255 scope global oaitun_ue1
       valid_lft forever preferred_lft forever
    inet6 fe80::fd4c:346d:dc7b:16f2/64 scope link flags 800
       valid_lft forever preferred_lft forever
13: oaitun_uem1: <POINTOPOINT,MULTICAST,NOARP,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UNKNOWN group default qlen 500
    link/none
    inet 11.0.2.2/24 brd 11.0.2.255 scope global oaitun_uem1
       valid_lft forever preferred_lft forever
    inet6 fe80::b97d:673f:d01d:ffa3/64 scope link flags 800
       valid_lft forever preferred_lft forever
```

Basic connectivity can be tested using `ping`:

From the eNB to the UE:
```bash
$ ping 11.0.1.2
```
From the UE to the eNB:
```bash
$ ping 11.0.1.1
```
Throughput testing can then be performed using `iperf`:

Start the `iperf` server on the UE:
```bash
$ iperf -B 11.0.1.2 -u -s -i 1 -fm
```

Transmit data from the eNB to the UE `iperf` server:
```bash
$ iperf -c 11.0.1.2 -u -b 1.00M -t 30 -i 1 -fm -B 11.0.1.1
```
