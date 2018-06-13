@echo off
del .\Inf\vusbbus.sys
copy /b .\Bus\objfre_wxp_x86\i386\vusbbus.sys .\Inf
devcon remove root\vusbbus
cd .\Inf
..\devcon install vusbbus.inf root\vusbbus
