/* Record version and build host architecture for GNU make.
Copyright (C) 1988-2022 Free Software Foundation, Inc.
This file is part of GNU Make.

GNU Make is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

GNU Make is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* We use <config.h> instead of "config.h" so that a compilation
   using -I. -I$srcdir will use ./config.h rather than $srcdir/config.h
   (which it would do because makeint.h was found in $srcdir).  */
#include <config.h>

#ifndef MAKE_HOST
# if defined (WINDOWS32) || defined (WIN32) || defined (_WIN32)
#   define MAKE_HOST "Windows"
# elif defined (__linux__) || defined (__linux)
#   define MAKE_HOST "Linux"  /* Linux & derivatives, including Android */
# elif defined (__APPLE__)
#   define MAKE_HOST "Darwin" /* OSX, MacOS, iOS */
# elif defined (__unix__) || defined (__unix)
#   define MAKE_HOST "Unix"
# elif defined (__MSDOS__)
#   define MAKE_HOST "MSDOS"
# else
#   define MAKE_HOST "unknown"
# endif
#endif

const char *version_string = PACKAGE_VERSION;
const char *make_host = MAKE_HOST;

/*
  Local variables:
  version-control: never
  End:
 */
