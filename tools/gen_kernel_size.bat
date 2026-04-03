@echo off
setlocal

if "%~2"=="" (
	echo Usage: %~nx0 kernel.bin output.inc
	exit /b 1
)

set "kernel=%~1"
set "output=%~2"

if not exist "%kernel%" (
	echo Kernel image not found: %kernel%
	exit /b 1
)

for %%I in ("%kernel%") do set "kernel_size=%%~zI"
set /a kernel_sectors=(kernel_size + 511) / 512

> "%output%" echo KERNEL_SIZE equ %kernel_sectors%
