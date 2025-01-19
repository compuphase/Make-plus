/* Path conversion for Windows pathnames.
Copyright (C) 1996-2016 Free Software Foundation, Inc.
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
#include <string.h>
#include <stdlib.h>
#include "pathstuff.h"

#if defined(WINDOWS32)
# include "w32/strlcpy.h"
#endif

/** Convert a delimiter separated vpath to canonical format. Since the path can
 *  grow (and shrink) in the conversion, this routine returns a newly allocated
 *  string (that must be free'd).
 *
 *  \param Path     The path in Windows-specific  format. This parameter must be
 *                  valid (it should not be NULL).
 *  \param delim    The delimiter to separate paths in a list (typically a ; in
 *                  Windows).
 *
 *  \return The path in canonical format, or NULL on failure. Failure occurs on
 *          insufficient memory or when `Path` is an empty string.
 *
 *  \note This function handles:
 *        - strings already in canonical format (no change)
 *        - strings with escaped spaces, but delimited in Windows' style (the
 *          delimiter is replaced with a space, no other changes)
 *        - strings with paths in double quotes (spaces are escaped and double
 *          quotes are removed)
 *        - strings with spaces _not_ in double quotes, but delimited Windows'
 *          style (spaces are escaped, delimiters are replaced)
 */
char *
convert_vpath_from_windows32(const char *Path, char delim)
{
    const char *src;
    char *cpath, *tgt;
    int instring, isescaped, delim_found;
    size_t size;

    assert(Path);
    while (*Path != '\0' && ISBLANK((unsigned char)*Path))
        Path++;
    if (*Path == '\0')
        return NULL;

    /* check how many bytes are needed; for the count, it is assumed that all
       spaces need escaping, because allocating a few bytes too many isn't a
       problem; at the same time, we check for delimiters outside quoted strings */
    delim_found = 0;
    instring = 0;
    size = 0;
    for (src = Path; *src != '\0'; src++) {
        size += (ISBLANK((unsigned char)*src)) ? 2 : 1;
        if (*src == '"')
            instring = !instring;
        else if (*src == delim && !instring)
            delim_found = 1;
    }

    /* allocate memory */
    cpath = xmalloc(size + 1);
    if (!cpath)
        return NULL;

    /* now do the conversion */
    src = Path;
    tgt = cpath;
    instring = 0;
    isescaped = 0;
    while (*src != '\0') {
        if (*src == '"') {
            /* skip the double quote and toggle escaping spaces */
            instring = !instring;
            isescaped = 0;
        } else if (ISBLANK((unsigned char)*src) && !isescaped && (instring || delim_found)) {
            /* escape the space */
            *tgt++ = '\\';
            *tgt++ = ' ';
        } else if (*src == delim && !instring) {
            /* replace delimitor by space (but only outside quoted strings) */
            while (tgt > cpath && ISBLANK((unsigned char)*(tgt - 1)))
                tgt--;  /* remove spaces before the delimiter */
            *tgt++=' ';
            while (ISBLANK((unsigned char)*(src + 1)))
                src++;  /* skip spaces behind the delimiter */
        } else {
            *tgt++ = *src;
        }
        /* upon seeing a \, the next character is considered escaped, but a
           double \ cancels this (and inside a quoted string, a \ does not have
           a special meaning) */
        if (!instring && *src == '\\')
            isescaped ^= 1;
        else
            isescaped = 0;
        src++;
    }
    assert((tgt - cpath) <= size);
    while (tgt > cpath && ISBLANK((unsigned char)*(tgt - 1)))
        tgt--;  /* remove trailing spaces */
    *tgt = '\0';

    return cpath;
}

/** Convert Canonical format to Windows-specific format. This means that if
 *  there are escaped spaces in a path name, that path name must be enclosed in
 *  double quotes. As a result, the path can grow (and shrink). The returned
 *  string is therefore allocated dynamically (and must be free'd).
 *
 *  \param Path     The path in canonical format. This parameter must be valid
 *                  (it should not be NULL).
 *  \param delim    The delimiter to insert between multiple path names in a
 *                  list.
 */
char *
convert_Path_to_windows32(const char *Path, char delim)
{
    const char *src;
    char *wpath, *tgt, *mark;
    int enquote, instring;
    size_t size;

    assert(Path);
    while (*Path != '\0' && ISBLANK((unsigned char)*Path))
        Path++;
    if (*Path == '\0')
        return NULL;

    /* dry run, check how many bytes are needed */
    src = Path;
    enquote = 0;
    instring = 0;
    size = 0;
    while (*src != '\0') {
        if (!instring && *src == '\\' && ISBLANK((unsigned char)*(src + 1))) {
            enquote = 1;
            src++;
            size++;
        } else if (!instring && ISBLANK((unsigned char)*src)) {
            if (enquote) {
                size += 2;  /* +2 for double-quotes */
                enquote = 0;
            }
            size += 1;      /* +1 for delimiter */
        } else {
            if (*src == '"')
                instring = !instring;
            size++;
        }
        src++;
    }
    if (enquote)
        size += 2;

    /* allocate memory */
    wpath = xmalloc(size + 1);
    if (!wpath)
        return NULL;

    /* now do the conversion */
    src = Path;
    tgt = wpath;
    mark = tgt;
    enquote = 0;
    instring = 0;
    while (*src != '\0') {
        if (!instring && *src == '\\' && ISBLANK((unsigned char)*(src + 1))) {
            /* escaped space, convert to normal space, but remember that we did
               this) */
            enquote = 1;
            src++;      /* skip '\\' (space is skipped at end of the loop) */
            *tgt++ = ' ';
        } else if (!instring && ISBLANK((unsigned char)*src)) {
            if (enquote) {
                /* escaped spaces found (which were translated to plain spaces),
                   now we need to enclose the path in double-quotes (which is
                   why we kept a mark to the start of the path) */
                *tgt++ = '"';
                memmove(mark + 1, mark, tgt - mark);
                *mark = '"';
                tgt++;  /* add 1 for the inserted double-quote at the mark */
                enquote = 0;
            }
            *tgt++ = delim;
            mark = tgt;
        } else {
            if (*src == '"')
                instring = !instring;
            *tgt++ = *src;
        }
        src++;
    }
    if (enquote) {
        /* enclose last segment in double quotes */
        *tgt++ = '"';
        memmove(mark + 1, mark, tgt - mark);
        *mark = '"';
        tgt++;  /* add 1 for the inserted double-quote at the mark */
    }
    assert(tgt-wpath==size);
    *tgt = '\0';

    return wpath;
}

/*
 * Convert to forward slashes. Resolve to full pathname optionally
 */
char *
convert_slashes(const char *filename, int resolve)
{
    static char w32_path[FILENAME_MAX];
    char *p;

    if (resolve)
        _fullpath(w32_path, filename, sizeof (w32_path));
    else
        strlcpy(w32_path, filename, sizeof (w32_path));

    for (p = w32_path; p && *p; p++)
        if (*p == '\\')
            *p = '/';

    return w32_path;
}

char *
getcwd_fs(char* buf, int len)
{
        char *p = getcwd(buf, len);

        if (p) {
                char *q = convert_slashes(buf, 0);
                strlcpy(buf, q, len);
        }

        return p;
}

#ifdef unused
/*
 * Convert delimiter separated pathnames (e.g. PATH) or single file pathname
 * (e.g. c:/foo, c:\bar) to NutC format. If we are handed a string that
 * _NutPathToNutc() fails to convert, just return the path we were handed
 * and assume the caller will know what to do with it (It was probably
 * a mistake to try and convert it anyway due to some of the bizarre things
 * that might look like pathnames in makefiles).
 */
char *
convert_path_to_nutc(char *path)
{
    int  count;            /* count of path elements */
    char *nutc_path;     /* new NutC path */
    int  nutc_path_len;    /* length of buffer to allocate for new path */
    char *pathp;        /* pointer to nutc_path used to build it */
    char *etok;            /* token separator for old path */
    char *p;            /* points to element of old path */
    char sep;            /* what flavor of separator used in old path */
    char *rval;

    /* is this a multi-element path ? */
    for (p = path, etok = strpbrk(p, ":;"), count = 0;
         etok;
         etok = strpbrk(p, ":;"))
        if ((etok - p) == 1) {
            if (*(etok - 1) == ';' ||
                *(etok - 1) == ':') {
                p = ++etok;
                continue;    /* ignore empty bucket */
            } else if (etok = strpbrk(etok+1, ":;"))
                /* found one to count, handle drive letter */
                p = ++etok, count++;
            else
                /* all finished, force abort */
                p += strlen(p);
        } else
            /* found another one, no drive letter */
            p = ++etok, count++;

    if (count) {
        count++;    /* x1;x2;x3 <- need to count x3 */

        /*
         * Hazard a guess on how big the buffer needs to be.
         * We have to convert things like c:/foo to /c=/foo.
         */
        nutc_path_len = strlen(path) + (count*2) + 1;
        nutc_path = xmalloc(nutc_path_len);
        pathp = nutc_path;
        *pathp = '\0';

        /*
         * Loop through PATH and convert one elemnt of the path at at
         * a time. Single file pathnames will fail this and fall
         * to the logic below loop.
         */
        for (p = path, etok = strpbrk(p, ":;");
             etok;
             etok = strpbrk(p, ":;")) {

            /* don't trip up on device specifiers or empty path slots */
            if ((etok - p) == 1)
                if (*(etok - 1) == ';' ||
                    *(etok - 1) == ':') {
                    p = ++etok;
                    continue;
                } else if ((etok = strpbrk(etok+1, ":;")) == NULL)
                    break;    /* thing found was a WINDOWS32 pathname */

            /* save separator */
            sep = *etok;

            /* terminate the current path element -- temporarily */
            *etok = '\0';

#ifdef __NUTC__
            /* convert to NutC format */
            if (_NutPathToNutc(p, pathp, 0) == FALSE) {
                free(nutc_path);
                rval = savestring(path, strlen(path));
                return rval;
            }
#else
            *pathp++ = '/';
            *pathp++ = p[0];
            *pathp++ = '=';
            *pathp++ = '/';
            strcpy(pathp, &p[2]);
#endif

            pathp += strlen(pathp);
            *pathp++ = ':';     /* use Unix style path separtor for new path */
            *pathp   = '\0'; /* make sure we are null terminaed */

            /* restore path separator */
            *etok = sep;

            /* point p to first char of next path element */
            p = ++etok;

        }
    } else {
        nutc_path_len = strlen(path) + 3;
        nutc_path = xmalloc(nutc_path_len);
        pathp = nutc_path;
        *pathp = '\0';
        p = path;
    }

    /*
      * OK, here we handle the last element in PATH (e.g. c of a;b;c)
     * or the path was a single filename and will be converted
     * here. Note, testing p here assures that we don't trip up
     * on paths like a;b; which have trailing delimiter followed by
     * nothing.
     */
    if (*p != '\0') {
#ifdef __NUTC__
        if (_NutPathToNutc(p, pathp, 0) == FALSE) {
            free(nutc_path);
            rval = savestring(path, strlen(path));
            return rval;
        }
#else
        *pathp++ = '/';
        *pathp++ = p[0];
        *pathp++ = '=';
        *pathp++ = '/';
        strcpy(pathp, &p[2]);
#endif
    } else
        *(pathp-1) = '\0'; /* we're already done, don't leave trailing : */

    rval = savestring(nutc_path, strlen(nutc_path));
    free(nutc_path);
    return rval;
}

#endif
