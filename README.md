RFTP
====

Overview
--------
The objective of this project is to design, implement, and test a protocol that provides reliable data transfer across a network that drops, delays, duplicates, or corrupts packets. The protocol also uses sliding-windows to improve efficiency and provide flow control.

Procedure
---------
* Implement two socket applications that use UDP to exchange messages. Verify that the applications can communicate through the Netem network emulator (http://www.linuxfoundation.org/collaborate/workgroups/networking/netem).
* Devise a protocol that can be used to send a large volume of data in small messages (e.g. no more that 1024 bytes of data per message). The protocol uses an IP-style checksum and 16-bit sequence numbers to handle corrupted and duplicated packets (i.e. define a message header). Have the sender place a sequence number and checksum in the header of each outgoing packet. Have the receiver use the checksum to verify that the packet arrives intact, and the sequence number to remove duplicates. A sequence number of all zeroes should not be used, except to indicate that the packet is an end-of-file packet.
* Use Netem to test the protocol by transferring a large volume of data (i.e., many packets). Specify nonzero values for packet duplication and packet corruption in Netem.
* Extend the protocol to handle packet loss by arranging for the receiver to return an acknowledgement. Have the sender retransmit a packet if the ack does not arrive within 50 msec. Wait for successful ack of each packet before sending the next.
* Use Netem to test retransmission by specifying a nonzero packet loss rate and transferring a large volume of data.
* Test the protocol with simultaneous packet loss, packet corruption, and packet duplication.
* Implement a CRC instead of a checksum and measure the difference in throughput for the file transfer application.
* Experiment with the timeout value by specifying different delay distributions in Netem.
* Extend the protocol to use a sliding window scheme with a window size of 8 messages.
* Test the protocol by having the sender display a log of incoming and outgoing packets.
* Configure Netem with high delay on the path from receiver to sender, and verify that a full window of messages can be transmitted before an acknowledgement arrives.
* Compare the throughtput of the protocol with that of a version that does not use sliding windows.
* Modify the protocol to work correctly with a 5-bit sequence number.
* Instead of using a constant window size of 8, revise the programs so that the window size is variable.
* Modify the receiver to have a limited buffer space. Compare the throughput with that of TCP.

Implementation
--------------

The file transfer application was fully implemented as presented above. A sender and receiver network applications are implemented to support the following requirements:
* Exchange of messages using the unreliable UDP protocol.
* Exchange of files using the unreliable UDP protocol.
* The ability to communicate through the Netem network emulator.
* The ability to handle packet losses, corruption, and duplication by implementing packet timeouts, acknowledgements, sequence numbers, checksum and Cyclic Redundancy Check (CRC) .
* Implementation of the sliding-windows algorithm to support sender/receiver flow control and to improve the efficiency of the message exchange.

Optimizations
-------------
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
