/* xmalloc.c -- malloc with out of memory checking

   Modified for KDiff3 by Joachim Eibl 2003.
   The original file was part of GNU DIFF.

   Copyright (C) 1990-1999, 2000, 2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.  */

# include <config-kdiff3.h>

#include <sys/types.h>


#include <stdlib.h>
#include <string.h>


#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

#include "gnudiff_diff.h"
/* If non NULL, call this function when memory is exhausted. */
//void (*xalloc_fail_func) PARAMS ((void)) = 0;
void (*xalloc_fail_func)(void) = 0;


void GnuDiff::xalloc_die (void)
{
  if (xalloc_fail_func)
    (*xalloc_fail_func) ();
  //error (exit_failure, 0, "%s", _(xalloc_msg_memory_exhausted));
  /* The `noreturn' cannot be given to error, since it may return if
     its first argument is 0.  To help compilers understand the
     xalloc_die does terminate, call exit. */
  exit (EXIT_FAILURE);
}

/* Allocate N bytes of memory dynamically, with error checking.  */

void *
GnuDiff::xmalloc (size_t n)
{
  void *p;

  p = malloc (n == 0 ? 1 : n); // There are systems where malloc returns 0 for n==0.
  if (p == 0)
    xalloc_die ();
  return p;
}

/* Change the size of an allocated block of memory P to N bytes,
   with error checking.  */

void *
GnuDiff::xrealloc (void *p, size_t n)
{
  p = realloc (p, n==0 ? 1 : n);
  if (p == 0)
    xalloc_die ();
  return p;
}


/* Yield a new block of SIZE bytes, initialized to zero.  */

void *
GnuDiff::zalloc (size_t size)
{
  void *p = xmalloc (size);
  memset (p, 0, size);
  return p;
}
