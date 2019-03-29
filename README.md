# GNU Make - patched
This is a version of GNU make with a few additions, many of which are inspired by Opus Make.

## .path - VPATH for targets
The new `.path` command is essentially a `VPATH` for targets. It allows you to easily separate intermediate files and output files from source files, while keeping the makefile clean and simple. For a motivation for adding `.path` see the [Wiki](../../wiki/Rationale-for-the-.path-directive)

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

## Descriptive aliases for automatic variables
The predefined variables for targets and prerequisites are cryptic and difficult to remember. This version adds variables with longer (descriptive) names, as an alias for these automatic variables.

| variable | alias        | Brief description |
| -------- | ------------ | ----------------- |
| $@       | $(.TARGET)	  | The path to the target of a rule. |
| $<       | $(.SOURCE)	  | The path name of the inferred source, or the path of the first explicit source. |
| $?       | $(.NEWSOURCES) |A space-separated list of all prerequisites that are newer than the target. |
| $^       | $(.SOURCES)  | A space-separated list of all prerequisites, where duplicate names have been removed. |
| $+       | $(.SOURCES+) | A space-separated list of all prerequisites, without removing duplicate names. |
| $*       |              | The stem with which an implicit rule matches (or in an explicit rule, $@ without its suffix). |
| $%       |              | The name of the member in the target, in the case that the target is a library. |
| $&vert;  |              | A space-separated list of order-only prerequisites. |

Note: the descriptions of these macros is terse and incomplete. Look up the full function of these macros in the GNU manual.

## A predefined variable for the space character
We use some tools that require a list of files separated with commas on the input line. GNU make separates the prerequisites by space characters. This can be converted with the `$(subst ...)` function, like in:
```
comma = ,

all:
	process output.db $(subst $(.SPACE),$(comma),$(.SOURCES))
```
Although you can create a definition for `$(.space)` in a makefile, it is a bit of a kludge. Therefore, a macro for a space character is now predefined.

The .space predefined variable is also useful when you want to indent recipes in the makefile with spaces rather than a tab. In order to do this, you need to set the predefined variable .RECIPEPREFIX to a space, and a convenient way to do so is by setting it to $(.space). For example:
```
.RECIPEPREFIX := $(.space)
```
Note that the `.space` variable must be expanded immediately in this case; use `:=` rather than `=`.

## "Built-in" rules and macros have moved to a configuration file
The built-in implicit rules, suffix rules and variable/macro definition are stored in a configuration file. The file is called `make.conf` (or `make.cfg` in DOS). It is located in the `/etc` directory for Unix-like operating systems, and in the same directory that the `make` executbale is in under Microsoft Windows and DOS.

The configuration file uses a subset of the syntax for makefiles. Essentially, it supports defintions of implicit rules, suffix rules and variables that are set with the `=` or `:=` operator.

A few examples are provided in the `configfiles` directory of this repository. These configurations were extracted from the GNU Make source code.

For a motivation for moving the built-in configuration out of the executable and into an editable text file, see the [Wiki](../../wiki/Rationale-for-a-configuration-file).

## Other patches
This version also includes the patches:
* make-4.2.1-sub_proc.patch (fixes a bug for Microsoft Windows, see https://github.com/mbuilov/gnumake-windows)
* make-4.2.1-win32-ctrl-c.patch (fix for Ctrl-C support in Microsoft Windows, see https://github.com/mbuilov/gnumake-windows)
* make-4.2.1-SV49841.patch (fix for function arguments incorrectly shown as defined, see http://savannah.gnu.org/bugs/?49841)
* `--warn-undefined-variables` is now the default, use `--no-warn-undefined-variables` to turn it off.
