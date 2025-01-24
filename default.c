/* Data base of default implicit rules for GNU Make.
Copyright (C) 1988-2016 Free Software Foundation, Inc.
This file is part of GNU Make.

GNU Make is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

GNU Make is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <assert.h>
#include "makeint.h"
#include "filedef.h"
#include "variable.h"
#include "rule.h"
#include "dep.h"
#include "job.h"
#include "commands.h"
#include "debug.h"

#if defined(WINDOWS32)
# include <windows.h>
# include <shlobj.h>
# include <io.h>
# include "w32/strlcpy.h"
#elif defined __linux__
# include <bsd/string.h>
#endif


static const char *default_variables[] =
  {
    "GNUMAKEFLAGS", "",     /* Make this assignment to avoid undefined variable warnings.  */
    ".RECIPEINDENT", "4",   /* Default indentation for recipe prefixes.  */
    ".space", " ",          /* For use in macros where you need to match on a space.  */
    0, 0
  };

struct textline
  {
    struct textline *next;
    const char *line;
  };
static struct textline textroot = { 0, 0 };

static char *striptrailing(char *line)
{
  char *ptr = strchr(line, '\0');
  assert(ptr);
  while (ptr > line && *(ptr - 1) <= ' ')
    *--ptr = '\0';  /* strip trailing whitespace */
  return line;
}

static char *skipleading(const char *line)
{
  while (*line != '\0' && *line <= ' ')
    line++;         /* skip leading whitespace */
  return (char*)line;
}

static char *collect_commandlines (struct textline *base)
{
  size_t length = 0;
  char *cmdlist;
  struct textline *item;

  for (item = base; item && ISBLANK(item->line[0]); item = item->next)
    length += strlen(skipleading(item->line)) + 1;
  cmdlist = xmalloc((length + 1) * sizeof(char));
  *cmdlist = '\0';
  for (item = base; item && ISBLANK(item->line[0]); item = item->next)
    {
      if (*cmdlist != '\0')
        strcat(cmdlist, "\n");
      strcat(cmdlist, skipleading(item->line));
    }
  return cmdlist;
}

/* Read the make "config" file, with the "built-in" variables, pattern rules
   and suffix rules.
   If `exclusive` is non-zero, `path` is assumed to contain a full file path,
   and an error message is issued when it is not present.
   If `exlusive` is zero and `path` is not `NULL`, it is assumed to be a
   directory only; no warning is issued if no configuration file is found in
   this directory. */
const char *
read_config (const char *path, int exclusive, const char *argv0)
{
  static PATH_VAR(cfgfile);
  char *ptr, *line;
  FILE *fcfg;
  struct textline *item, *tail;

  cfgfile[0] = '\0';

  /* If specific path is set, use only that */
  if (path != NULL)
    {
      if (exclusive)
        {
          strlcpy(cfgfile, path, sizeof(cfgfile));
          if (access (cfgfile, 0) != 0)
              OS (error, NILF,
                  _("warning:  Configuration file '%s' is not found."), cfgfile);
        }
      else
        {
          strlcpy(cfgfile, path, sizeof(cfgfile));
          if ((ptr = strrchr(cfgfile, DIRSEP_C)) == NULL || *(ptr + 1) != '\0')
            strlcat (cfgfile, DIRSEP_S, sizeof cfgfile);
#         if defined(__MSDOS__)
            strlcat (cfgfile, "make.cfg", sizeof cfgfile);
#         else
            strlcat (cfgfile, "make.conf", sizeof cfgfile);
#         endif
        }
    }

  /* First try current directory. */
  if (access (cfgfile, 0) != 0)
    {
      if (getcwd (cfgfile, sizeof cfgfile) != NULL)
        {
#         if defined(__MSDOS__)
            strlcat (cfgfile, DIRSEP_S "make.cfg", sizeof cfgfile);
#         else
            strlcat (cfgfile, DIRSEP_S "make.conf", sizeof cfgfile);
#         endif
        }
    }

  /* Then try the current user's home directory (or the application data folder
     under Windows). */
  if (access (cfgfile, 0) != 0)
    {
#     if defined(__MSDOS__)
        /* There is no user directory under DOS. */
#     elif defined(WINDOWS32)
        if (SHGetFolderPathA (NULL, CSIDL_APPDATA, NULL, 0, cfgfile) == S_OK)
          strlcat (cfgfile, DIRSEP_S "make.conf", sizeof cfgfile);
#     else
        if ((ptr = getenv ("HOME")) != NULL)
          {
            strlcpy (cfgfile, ptr, sizeof cfgfile);
            strlcat (cfgfile, DIRSEP_S "make.conf", sizeof cfgfile);
          }
#     endif
    }

  /* Finally try /etc (or the executable's directory under Windows) */
  if (access (cfgfile, 0) != 0)
    {
#     if defined(__MSDOS__)
        strlcpy (cfgfile, argv0, sizeof cfgfile);
        ptr = strrchr (cfgfile, DIRSEP_C);
        if (ptr != NULL)
          strlcpy (ptr + 1, "make.cfg", sizeof cfgfile - (ptr - cfgfile) - 1);
#     elif defined(WINDOWS32)
        GetModuleFileName (NULL, cfgfile, GET_PATH_MAX);
        ptr = strrchr (cfgfile, DIRSEP_C);
        if (ptr != NULL)
          strlcpy (ptr + 1, "make.conf", sizeof cfgfile - (ptr - cfgfile) - 1);
#     else
        strlcpy (cfgfile, "/etc/make.conf", sizeof cfgfile);
#     endif
    }

  if (access (cfgfile, 0) != 0)
    return NULL;

  fcfg = fopen(cfgfile, "r");
  if (!fcfg)
    return NULL;

  #define LINELENGTH  2048
  line = xmalloc(LINELENGTH * sizeof(char));
  if (!line)
    {
      fclose(fcfg);
      return NULL;
    }
  tail = &textroot;
  while (fgets(line, LINELENGTH, fcfg))
    {
    /* strip trailing whitespace, strip comments, collect lines */
    int concat;
    do
      {
      concat = 0;
      striptrailing(line);
      ptr = strchr(line, '\0');
      if (ptr > line && *(ptr - 1) == '\\')
        {
          *--ptr = '\0';            /* erase \ */
          /* check for double \\ at the end of line */
          if (ptr == line || *(ptr - 1) != '\\')
            {
              concat = 1;           /* next line must be concatenated */
              striptrailing(line);  /* strip trailing whitespace before \ */
            }
        }
      ptr = strchr(line, '#');
      if (ptr)
        {
        if (ptr == line || *(ptr-1) != '\\')
          *ptr = '\0';              /* a comment, terminate the string at the # */
        else
          memmove(ptr - 1, ptr, strlen(ptr) + 1); /* \# appears, delete the \ (and keep the #) */
        }
      if (concat)
        {
          int length = strlen(line);
          if (!fgets(line + length, LINELENGTH - length, fcfg))
            concat = 0;
        }
      }
    while (concat);
    /* append the line to the string list (but ignore empty strings) */
    if (*line)
      {
        item = xmalloc(sizeof(struct textline));
        if (item)
          {
            item->line = xstrdup(line);
            item->next = NULL;
            tail->next = item;
            tail = item;
          }
      }
    }
  fclose(fcfg);
  free(line);

  #undef LINELENGTH
  return cfgfile;
}

/* Free all strings read from the configuration file. */
void
clear_config (void)
{
  while (textroot.next)
    {
      struct textline *item = textroot.next;
      textroot.next = item->next; /* unlink first */
      free((void*)item->line);
      free(item);
    }
}


static char *
is_variable (const char *line, char *name, size_t namelength)
{
  char *ptr;
  if (!ISBLANK(line[0]) && (ptr = strchr(line, '=')) != NULL)
    {
      size_t length = ptr - line;
      if (length >= namelength)
        length = namelength - 1;
      strncpy (name, line, length);
      name[length] = '\0';
      while (length > 0 && name[length - 1] == ':')
        name[--length] = '\0';
      striptrailing(name);
      return skipleading(ptr + 1);
    }
  return NULL;
}

char *
get_default_variable (const char *name)
{
  struct textline *item;
  for (item = textroot.next; item; item = item->next)
    {
      char vname[128];
      char *ptr = is_variable (item->line, vname, sizeof (vname));
      if (ptr != NULL && streq(vname, name))
        return ptr;
    }
  return NULL;
}


/* Set up the default .SUFFIXES list.  */
void
set_default_suffixes (void)
{
  suffix_file = enter_file (strcache_add (".SUFFIXES"));
  suffix_file->builtin = 1;

  if (no_builtin_rules_flag)
    define_variable_cname ("SUFFIXES", "", o_default, 0);
  else
    {
      struct textline *item;
      struct dep *d;
      char *suffixes = NULL;
      const char *p;
      /* find all .SUFFIXES pseudo-targets and add them to the list */
      for (item = textroot.next; item; item = item->next)
        {
          p = item->line;
          if (strncmp(p, ".SUFFIXES", 9) == 0 && ISBLANK(p[9]))
            {
              int length = suffixes ? strlen(suffixes) : 0;
              p += 9;   /* skip .SUFFIXES */
              while (*p != '\0' && *p <= ' ')
                p++;
              suffixes = xrealloc(suffixes, (length + strlen(p) + 2) * sizeof(char));
              if (suffixes)
                {
                if (length > 0)
                  suffixes[length++] = ' ';
                strcpy(suffixes + length, p);
                }
            }
        }
      if (suffixes)
        {
          char *p2;
          /* clean up the collected string: replace tabs by spaces and replace
             multiple spaces by a single space */
          for (p2 = suffixes; *p2; p2++)
            if (*p2 == '\t')
              *p2 = ' ';
          for (p2 = suffixes; *p2; p2++)
            {
              if (*p2 == ' ')
                {
                  int count = 1;
                  while (p2[count] == ' ')
                    count++;
                  if (count > 1)
                    memmove(p2 + 1, p2 + count, strlen(p2 + count) + 1);
              }
            }

          p = suffixes;
          suffix_file->deps = enter_prereqs(PARSE_SIMPLE_SEQ((char**)&p, struct dep), NULL);
          for (d = suffix_file->deps; d; d = d->next)
            d->file->builtin = 1;

          define_variable_cname ("SUFFIXES", suffixes, o_default, 0);
        }
      else
        define_variable_cname ("SUFFIXES", "", o_default, 0);
    }
}

/* Enter the default suffix rules as file rules.  This used to be done in
   install_default_implicit_rules, but that loses because we want the
   suffix rules installed before reading makefiles, and the pattern rules
   installed after.  */

void
install_default_suffix_rules (void)
{
  struct textline *item;

  if (no_builtin_rules_flag)
    return;

  /* find all lines that match a suffix rule */
  for (item = textroot.next; item; item = item->next)
    {
      char *ptr;
      if (item->line[0] == '.' && (ptr = strchr(item->line, ':')) != NULL && *(ptr + 1) == '\0')
        {
          struct file *f;
          *ptr = '\0';  /* erase ':' */
          striptrailing ((char*)item->line);
          f = enter_file (strcache_add (item->line));
          assert (f->cmds == 0);      /* this function should run before any makefile is parsed.  */
          f->cmds = xmalloc (sizeof (struct commands));
          f->cmds->fileinfo.filenm = 0;
          f->cmds->commands = collect_commandlines(item->next);
          f->cmds->command_lines = 0; /* adjusted by chop_commands() */
          f->builtin = 1;
        }
    }
}


/* Install the default pattern rules.  */

void
install_default_implicit_rules (void)
{
  struct textline *item;

  if (no_builtin_rules_flag)
    return;

  for (item = textroot.next; item; item = item->next)
    {
      char *pct, *clm;
      if (!ISBLANK(item->line[0]) && (pct = strchr(item->line, '%')) != NULL && (clm = strchr(pct, ':')) != NULL && *(clm + 1) != '=')
        {
          char target[128];
          struct pspec p;
          int terminal = 0;
          clm++;                  /* skip ':' */
          if (*clm == ':')
            {
            terminal = 1;         /* double column -> this is a terminal rule */
            clm++;                /* skip second ':' */
            }
          assert(clm - item->line < sizeof(target));
          strncpy(target, item->line, clm - item->line);
          target[clm - item->line] = '\0';
          p.target = striptrailing(target); /* part before item->line, striptail() */
          p.dep = skipleading(clm);         /* part after item->line, skiplead */
          p.commands = collect_commandlines(item->next);
          install_pattern_rule (&p, terminal);
        }
    }
}

void
define_default_variables (void)
{
  struct textline *item;
  const char **s;

  /* A few variables are hard-coded (although they can be overriden by the
     configuration file). */
  for (s = default_variables; *s != 0; s += 2)
    define_variable (s[0], strlen (s[0]), s[1], o_default, 1);

  if (no_builtin_variables_flag)
    return;

  for (item = textroot.next; item; item = item->next)
    {
      char vname[128];
      const char *ptr = is_variable (item->line, vname, sizeof (vname));
      if (ptr != NULL)
        define_variable (vname, strlen (vname), ptr, o_default, 1);
    }
}

void
undefine_default_variables (void)
{
  struct textline *item;

  for (item = textroot.next; item; item = item->next)
    {
      char *ptr;
      if (!ISBLANK(item->line[0]) && (ptr = strchr(item->line, '=')) != NULL)
        {
          while (ptr > item->line && (*(ptr - 1) == ':' || *(ptr - 1) == ' '))
            ptr--;
          undefine_variable_global (item->line, ptr - item->line, o_default);
        }
    }
}
