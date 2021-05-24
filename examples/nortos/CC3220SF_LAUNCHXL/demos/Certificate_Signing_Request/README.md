## Example Summary

This example introduce the user to the easily creation of certificate signing request on CC32xx devices.

## Peripherals Exercised

* `Board_LED0` - Indicates that the board was initialized within `main()`

## Example Usage

* Internally generate certificate signing request 

* Build the project and run it on debug or flash it by using the SimpleLink Uniflash application

* Open a serial session (e.g. `HyperTerminal`,`puTTY`, etc.) to the appropriate COM port.
> The COM port can be determined via Device Manager in Windows or via `ls /dev/tty*` in Linux.

The connection should have the following settings
```
    Baud-rate:    115200
    Data bits:       8
    Stop bits:       1
    Parity:       None
    Flow Control: None
```

* The application run and expect to receive the following inputs (possible input):
    1. validity period            (positive, up to max uint32 size)
    2. Is certificate CA          (0 no \ 1 yes)
    3. subject country            (2 upper case characters, i.e US\IL\EU )
    4. subject state              (String up to 63 characters, 0-9\a-z\A-Z\,/,,, ,)
	5. subject locality           (String up to 63 characters, 0-9\a-z\A-Z\,/,,, ,)
	6. subject surname            (String up to 63 characters, 0-9\a-z\A-Z\,/,,, ,)
	7. subject organization       (String up to 63 characters, 0-9\a-z\A-Z\,/,,, ,)
	8. subject organization unit  (String up to 63 characters, 0-9\a-z\A-Z\,/,,, ,)
	9. subject common name        (String up to 63 characters, 0-9\a-z\A-Z\,/,,, ,)
	10. subject email             (String up to 63 characters, 0-9\a-z\A-Z\,/,,, ,)
```

* If values are not supplied (pressing enter), default are printed to terminal and being used once creating the certificate

* Certificate is generated and printed to terminal

* Copy certificate from terminal, sign with ecc-spec256r1, in pem format 

* Enter desired file name and paste to terminal in order to save in the File System

* Once written to File System, the certificate can be used for secured connection
  

> Please install the latest CC32xx Service Pack 

## References
For further information please refer to the user programmers guide: [CC3X20 Programmer's Guide](http://www.ti.com/lit/swru455)