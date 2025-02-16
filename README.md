# Make+
This is a forked version of GNU 4.2.1 Make with a few additions and changes to enhance its ease of use. Several of these changes are inspired by Opus Make version 6.12 from 1998. In adding these features to GNU Make, I have come to appreciate how advanced Opus Make was 25 years ago. After the fork, GNU Make has had releases 4.3 and 4.4; the user-visible changes of these releases have been merged to Make+.

## A recipe starts with an indent of 4 spaces or a TAB character
Any combination of white-space that gives a visible indentation of four spaces or more, is valid for a recipe line (a shell line below a rule). Existing makefiles (that use TABs) still work; one or more spaces followed by a TAB is fine too. The [Wiki](../../wiki/Rationale-for-the-recipe-prefix) has further motivation for changing the recipe prefix.

Four spaces (or a TAB) is the default criterion, by the way. There is a variable to set it to a different value, see the [Wiki](../../wiki/Rationale-for-the-recipe-prefix).

At the same time, the special variable `.RECIPEPREFIX` is removed, motivation for removing it is also in the [Wiki](../../wiki/Rationale-for-removing-special-variable-.RECIPEPREFIX).

## .path - VPATH for targets
The new `.path` command is essentially a `VPATH` for targets. It allows you to easily separate intermediate files and output files from source files, while keeping the makefile clean and simple. For a motivation for adding `.path` see the [Wiki](../../wiki/Rationale-for-the-.path-directive)

The `.path` command functions essentially the same as `vpath`, it also uses the same syntax. The difference is that when a prerequisite is *not* found (in any of the paths), and thus must be created by a rule, *where* it will be created:
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

You can clear the VPATH list by putting no path name on the `vpath` command. For the target-path list, you use the same syntax on the `.path` command. You could set a global `.path` by omitting the pattern (but I do not see the point). You may mix `vpath` and `.path` for the same extension, but there is probably little use in that.

## Descriptive aliases for automatic variables
The predefined variables for targets and prerequisites are cryptic and difficult to remember. This version adds variables with longer (descriptive) names, as an alias for these automatic variables.

| variable | alias        | Brief description |
| -------- | ------------ | ----------------- |
| $@       | $(.TARGET)	  | The path to the target of a rule. |
| $<       | $(.SOURCE)	  | The path name of the inferred source, or the path of the first explicit source. |
| $?       | $(.NEWSOURCES) |A space-separated list of all prerequisites that are newer than the target. |
| $^       | $(.SOURCES)  | A space-separated list of all prerequisites, where duplicate names have been removed. |
| $+       | $(.SOURCES+) | A space-separated list of all prerequisites, without removing duplicate names. |
| $*       | $(.STEM)     | The stem with which an implicit rule matches (or in an explicit rule, $@ without its suffix). |
| $%       |              | The name of the member in the target, in the case that the target is a library. |
| $&vert;  |              | A space-separated list of order-only prerequisites. |

Note: the descriptions of these macros is terse and incomplete. Look up the full function of these macros in the GNU manual. An updated manual, focusing on this fork, is part of this project; see the "doc" folder.

## Predefined variables for the space and "newline" characters
We use some tools that require a list of files separated with commas on the input line. GNU make separates the prerequisites by space characters. This can be converted with the `$(subst ...)` function, like in:
```
comma = ,

all:
	process output.db $(subst $(.space),$(comma),$(.SOURCES))
```
Although you can create a definition for `$(.space)` in a makefile, it is a bit of a kludge. Therefore, a macro for a space character is now predefined.

The same reasoning applies to the newline character: there is a way to create a definition for a newline in a makefile, but it is a kludge. Therefore, macro `$(.newline)` is predefined as well.

## "Built-in" rules and macros have moved to a configuration file
The built-in pattern rules, suffix rules and variable/macro definition are stored in a configuration file. The file is called `make.conf` (or `make.cfg` in DOS). It is located in the `/etc` directory for Unix-like operating systems, and in the same directory that the `make` executable is in under Microsoft Windows and DOS.

The configuration file uses a subset of the syntax for makefiles. Essentially, it supports definitions of pattern rules, suffix rules and macros that are set with the `=` or `:=` operators.

A few standard configurations are provided in the `configfiles` directory of this repository. These configurations were extracted from the GNU Make source code. To use one of these configurations, rename the respective file to `make.conf` and place it in the appropriate location.

Make+ searches for a configuration file in the current directory, the user's "home" directory, and in a system path (in that order). The system path is `/etc` on Unix-like systems, and the same path as where the `make` executable is on Microsoft Windows. Furthermore, it supports the `-c` option (`--config-file`) to select a configuragion file explicitly, and the `-C` option (`--directory`) is also handled to set a location to load the configuration file from.

Without a `make.conf` file, `make` still runs fine. You will have no built-in rules or macros, though. (You may actually prefer to use only explicit rules and macros in your makefile).

For a motivation for moving the built-in configuration out of the executable and into an editable text file, see the [Wiki](../../wiki/Rationale-for-a-configuration-file).

## "ifdef" tests for "if defined"
The semantics of `ifdef` have changed to test for whether a macro is defined. The semantics of the official GNU Make tests whether the macro is non-empty. See the [Wiki](../../wiki/Rationale-for-fixing-ifdef) for the motivation for this change.

To test whether a macro is non-empty, you must now use:
```
ifset $(var)
...
endif
```

The inverse, test whether a macro is empty, is done with:
```
ifclear $(var)
...
endif
```

Note that if a macro only contains white-space, it is considered empty.

The `ifset` and `ifclear` directives amounts to the same as using `ifeq` or `ifneq` and setting the second parameter to nothing. For example, `ifset` is equivalent to:
```
ifneq($(var),)
...
endif
```
One detail is that `ifeq` and `ifneq` do not ignore white-space in the evaluation of the macro, whereas `ifset` and `ifclear` do.

## Other patches
This version also includes the patches:
* make-4.2.1-sub_proc.patch (fixes a bug for Microsoft Windows, see https://github.com/mbuilov/gnumake-windows)
* make-4.2.1-win32-ctrl-c.patch (fix for Ctrl-C support in Microsoft Windows, see https://github.com/mbuilov/gnumake-windows)
* make-4.2.1-SV49841.patch (fix for function arguments incorrectly shown as defined, see http://savannah.gnu.org/bugs/?49841)
* `--warn-undefined-variables` is now the default, use `--no-warn-undefined-variables` to turn it off.
