# Introduction

1. LineNumberTracker : LNT
2. SymbolLocationTracker : SLT

## Edit Makefile in each directory

1. Change -DPINKIT=... to point to your location of your Pin 4 kit
2. Change -DSTAPIN_REPO=... to point to your location of libstapin and stapin-plugin dirctories:
  pinplay-tools/STAPIN/STAPINLibPlugin/

## Build process
 make clean
 make setup (or setup.debug for a debug build)
 make

