#! /bin/bash

cd cvttools/posix/hashcat-utils-master/src && make
cd -
cd cvttools/posix/JohnTheRipper/src && ./configure && make

cpan Compress::Raw::Lzma
