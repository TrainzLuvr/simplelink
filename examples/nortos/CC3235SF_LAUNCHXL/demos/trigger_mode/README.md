## Example Summary

This example introduce the user to Trigger Mode on CC32xx devices.

## Overview

* This example is based on the Provisioning example provided by the CC32xx SDK.

* The purpose of this example is to show the user a method, in which the device can enter LPDS, and be awoken by a packet sent on a specific, pre-determined socket.

## Peripherals Exercised

* `Board_LED0` - Blinking led Indicates disconnected. Led on indicates connection established
* `Board_LED1` - Led on indicates error occurred
* `Board_LED2` - Led on indicates error occurred
and while blinking, indicates that the board is trying to connect to AP.


## Example Usage

* Establish connection between CC32xx device to a specific AP using the Android \ IOS mobile application

* Build the project and flash from the debugger or by using the SimpleLink Uniflash 

* Open a serial session (e.g. `HyperTerminal`,`puTTY`, etc.) to the appropriate COM port.
> The COM port can be determined via Device Manager in Windows or via `ls /dev/tty*` in Linux

The connection should have the following settings
```
    Baud-rate:    115200
    Data bits:       8
    Stop bits:       1
    Parity:       None
    Flow Control: None
```

* The example starts by showing on the terminal the application name and tries to establish connection with existing profiles

* If a connection is not established during preconfigured time out, provisioning will start, wait for connection to be established and for an IP to be aquired

* After connection is established, Ping is sent once to the gateway specified by the Provisioning stage to verify connection

>__Once the connection is verified, we can start testing the Trigger Mode functionality__

__For example:__ in order to see how the feature works, the user can:
* Ask the Access point to open a TCP connection to the SimpleLink device
* After the TCP connection is established the device will simply enter LPDS mode (NWP is still awake) until new packets are received (e.g each 5 seconds, NWP as wakeup source)  
* Send X packets each Y seconds to the SimpleLink device from the Access Point
* During the interval, the application processor will sleep and save power
  
> These procedures can be done using iperf or a dedicated script 

## Application Design Details

* The application is based on the Provisioning application, and as such requires exactly the same from the user.

* The application is designed to enter LPDS once a TCP connection is established, and be awoken when a packet is received (on the TCP port).


> Please install the latest CC32xx Service Pack in order to get the most optimization and lowest current consumption.

## References
* For more detailed information please refer to the [programmer's user guide](http://www.ti.com/lit/SWRU455) and review the Trigger Mode feature (6.7.2)
* For more detailed information please refer to the [programmer's user guide](http://www.ti.com/lit/SWRU455) and review the Provisioning chapter 15.
* For more detailed information regarding the provisioning feature, please refer to the [provisioning application report](http://www.ti.com/lit/SWRA513)
* For further information please refer to the user programmers guide: [CC3X20 Programmer's Guide](http://www.ti.com/lit/swru455)