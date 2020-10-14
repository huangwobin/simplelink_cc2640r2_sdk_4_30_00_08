lib_search.exe

Created by: 	Matt Kracht
Updated by:     Josh Phua
Last Updated: 	11/29/2016
Version:	    2.0.0

********************************************************************************

usage: lib_search [-h] [-v] opt lib_dir cmd device xml

Used in conjunction with BLE Stack releases as a pre-build step. Parses
provided .opt file and creates a linker file with necessary libs to be linked
into project.

positional arguments:
  opt            Local project configuration file for stack project
  lib_dir        Directory containing library files
  cmd            Linker command file that will be written to
  device         device (e.g. cc2640, cc1350)
  xml            directory of xml features files

optional arguments:
  -h, --help     show this help message and exit
  -v, --version  show program's version number and exit
