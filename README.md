# GNU Make - patched
There is a feature in Opus Make that we use all the time for Windows development: VPATHs for targets. This version of GNU make adds that feature. For simple projects, the use of "target VPATHs" removes that you need to resort to recursive make.

I used the same trick as Opus Make: the "vpath" command stays exactly the same, and there is a new command to implement the new behaviour. Like Opus Make, the new command is called ".path". The syntax of the ".path" command is the same as for "vpath". The new command simply forces that if the file could not be found in any of the directories, it places it in the first of the directories.

For example, a simple makefile
```
.path %.obj ./obj
.path %.exe ./bin

project: hello.exe

hello.obj : hello.c
	cc -o=$@ $<

hello.exe : hello.obj
	link -o=$@ $<
```
If we run "make -n" on this makefile, we will get:
```
cc -o=./obj/hello.obj hello.c
link -o=./bin/hello.exe ./obj/hello.obj
```
When we had written "vpath" instead of ".path", there would not have been "./obj" and "./bin" subdirectories on the command lines.
