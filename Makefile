# Makefile

# Top-level makefile for InterGif
# (K) All Rites Reversed - Copy What You Like (see file Copying)
#
# Authors:
#      Peter Hartley       <peter@ant.co.uk>
#

intergif:
    dir src
    amu

igviewer:
    dir viewersrc
    amu

types:
    /@.bin.types

findtypes:
    /@.bin.findtypes @ @.bin.types

release:
    @echo Making release archives 'ig' and 'igsrc'
    @/@.bin.release

clean:
    @-delete foo
    @-wipe src.o.* ~cfr~v
