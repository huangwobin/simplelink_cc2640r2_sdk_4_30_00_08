@echo off
::------------------------------------------------------------------------------
::
::  This batch file will generate the BLE ROM symbols based
::  on ble_r2.out and the symbol steering files. The first
::  (ble_r2.symbols) will contain all ROM symbols, and the
::  second (common_r2.symbols) will only contain the ECC
::  ROM symbols needed by the Application.
::
::  Note: This batch file only supports R2.
::
::------------------------------------------------------------------------------

:: Set Current Working Directory to Project Directory
cd %1

:: Set Paths
set dIAR_R2="C:\Program Files (x86)\IAR Systems\EWARM-7.80.3"
set pIAR_Sym=%dIAR_R2%\arm\bin\isymexport.exe

echo Generating Symbols for BLE and Common
%pIAR_Sym% .\ble_r2.out .\ble_r2.symbols --reserve_ranges --edit .\symbol_steering.txt
%pIAR_Sym% .\ble_r2.out .\common_r2.symbols --edit .\symbol_steering_common.txt

echo All done!
goto done

:error
echo Error encountered during execution. Exiting...

:done

::------------------------------------------------------------------------------
