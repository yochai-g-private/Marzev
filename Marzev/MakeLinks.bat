@echo off

set ProjectDir=%~dp0
set Lib=%ProjectDir%lib\

set Libraries=%USERPROFILE%\Documents\Arduino\Libraries\

::============================================
:: Create links
::============================================

::call :Link_LIB      SevenSegmentFun     src
::call :Link_LIB      TimeLib             .

call :Link_LIB      NYG

goto :eof

::---------------------------
:Link_LIB
::---------------------------
if exist %Lib%%1        rmdir  /s/q %Lib%%1
mklink /D %Lib%%1       %Libraries%%1

goto :eof
