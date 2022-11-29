README for the source of a v0.01 emulator of hp 9000 series 200 machines

Preliminary remark:

The goal aimed by this emulator is to have a software program with:
- 68000 and peripherals debugging facilities
- breakpoint facilities
- ... whatever I need

to help myself to rebuild the same functionnalities in verilog in an fpga.

In brief I want make the same thing that I have done for HP85/86 systems
(see http://sites.google.com/site/olivier2smet2/hpseries80)

And that's all, period.

Nice facts:

- emulate hp9816, hp9836, hp9836c, hp9837
- national 16081 floating point accelator done (boost latest version of basic
  and pascal)
- hpib with ti9914a controller at functional level with bootrom, basic and
  pascal (3 code base, hard enough to synchronize), easy to add more instruments
- hpib amigo and ss80 based floppy disc (ram based) and hard disc (file based)
  at functional level
- C language based (easier to port)
- text only readme formatted at 80 columns for an easier reading experience on
  text only systems (you can't compile and run the emulator, but the readme is
  human readable at least)
- only free tools used (free compiler from microsoft and free ressource editor,
  I use ResEdit)
- should compile from Visual C 6 upto Visual C++ studio Express edition 2010
  
Bad facts:
- English is not my mothertongue
- only 68000 cpu, 68010 is almost done (see source)
- no hp mmu (even if schematics exist, not a priority as I don't found HP UX V5
  or 7 or previous disk image on the net)
- no dma card done (even if schematics exist, not needed as the HPIB is fast
  enought for me actually)
- 68000 timing is not always exact (not needed for what i want to do with this
  emulator)
- hpib hp2225 crude dump printer capability
- only interface part of serial module for 9816 emulation
- Windows based (for some people), but should work from NT4 upto Windows 7
- IA32 intel target but I think a pentiumII 300 is enough to have a 8MHz system
- not x64 clean (but I tested, you could make it clean in 5 minutes at the cost
  to have incompatable saves of the system ... so I let it as is)
- endianess changes are hard coded at differents place: 68000 emulation, memory
  access, peripherals I/O, ... but it is easier to remove the inverse order than
  to add it ...
- another amigo but only ss80 emulation (I do not use ISA HPIB card to connect
  real instruments, sorry I have only an old 9121S, one 9133 and a barely
  functional 9153B)
- whatever i forget and you dislike
- (fill the blank lines and add some if you need)
-
-
- perhaps you will not like my coding practices, but honestly, I don't care,
  it's just a hobby for me ...
  (but if you code from scratch a good quality emulator for series 200, or 300 i
   will be happy to look at it to learn some nice coding practices BUT only if
	it is in C language AND even if it is for some unknown target such as PERQ
	machine with your homemade C compiler written in PERQ pascal)
- as a bad coding practice example, i sometimes write very long C line, but I'm
  not sorry for that
- you have to download the binary zip file to get access of bootrom files,
  kml files (HMI description), disk images for pascal and basic and even disc
  image of 7908 pascal 3.25 on hfs filesystem and basic 5 in hfs system (to keep
  this zip small) needed to run the program after compiling it
- you need to have a legal licence of a windows operating system to run the
  compiler needed to generate the binary of the program as distributed (I hope
  yours is legal, mine is)
  
Some comments:

The emulator is loosely based on an old (circa 2003) release of emu42
(Christoph Giesslink : http://hp.giesselink.com/emu42.htm)

Over the time many stuff changed, only kml, debugger and files parts are
still with original code. In any case, all files are loosely (c) C. Giesslink
for having released the source code of his HP42S emulator :)

Now some comments about the sources files (with some indication about the
windows peculiarity used in each)

Headers
=======

HP98x6.h
--------
main header file to declare extern functions and variables

98x6.h
------
header file for mc 68000 processor and chipsets for chipmunk system
can be used to declare a 68010 processor (some comments need to be removed
manually elsewhere)

debugger.h
----------
still emu42 like

HP98x6.h
--------
General definitions of functions


kml.h
-----
kml definition for display temple (look and fell, display and annunciators)
just added the last 2 functions

mops.h
------
memory operations definitions

opcodes.h
---------
structs for ea modes and opcodes

ops.h
-----
real code for elementary 68000 opcodes execution 
adjust cycles counter (not all operations are exact 68000 duration)

ressource.h (windows specific)
-----------
windows ressource define (generated from HP98x6.rc)

color.h (windows specific)
-------
colors definition for debugger

Stdafx.h (windows specific)
-------- 
generic windows headers include

Ressources
==========

bitmap1.bmp
-----------
some check bitmap for debbuger part

HP98x6.ico
----------
icone for windows apps

small.ico
---------
same for a smaller size

toolbar1.bmp
------------
toolbar bitmap for debugger

HP98x6.rc (windows specific of course))
---------
ressource files for menu, dialog boxes, ...
you can use the free tool resedit (http://www.resedit.net/)

216_bitmap1.bmp
---------------
bitmap for alpha screen rendering for 9816.c display
perhaps not exact, the available docs are not very precise on it

236_bitmap1.bmp
---------------
bitmap for alpha screen rendering for 9836.c display
perhaps not exact, the available docs are not very precise on it

236c_bitmap1.bmp
---------------
bitmap for alpha screen rendering for 9836c.c display
perhaps not exact, the available docs are not very precise on it

Btw if you can dump characters generator rom of 9816, 9826 and 9836
I will use them to have a better rendering...
(special components, but the schematics give the pinout)

Sources
=======

HP98x6.c (windows specific)
--------
main source for application
- menus
- windows functions (title, status of menu, )
- new system settings
- windows message handler (save, create, load, ...)
- keyboard and mouse wheel handler
- kml buttons handler
- main windows event dispatch loop

files.c (windows specific)
-------
All stuff for files and documents
Some debug functions (crc, S record, ...)
All new document (as system) settings and functions 
Some windows functions too
Load and save system functions
Open file dialog boxes functions
Load bmp bitmap functions

settings.c (windows specific)
----------
deal with registry settings and reading, from emu42 as is


StdAfx.c (windows specific)
-------- 
empty one :) easy to port


display.c (windows specific)
---------
deal with all common display functions
create and destroy all the windows bitmap used for the display subsystem
deals with palettes (all bitmaps are 8 bpp bitmap (only the window one is
something else)) for porting, use sdl ... if you like it and you system is
supported

display16.c (blitcopy functions windows specific only) 
-----------
display for 9836A and 9816A
blink effect is not done, but the structure is there (use of alpha2 as the
bitmap displayed when toc blink and alpha1 when tic blink)

display36c.c
------------
display for 9836C
miising blink also

display37.c
-----------
display for 9837
many guess for line mover machine

98x6.c (windows specific)
------
main loop for thread system execution (mc68000, peripherals and hpib instruments
loosely synchronous)
use high performance counter on intel chip to measure the speed of the emulator.

debugger.c (windows specific)
----------
deals with debugger dialog
not all functionnality are done

fetch.c
-------
execute mc68000 opcodes with cycles (not all exact)
the structure of the emulation was choosen to ease future verilog rewritting
it's not the fastest but it's one I deeply understand ;) (and that's enough)

opcodes.c
---------
mc68000 opcode huge combinatorial only decoder function

mops.c
------
memory access functions
all bus access are done here 
you can find also prom ID for different systems

disasm.c
--------
crude and quick mc68000 dis-assembler, with some labels from bootrom 3.0

kml.c (windows specific)
-----
Taken as is from this old version of emu42
just added 2 quick hack functions


hp-9130.c
---------
internal floppy discs of 9836, hp9130 like

hp-ib.c
-------
hpib controller based on TI9914A
very tricky code for delay, mc68000 code from bootrom, pascal and basic are all
different and need some tricky delays adjustement

hp-9121.c
---------
amigo protocol dual unit as 9121D or 9895A floppy
amigo protocol mono unit as 9134 disk
ram based, user have to save when needed

hp-9122.c
---------
ss80 protocol dual unit as 9122D, support all format
still ram based

hp-7908.c (windows asynchronous file I/O)
---------
ss80 protocol mono unit as 7908, 7909, 7912 disk, file based, no need to save
work

hp-2225.c
---------
basic hpib printer (dump on file)

hp-98626.c
----------
9816 onboard serial module
non functionnal, just the IO regs for passing bootrom test

hp-98635.c
----------
floating point card emulation


Thanks:
=======
already done in the readme file of the binary zip, sorry for the people who
can't deal with zip file, mail me if you want a compress-ar-uuencode file (haha,
you can't read this one either)

Anyway,

Many thanks to Christoph Giesslink for his emu42 program (base code for my
emulators since 2004)
Many thanks to Jon Johnston of hpmuseum.net for invaluable help: rom dumps,
disk image files in td0 format, pdf of manuals and schematics
Many thanks also to Tony Duell, his schematics are invaluable to make some
emulator when you don't have the real machine (hint: green channel inversion
on 9836C alpha part card)
Many thanks to bitsavers.org for hosting such quantity of old manuals

Now some wishes:

I hope this project could help the 9000 serie 200 community to grow in quality
and quantity, and to ease the availability of old disc images of program.

Olivier De Smet (olivier dot two (arabic numeral) dot smet at gmail dot com)

March 2011 the 5th
