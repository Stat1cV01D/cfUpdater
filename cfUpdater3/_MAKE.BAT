set INCLUDE=..\..\vc6_compiler\INCLUDE;..\..\vc6_compiler\INCLUDE;C:\Program Files\Microsoft Visual Studio 9.0\VC\atlmfc\include;
set LIB=..\..\vc6_compiler

rem ..\..\vc6_compiler\rc cfUpdater3.rc
..\..\vc6_compiler\cl cfUpdater3.cpp Additional.cpp /GD /link kernel32.lib rain.lib cfUpdater3.res

pause
del *.obj
del *.res
