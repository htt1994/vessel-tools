rem Notes:
rem It is necessary to ensure that Qt Creator finds the 64-bit Qt dlls.  Put them at the front of the PATH list.

set PATH=C:\Qt64\4.8.1\bin;C:\Qt64\4.8.1\lib;C:\VTK-VS10-x64;%PATH%
set VTK_DIR=C:\VTK-VS10-x64
C:\QtSDK\QtCreator\bin\qtcreator CMakeLists.txt

