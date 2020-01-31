/* xmalloc.c -- malloc with out of memory checking

   Modified for KDiff3 by Joachim Eibl 2003.
   The original file was part of GNU DIFF.


    Part of KDiff3 - Text Diff And Merge Tool
   
    SPDX-FileCopyrightText: 1988-2002 Free Software Foundation, Inc.
    SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
    SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <stdlib.h>
#include <string.h>

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#include "gnudiff_diff.h"
/* If non NULL, call this function when memory is exhausted. */
void (*xalloc_fail_func)() = nullptr;

void GnuDiff::xalloc_die()
{
    if(xalloc_fail_func)
        (*xalloc_fail_func)();
    //error (exit_failure, 0, "%s", _(xalloc_msg_memory_exhausted));
    /* The `noreturn' cannot be given to error, since it may return if
     its first argument is 0.  To help compilers understand the
     xalloc_die does terminate, call exit. */
    exit(EXIT_FAILURE);
}

/* Allocate N bytes of memory dynamically, with error checking.  */

void *
GnuDiff::xmalloc(size_t n)
{
    void *p;

    p = malloc(n == 0 ? 1 : n); // There are systems where malloc returns 0 for n==0.
    if(p == nullptr)
        xalloc_die();
    return p;
}

/* Change the size of an allocated block of memory P to N bytes,
   with error checking.  */

void *
GnuDiff::xrealloc(void *p, size_t n)
{
    p = realloc(p, n == 0 ? 1 : n);
    if(p == nullptr)
        xalloc_die();
    return p;
}

/* Yield a new block of SIZE bytes, initialized to zero.  */

void *
GnuDiff::zalloc(size_t size)
{
    void *p = xmalloc(size);
    memset(p, 0, size);
    return p;
}
