USB HASP key emulator, based on USB bus driver
Copyright (c) 2004 Chingachguk, Denger2k, tch2000 All Rights Reserved
----

This emulator and it sources are intended ONLY for legal use. Thus, legal 
emulation of protected program HASP keys according to the law is about to 
protect copyrights of the country in which you live or according the international 
agreements.

Any use of the given program, breaking copyrights or international agreements, 
is not lawful.

ALL CIVIL and the criminal LIABILITY FOR ILLEGAL USE OF EMULATOR AND IT 
SOURCES LAYS ONLY ON USER.

Authors of this program do not carry any responsibility for any actions of users 
concerning copyrights violation.

YOU MUST NOT DISTRIBUTE THE EMULATOR AS A COMPILED PROGRAM AND TURN IT'S USAGE TO YOUR ADVANTAGE.

If you are not sure concerning your rights, please contact your local legal adviser.

----

To compile emulator:
1.  Change path in files "chk make.bat", "free make.bat" in
this lines:
set SRC_DRIVE=C:
set SRC_PATH=22\bus
set DDK_PATH=D:\WINDDK\2600.1106
, where SRC_DRIVE - disk drive letter, where sources are located
        SRC_PATH  - path to .\bus directory,
        DDK_PATH  - path to Windows XP DDK directory.

2. Execute
"chk make.bat"  for make debug build or
"free make.bat" for make release build.

3. For device driver installation execute
"chk install.bat"  to install debug build or
"free install.bat" to install release build of driver.

On run driver once scan registry key
\Registry\MACHINE\System\CurrentControlSet\Services\Emulator\HASP\Dump
, read dumps of all keys and create virtual USB-keys for each dump.
For success execution of this phase you _need_ to have already
Aladdin HASP device driver installed.

4. For "unplug" all virtual USB-keys you can execute file
"unplug all.bat", which call enum.exe to do this task.
With help of last program you can not only "unplug" keys,
but also "plug" it, see .\Controller\enum.exe /?.

in the general case for rebuilding list of avalible USB-keys according
to registry state you can restart driver (see p. 3) or use
enum.exe utility (see p. 4).

Emulator installation:
1. To install emulator you need 3 files:
.\Inf\vusbbus.cat
.\Inf\vusbbus.inf
.\Inf\VUsbBus.sys
, где VUsbBus.sys - compiled from sources driver.

You can install this driver in two ways:
1. First method:
	3.1. Run\Settings\Control panel\Device installation
	3.2. Yes, device is already connected
	3.3. Add new device
	3.4. Install device from list manually
	3.5. System devices
	3.6. Install from a disk...\Browse...
	3.7. Locate .\Inf directory, then "ќк".

2. Second method: add to .\Inf directory file devcon.exe
and .bat-files with this content:
Install.bat:
@echo off
devcon remove root\vusbbus
devcon install vusbbus.inf root\vusbbus

Remove.bat:
@echo off
devcon remove root\vusbbus
