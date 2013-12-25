Linux-Messagebox-Module
=======================
With this module users in the system can communicate with each other.

You can write a user space program which uses this module to implement a graphical chat interface. 
If you do, please inform us, we will like to see our module was useful to someone.

Installing
----------
-Compile with makefile. Don't be worried if you see some warning messages.

-Load the module with "insmod ./messagebox.ko" command

-Get the major number from system: grep messagebox /proc/devices

-Create the device node: mknod /dev/messagebox0 c MajorNumberFromPreviousStep 0

-Set-up user privileges chmod 666 /dev/messagebox0

Usage
-----
Sending messages to a user: echo "To user1: Hello!" > /dev/messagebox0

Rules:

*You must write T upper-case, o lower-case and put a space between username and o.

*After the username put a ":" and space character.

Receiving messages: "cat /dev/messagebox0"
*This will bring the messages sent to active user (caller).

![ScreenShot](https://raw.github.com/erdememekligil/Linux-Messagebox-Module/master/ss.jpg)
Special Command (Ioctl)
-----------------------
Normally cat operation will bring all messages sent to user. (Read mode 0)

With using ioctl.c you can change it to bring only unread messages. (Read mode 1)

Usage:

-Compile the ioctl.c: "gcc ioctl.c -o ioctl"

-Change read mode to 1: "./ioctl /dev/messagebox0 1"

Authors
-------
Erdem Emekligil - www.emekligil.com 

Ibrahim Seckin 

Samil Can
