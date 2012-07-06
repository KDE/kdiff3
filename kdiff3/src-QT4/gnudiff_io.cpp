/* File I/O for GNU DIFF.

   Modified for KDiff3 by Joachim Eibl 2003, 2004, 2005.
   The original file was part of GNU DIFF.

   Copyright (C) 1988, 1989, 1992, 1993, 1994, 1995, 1998, 2001, 2002
   Free Software Foundation, Inc.

   GNU DIFF is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU DIFF is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "gnudiff_diff.h"
#include <stdlib.h>

/* Rotate an unsigned value to the left.  */
#define ROL(v, n) ((v) << (n) | (v) >> (sizeof (v) * CHAR_BIT - (n)))

/* Given a hash value and a new character, return a new hash value.  */
#define HASH(h, c) ((c) + ROL (h, 7))

/* The type of a hash value.  */
typedef size_t hash_value;
verify (hash_value_is_unsigned, ! TYPE_SIGNED (hash_value));

/* Lines are put into equivalence classes of lines that match in lines_differ.
   Each equivalence class is represented by one of these structures,
   but only while the classes are being computed.
   Afterward, each class is represented by a number.  */
struct equivclass
{
  lin next;		/* Next item in this bucket.  */
  hash_value hash;	/* Hash of lines in this class.  */
  const QChar *line;	/* A line that fits this class.  */
  size_t length;	/* That line's length, not counting its newline.  */
};

/* Hash-table: array of buckets, each being a chain of equivalence classes.
   buckets[-1] is reserved for incomplete lines.  */
static lin *buckets;

/* Number of buckets in the hash table array, not counting buckets[-1].  */
static size_t nbuckets;

/* Array in which the equivalence classes are allocated.
   The bucket-chains go through the elements in this array.
   The number of an equivalence class is its index in this array.  */
static struct equivclass *equivs;

/* Index of first free element in the array `equivs'.  */
static lin equivs_index;

/* Number of elements allocated in the array `equivs'.  */
static lin equivs_alloc;


/* Check for binary files and compare them for exact identity.  */

/* Return 1 if BUF contains a non text character.
   SIZE is the number of characters in BUF.  */

#define binary_file_p(buf, size) (memchr (buf, 0, size) != 0)

/* Compare two lines (typically one from each input file)
   according to the command line options.
   For efficiency, this is invoked only when the lines do not match exactly
   but an option like -i might cause us to ignore the difference.
   Return nonzero if the lines differ.  */

bool GnuDiff::lines_differ (const QChar *s1, size_t len1, const QChar *s2, size_t len2 )
{
   const QChar *t1 = s1;
   const QChar *t2 = s2;
   const QChar *s1end = s1+len1;
   const QChar *s2end = s2+len2;

   for ( ; ; ++t1, ++t2 )
   {
      /* Test for exact char equality first, since it's a common case.  */
      if ( t1!=s1end && t2!=s2end && *t1==*t2 )
         continue;
      else
      {
         while ( t1!=s1end &&
                 ( (bIgnoreWhiteSpace && isWhite( *t1 ))  ||
                   (bIgnoreNumbers    && (t1->isDigit()  || *t1=='-' || *t1=='.' ))))
         {
            ++t1;
         }

         while ( t2 != s2end &&
                 ( (bIgnoreWhiteSpace && isWhite( *t2 ))  ||
                   (bIgnoreNumbers    && (t2->isDigit() || *t2=='-' || *t2=='.' ))))
         {
            ++t2;
         }

         if ( t1!=s1end && t2!=s2end )
         {
            if (ignore_case)
            {  /* Lowercase comparison. */
               if ( t1->toLower() == t2->toLower() )
                  continue;
            }
            else if ( *t1 == *t2 )
               continue;
            else
               return true;
         }
         else if ( t1==s1end && t2==s2end )
            return false;
         else
            return true;
      }
   }
   return false;
}


/* Split the file into lines, simultaneously computing the equivalence
   class for each line.  */

void GnuDiff::find_and_hash_each_line (struct file_data *current)
{
  hash_value h;
  const QChar *p = current->prefix_end;
  QChar c;
  lin i, *bucket;
  size_t length;

  /* Cache often-used quantities in local variables to help the compiler.  */
  const QChar **linbuf = current->linbuf;
  lin alloc_lines = current->alloc_lines;
  lin line = 0;
  lin linbuf_base = current->linbuf_base;
  lin *cureqs = (lin*)xmalloc (alloc_lines * sizeof *cureqs);
  struct equivclass *eqs = equivs;
  lin eqs_index = equivs_index;
  lin eqs_alloc = equivs_alloc;
  const QChar *suffix_begin = current->suffix_begin;
  const QChar *bufend = current->buffer + current->buffered;
  bool diff_length_compare_anyway =
    ignore_white_space != IGNORE_NO_WHITE_SPACE || bIgnoreNumbers;
  bool same_length_diff_contents_compare_anyway =
    diff_length_compare_anyway | ignore_case;

  while ( p < suffix_begin)
    {
      const QChar *ip = p;

      h = 0;

      /* Hash this line until we find a newline or bufend is reached.  */
      if (ignore_case)
	switch (ignore_white_space)
	  {
	  case IGNORE_ALL_SPACE:
	    while ( p<bufend && (c = *p) != '\n' )
            {
          if (! (isWhite(c) || (bIgnoreNumbers && (c.isDigit() || c=='-' || c=='.' )) ))
                  h = HASH (h, c.toLower().unicode());
              ++p;
            }            
	    break;

	  default:
	    while ( p<bufend && (c = *p) != '\n' )
            {
               h = HASH (h, c.toLower().unicode());
               ++p;
            }
	    break;
	  }
      else
	switch (ignore_white_space)
	  {
	  case IGNORE_ALL_SPACE:
	    while ( p<bufend && (c = *p) != '\n')
            {
          if (! (isWhite(c)|| (bIgnoreNumbers && (c.isDigit() || c=='-' || c=='.' )) ))
                 h = HASH (h, c.unicode());
              ++p;
            }
	    break;

	  default:
	    while ( p<bufend && (c = *p) != '\n')
            {
               h = HASH (h, c.unicode());
               ++p;
            }
	    break;
	  }

      bucket = &buckets[h % nbuckets];
      length = p - ip;
      ++p;

      for (i = *bucket;  ;  i = eqs[i].next)
	if (!i)
	  {
	    /* Create a new equivalence class in this bucket.  */
	    i = eqs_index++;
	    if (i == eqs_alloc)
	      {
		if ((lin)(PTRDIFF_MAX / (2 * sizeof *eqs)) <= eqs_alloc)
		  xalloc_die ();
		eqs_alloc *= 2;
		eqs = (equivclass*)xrealloc (eqs, eqs_alloc * sizeof *eqs);
	      }
	    eqs[i].next = *bucket;
	    eqs[i].hash = h;
	    eqs[i].line = ip;
	    eqs[i].length = length;
	    *bucket = i;
	    break;
	  }
	else if (eqs[i].hash == h)
	  {
	    const QChar *eqline = eqs[i].line;

	    /* Reuse existing class if lines_differ reports the lines
               equal.  */
	    if (eqs[i].length == length)
	      {
		/* Reuse existing equivalence class if the lines are identical.
		   This detects the common case of exact identity
		   faster than lines_differ would.  */
		if (memcmp (eqline, ip, length*sizeof(QChar)) == 0)
		  break;
		if (!same_length_diff_contents_compare_anyway)
		  continue;
	      }
	    else if (!diff_length_compare_anyway)
	      continue;

	    if (! lines_differ (eqline, eqs[i].length, ip, length))
	      break;
	  }

      /* Maybe increase the size of the line table.  */
      if (line == alloc_lines)
	{
	  /* Double (alloc_lines - linbuf_base) by adding to alloc_lines.  */
	  if ((lin)(PTRDIFF_MAX / 3) <= alloc_lines
	      || (lin)(PTRDIFF_MAX / sizeof *cureqs) <= 2 * alloc_lines - linbuf_base
	      || (lin)(PTRDIFF_MAX / sizeof *linbuf) <= alloc_lines - linbuf_base)
	    xalloc_die ();
	  alloc_lines = 2 * alloc_lines - linbuf_base;
	  cureqs =(lin*) xrealloc (cureqs, alloc_lines * sizeof *cureqs);
	  linbuf += linbuf_base;
	  linbuf = (const QChar**) xrealloc (linbuf,
			     (alloc_lines - linbuf_base) * sizeof *linbuf);
	  linbuf -= linbuf_base;
	}
      linbuf[line] = ip;
      cureqs[line] = i;
      ++line;
    }

  current->buffered_lines = line;

  for (i = 0;  ;  i++)
    {
      /* Record the line start for lines in the suffix that we care about.
	 Record one more line start than lines,
	 so that we can compute the length of any buffered line.  */
      if (line == alloc_lines)
	{
	  /* Double (alloc_lines - linbuf_base) by adding to alloc_lines.  */
	  if ((lin)(PTRDIFF_MAX / 3) <= alloc_lines
	      || (lin)(PTRDIFF_MAX / sizeof *cureqs) <= 2 * alloc_lines - linbuf_base
	      || (lin)(PTRDIFF_MAX / sizeof *linbuf) <= alloc_lines - linbuf_base)
	    xalloc_die ();
	  alloc_lines = 2 * alloc_lines - linbuf_base;
	  linbuf += linbuf_base;
	  linbuf = (const QChar**)xrealloc (linbuf,
			     (alloc_lines - linbuf_base) * sizeof *linbuf);
	  linbuf -= linbuf_base;
	}
      linbuf[line] = p;

      if ( p >= bufend)
	break;

      if (context <= i && no_diff_means_no_output)
	break;

      line++;

      while (p<bufend && *p++ != '\n')
        continue;
    }

  /* Done with cache in local variables.  */
  current->linbuf = linbuf;
  current->valid_lines = line;
  current->alloc_lines = alloc_lines;
  current->equivs = cureqs;
  equivs = eqs;
  equivs_alloc = eqs_alloc;
  equivs_index = eqs_index;
}

/* We have found N lines in a buffer of size S; guess the
   proportionate number of lines that will be found in a buffer of
   size T.  However, do not guess a number of lines so large that the
   resulting line table might cause overflow in size calculations.  */
static lin
guess_lines (lin n, size_t s, size_t t)
{
  size_t guessed_bytes_per_line = n < 10 ? 32 : s / (n - 1);
  lin guessed_lines = MAX (1, t / guessed_bytes_per_line);
  return MIN (guessed_lines, (lin)(PTRDIFF_MAX / (2 * sizeof (QChar *) + 1) - 5)) + 5;
}

/* Given a vector of two file_data objects, find the identical
   prefixes and suffixes of each object.  */

void GnuDiff::find_identical_ends (struct file_data filevec[])
{
  /* Find identical prefix.  */
  const QChar *p0, *p1, *buffer0, *buffer1;
  p0 = buffer0 = filevec[0].buffer;
  p1 = buffer1 = filevec[1].buffer;
  size_t n0, n1;
  n0 = filevec[0].buffered;
  n1 = filevec[1].buffered;
  const QChar* const pEnd0 = p0 + n0;
  const QChar* const pEnd1 = p1 + n1;

  if (p0 == p1)
    /* The buffers are the same; sentinels won't work.  */
    p0 = p1 += n1;
  else
    {
      /* Loop until first mismatch, or end. */
      while ( p0!=pEnd0  &&  p1!=pEnd1  &&  *p0 == *p1 )
      {
         p0++;
         p1++;
      }
    }

  /* Now P0 and P1 point at the first nonmatching characters.  */

  /* Skip back to last line-beginning in the prefix. */
  while (p0 != buffer0 && (p0[-1] != '\n' ))
    p0--, p1--;

  /* Record the prefix.  */
  filevec[0].prefix_end = p0;
  filevec[1].prefix_end = p1;

  /* Find identical suffix.  */

  /* P0 and P1 point beyond the last chars not yet compared.  */
  p0 = buffer0 + n0;
  p1 = buffer1 + n1;

   const QChar *end0, *beg0;
   end0 = p0; /* Addr of last char in file 0.  */

   /* Get value of P0 at which we should stop scanning backward:
      this is when either P0 or P1 points just past the last char
      of the identical prefix.  */
   beg0 = filevec[0].prefix_end + (n0 < n1 ? 0 : n0 - n1);

   /* Scan back until chars don't match or we reach that point.  */
   for (; p0 != beg0; p0--, p1--)
   {
      if (*p0 != *p1)
      {
         /* Point at the first char of the matching suffix.  */
         beg0 = p0;
         break;
      }
   }

   // Go to the next line (skip last line with a difference)
   if ( p0 != end0 )
   {
      if (*p0 != *p1)
         ++p0;
      while ( p0<pEnd0 && *p0++ != '\n')
         continue;
   }

   p1 += p0 - beg0;

  /* Record the suffix.  */
  filevec[0].suffix_begin = p0;
  filevec[1].suffix_begin = p1;

  /* Calculate number of lines of prefix to save.

     prefix_count == 0 means save the whole prefix;
     we need this for options like -D that output the whole file,
     or for enormous contexts (to avoid worrying about arithmetic overflow).
     We also need it for options like -F that output some preceding line;
     at least we will need to find the last few lines,
     but since we don't know how many, it's easiest to find them all.

     Otherwise, prefix_count != 0.  Save just prefix_count lines at start
     of the line buffer; they'll be moved to the proper location later.
     Handle 1 more line than the context says (because we count 1 too many),
     rounded up to the next power of 2 to speed index computation.  */

  const QChar **linbuf0, **linbuf1;
  lin alloc_lines0, alloc_lines1;
  lin buffered_prefix, prefix_count, prefix_mask;
  lin middle_guess, suffix_guess;
  if (no_diff_means_no_output
      && context < (lin)(LIN_MAX / 4) && context < (lin)(n0))
    {
      middle_guess = guess_lines (0, 0, p0 - filevec[0].prefix_end);
      suffix_guess = guess_lines (0, 0, buffer0 + n0 - p0);
      for (prefix_count = 1;  prefix_count <= context;  prefix_count *= 2)
	continue;
      alloc_lines0 = (prefix_count + middle_guess
		      + MIN (context, suffix_guess));
    }
  else
    {
      prefix_count = 0;
      alloc_lines0 = guess_lines (0, 0, n0);
    }

  prefix_mask = prefix_count - 1;
  lin lines = 0;
  linbuf0 = (const QChar**) xmalloc (alloc_lines0 * sizeof(*linbuf0));
  p0 = buffer0;

  /* If the prefix is needed, find the prefix lines.  */
  if (! (no_diff_means_no_output
	 && filevec[0].prefix_end == p0
	 && filevec[1].prefix_end == p1))
    {
      end0 = filevec[0].prefix_end;
      while (p0 != end0)
	{
	  lin l = lines++ & prefix_mask;
	  if (l == alloc_lines0)
	    {
	      if ((lin)(PTRDIFF_MAX / (2 * sizeof *linbuf0)) <= alloc_lines0)
		xalloc_die ();
	      alloc_lines0 *= 2;
              linbuf0 = (const QChar**) xrealloc (linbuf0, alloc_lines0 * sizeof(*linbuf0));
	    }
	  linbuf0[l] = p0;
	  while ( p0<pEnd0 && *p0++ != '\n' )
	    continue;
	}
    }
  buffered_prefix = prefix_count && context < lines ? context : lines;

  /* Allocate line buffer 1.  */

  middle_guess = guess_lines (lines, p0 - buffer0, p1 - filevec[1].prefix_end);
  suffix_guess = guess_lines (lines, p0 - buffer0, buffer1 + n1 - p1);
  alloc_lines1 = buffered_prefix + middle_guess + MIN (context, suffix_guess);
  if (alloc_lines1 < buffered_prefix
      || (lin)(PTRDIFF_MAX / sizeof *linbuf1) <= alloc_lines1)
    xalloc_die ();
  linbuf1 = (const QChar**)xmalloc (alloc_lines1 * sizeof(*linbuf1));

  lin i;
  if (buffered_prefix != lines)
  {
      /* Rotate prefix lines to proper location.  */
      for (i = 0;  i < buffered_prefix;  i++)
	linbuf1[i] = linbuf0[(lines - context + i) & prefix_mask];
      for (i = 0;  i < buffered_prefix;  i++)
	linbuf0[i] = linbuf1[i];
  }

  /* Initialize line buffer 1 from line buffer 0.  */
  for (i = 0; i < buffered_prefix; i++)
    linbuf1[i] = linbuf0[i] - buffer0 + buffer1;

  /* Record the line buffer, adjusted so that
     linbuf[0] points at the first differing line.  */
  filevec[0].linbuf = linbuf0 + buffered_prefix;
  filevec[1].linbuf = linbuf1 + buffered_prefix;
  filevec[0].linbuf_base = filevec[1].linbuf_base = - buffered_prefix;
  filevec[0].alloc_lines = alloc_lines0 - buffered_prefix;
  filevec[1].alloc_lines = alloc_lines1 - buffered_prefix;
  filevec[0].prefix_lines = filevec[1].prefix_lines = lines;
}

/* If 1 < k, then (2**k - prime_offset[k]) is the largest prime less
   than 2**k.  This table is derived from Chris K. Caldwell's list
   <http://www.utm.edu/research/primes/lists/2small/>.  */

static unsigned char const prime_offset[] =
{
  0, 0, 1, 1, 3, 1, 3, 1, 5, 3, 3, 9, 3, 1, 3, 19, 15, 1, 5, 1, 3, 9, 3,
  15, 3, 39, 5, 39, 57, 3, 35, 1, 5, 9, 41, 31, 5, 25, 45, 7, 87, 21,
  11, 57, 17, 55, 21, 115, 59, 81, 27, 129, 47, 111, 33, 55, 5, 13, 27,
  55, 93, 1, 57, 25
};

/* Verify that this host's size_t is not too wide for the above table.  */

verify (enough_prime_offsets,
	sizeof (size_t) * CHAR_BIT <= sizeof prime_offset);

/* Given a vector of two file_data objects, read the file associated
   with each one, and build the table of equivalence classes.
   Return nonzero if either file appears to be a binary file.
   If PRETEND_BINARY is nonzero, pretend they are binary regardless.  */

bool
GnuDiff::read_files (struct file_data filevec[], bool /*pretend_binary*/)
{
  int i;

  find_identical_ends (filevec);

  equivs_alloc = filevec[0].alloc_lines + filevec[1].alloc_lines + 1;
  if ((lin)(PTRDIFF_MAX / sizeof *equivs) <= equivs_alloc)
    xalloc_die ();
  equivs = (equivclass*)xmalloc (equivs_alloc * sizeof *equivs);
  /* Equivalence class 0 is permanently safe for lines that were not
     hashed.  Real equivalence classes start at 1.  */
  equivs_index = 1;

  /* Allocate (one plus) a prime number of hash buckets.  Use a prime
     number between 1/3 and 2/3 of the value of equiv_allocs,
     approximately.  */
  for (i = 9;  1 << i < equivs_alloc / 3; i++)
    continue;
  nbuckets = ((size_t) 1 << i) - prime_offset[i];
  if (PTRDIFF_MAX / sizeof *buckets <= nbuckets)
    xalloc_die ();
  buckets = (lin*)zalloc ((nbuckets + 1) * sizeof *buckets);
  buckets++;

  for (i = 0; i < 2; i++)
    find_and_hash_each_line (&filevec[i]);

  filevec[0].equiv_max = filevec[1].equiv_max = equivs_index;

  free (equivs);
  free (buckets - 1);

  return 0;
}
