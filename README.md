RFTP
====

The file transfer application was fully implemented based on the specifications presented in the project guidelines document. A sender and receiver network applications are implemented to support the following requirements:
* Exchange of messages using the unreliable UDP protocol.
* Exchange of files using the unreliable UDP protocol.
* The ability to communicate through the Netem network emulator.
* The ability to handle packet losses, corruption, and duplication by implementing packet timeouts, acknowledgements, sequence numbers, checksum and Cyclic Redundancy Check (CRC) .
* Implementation of the sliding-windows algorithm to support sender/receiver flow control and to improve the efficiency of the message exchange.

Discussion of File Transfer Application
---------------------------------------
We implemented some optimization techniques in the code to allow for more dynamic and more accurate realization of the TCP protocol operation:
* Disabling the UDP checksum calculation and verification to make our application layer checksum/CRC implementation override the default UDP implementation.
* The sliding window size is statically set at one side of communication and is sent to the other side to by dynamically adjusted there. In this way we need to statically set this variable at only one communication end.

Compiling and Running the Code
------------------------------
The code was implemented using GNU C++ (g++ compiler version 4.3.3) under Linux Ubuntu 9.04 Kernel version 2.6.28-11. The code is compiled using the make utility with a MakeFile.

Partners
--------
* Wassim Itani
* Ahmad El Hajj

References
----------
Ayman Kayssi, American University of Beirut, <a href="http://staff.aub.edu.lb/~ayman/" target="_new">More</a>.
