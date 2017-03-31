@echo off

REM Beware, this references a specific version of Visual Studio / Visual C++. Edit the line below as needed.
IF NOT DEFINED LIB (call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" amd64)

SET SRC=%1
if "%SRC%" == "" SET SRC=4coder_barry.cpp

SET OPTS=/W4 /wd4310 /wd4100 /wd4201 /wd4505 /wd4996 /wd4127 /wd4510 /wd4512 /wd4610 /wd4457 /WX
SET OPTS=%OPTS% /GR- /nologo /FC
SET DEBUG=/Zi
SET BUILD_DLL=/LD /link /INCREMENTAL:NO /OPT:REF
SET EXPORTS=/EXPORT:get_bindings /EXPORT:get_alpha_4coder_version

REM Searches this directory, where buildsuper.bat resides, for include files. So, it can compile.
SET CODE_HOME=%~dp0

cl %OPTS% /I"%CODE_HOME% " %DEBUG% "%SRC%" /Fecustom_4coder %BUILD_DLL% %EXPORTS%

REM Don't need these
del *.lib
del *.obj
del *.exp
