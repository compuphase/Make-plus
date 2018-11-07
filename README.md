# GNU Make - patched
This is a version of GNU make with a few additions that are mostly inspired by Opus Make.

## VPATH for targets
There is a feature in Opus Make that we use all the time for Windows development: the "`.path`" command, which is essentially a VPATH for targets. For simple projects, the use of "target VPATHs" removes that you need to resort to recursive make. It allows you to easily separate intermediate files from source files.

The `.path` command essentially the same as `vpath`, it also uses the same syntax. The difference is that when a prerequisite is not found (in any of the paths), where it will be located:
* If you use one or more `.path` commands, it is located in the first path listed (on the first `.path` command).
* If you only use `vpath`, it is located in the `.` directory.

For example, a simple makefile
```
.path %.obj ./obj
.path %.exe ./bin

project: hello.exe


hello.obj : hello.c
	cc -c -o=$@ $<

hello.exe : hello.obj
	link -o=$@ $<
```
If we run "make -n" on this makefile, we will get:
```
cc -c -o=./obj/hello.obj hello.c
link -o=./bin/hello.exe ./obj/hello.obj
```
When we had written "vpath" instead of ".path", there would not have been "./obj" and "./bin" subdirectories on the command lines.

You can clear the VPATH list by putting no path name on the `vpath` command. For the target-path list, you use the same syntax on the `.path` command. You could set a global `.path` by omitting the pattern (but I do not see the point). You may mix `vpath` and `.path`, but there is probably little use in that.

## Other patches
This version also includes the patches:
* make-4.2.1-sub_proc.patch (fixes a bug for Microsoft Windows, see https://github.com/mbuilov/gnumake-windows)
* make-4.2.1-win32-ctrl-c.patch (fix for Ctrl-C support in Microsoft Windows, see https://github.com/mbuilov/gnumake-windows)
* make-4.2.1-SV49841.patch (fix for function arguments incorrectly shown as defined, see http://savannah.gnu.org/bugs/?49841)
