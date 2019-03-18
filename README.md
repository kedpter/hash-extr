# hash-extr

A command-line tool to extract hashes from encrypted files for hashcat.
1. Detect file types by checking the magic number.
2. Extract hashes with some 3rd tools.
3. Convert to hashcat supported format.

## Usage
```
./hash-extr 123.7z
```
For more details, check `--help`

Supported file types:
- :white_check_mark:office
- :white_check_mark:pdf
- :white_check_mark:szip
- :white_check_mark:rar
- :white_check_mark:pkzip

## Build

```
# linux or mac
make
# windows
# make win
```

## Requirements

  - hashcat-utils-master
  - JohnTheRipper
  - `Lzma` package in perl

  I have downloaded the packages in the folder `cvttools`. Install them by executing
  ```
  sudo bash tool_intall.sh
  ```
