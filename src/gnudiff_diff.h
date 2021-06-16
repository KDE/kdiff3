/*
 Shared definitions for GNU DIFF
    Modified for KDiff3 by Joachim Eibl <joachim.eibl at gmx.de> 2003, 2004, 2005.
    The original file was part of GNU DIFF.

    Part of KDiff3 - Text Diff And Merge Tool

    SPDX-FileCopyrightText: 1988-2002 Free Software Foundation, Inc.
    SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
    SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef GNUDIFF_DIFF_H
#define GNUDIFF_DIFF_H

#include "LineRef.h"
#include "Utils.h"

#include <QtGlobal>

#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>

#include <stdio.h>

#include <QString>

/* The integer type of a line number. */
typedef qint64 GNULineRef;
#define GNULINEREF_MAX std::numeric_limits<GNULineRef>::max()
static_assert(std::is_signed<GNULineRef>::value, "GNULineRef must be signed.");
static_assert(sizeof(GNULineRef) >= sizeof(size_t), "GNULineRef must be able to receive size_t values.");

class GnuDiff
{
  public:
    /* Variables for command line options */

    /* Nonzero if output cannot be generated for identical files.  */
    bool no_diff_means_no_output;

    /* Number of lines of context to show in each set of diffs.
   This is zero when context is not to be shown.  */
    GNULineRef context;

    /* The significance of white space during comparisons.  */
    enum
    {
        /* All white space is significant (the default).  */
        IGNORE_NO_WHITE_SPACE,

        /* Ignore changes due to tab expansion (-E).  */
        IGNORE_TAB_EXPANSION,

        /* Ignore changes in horizontal white space (-b).  */
        IGNORE_SPACE_CHANGE,

        /* Ignore all horizontal white space (-w).  */
        IGNORE_ALL_SPACE
    } ignore_white_space;

    /* Ignore changes that affect only numbers. (J. Eibl)  */
    bool bIgnoreNumbers;
    bool bIgnoreWhiteSpace;

    /* Files can be compared byte-by-byte, as if they were binary.
   This depends on various options.  */
    bool files_can_be_treated_as_binary;

    /* Ignore differences in case of letters (-i).  */
    bool ignore_case;

    /* Use heuristics for better speed with large files with a small
   density of changes.  */
    bool speed_large_files;

    /* Don't discard lines.  This makes things slower (sometimes much
   slower) but will find a guaranteed minimal set of changes.  */
    bool minimal;

    /* The result of comparison is an "edit script": a chain of `struct change'.
   Each `struct change' represents one place where some lines are deleted
   and some are inserted.

   LINE0 and LINE1 are the first affected lines in the two files (origin 0).
   DELETED is the number of lines deleted here from file 0.
   INSERTED is the number of lines inserted here in file 1.

   If DELETED is 0 then LINE0 is the number of the line before
   which the insertion was done; vice versa for INSERTED and LINE1.  */

    struct change {
        change *link; /* Previous or next edit command  */
        GNULineRef inserted; /* # lines of file 1 changed here.  */
        GNULineRef deleted;  /* # lines of file 0 changed here.  */
        GNULineRef line0;    /* Line number of 1st deleted line.  */
        GNULineRef line1;    /* Line number of 1st inserted line.  */
        bool ignore;         /* Flag used in context.c.  */
    };

    /* Structures that describe the input files.  */

    /* Data on one input file being compared.  */

    struct file_data {
        /* Buffer in which text of file is read.  */
        const QChar *buffer;

        /* Allocated size of buffer, in QChars.  Always a multiple of
       sizeof(*buffer).  */
        size_t bufsize;

        /* Number of valid bytes now in the buffer.  */
        size_t buffered;

        /* Array of pointers to lines in the file.  */
        const QChar **linbuf;

        /* linbuf_base <= buffered_lines <= valid_lines <= alloc_lines.
       linebuf[linbuf_base ... buffered_lines - 1] are possibly differing.
       linebuf[linbuf_base ... valid_lines - 1] contain valid data.
       linebuf[linbuf_base ... alloc_lines - 1] are allocated.  */
        GNULineRef linbuf_base, buffered_lines, valid_lines, alloc_lines;

        /* Pointer to end of prefix of this file to ignore when hashing.  */
        const QChar *prefix_end;

        /* Count of lines in the prefix.
       There are this many lines in the file before linbuf[0].  */
        GNULineRef prefix_lines;

        /* Pointer to start of suffix of this file to ignore when hashing.  */
        const QChar *suffix_begin;

        /* Vector, indexed by line number, containing an equivalence code for
       each line.  It is this vector that is actually compared with that
       of another file to generate differences.  */
        GNULineRef *equivs;

        /* Vector, like the previous one except that
       the elements for discarded lines have been squeezed out.  */
        GNULineRef *undiscarded;

        /* Vector mapping virtual line numbers (not counting discarded lines)
       to real ones (counting those lines).  Both are origin-0.  */
        GNULineRef *realindexes;

        /* Total number of nondiscarded lines.  */
        GNULineRef nondiscarded_lines;

        /* Vector, indexed by real origin-0 line number,
       containing TRUE for a line that is an insertion or a deletion.
       The results of comparison are stored here.  */
        bool *changed;

        /* 1 if at end of file.  */
        bool eof;

        /* 1 more than the maximum equivalence value used for this or its
       sibling file.  */
        GNULineRef equiv_max;
    };

    /* Data on two input files being compared.  */

    struct comparison {
        file_data file[2];
        comparison const *parent; /* parent, if a recursive comparison */
    };

    /* Describe the two files currently being compared.  */

    file_data files[2];

    /* Declare various functions.  */

    /* analyze.c */
    change *diff_2_files(comparison *);
    /* io.c */
    bool read_files(file_data[], bool);

    /* util.c */
    bool lines_differ(const QChar *, size_t, const QChar *, size_t);
    void *zalloc(size_t);

  private:
    // gnudiff_analyze.cpp
    GNULineRef diag(GNULineRef xoff, GNULineRef xlim, GNULineRef yoff, GNULineRef ylim, bool find_minimal, struct partition *part) const;
    void compareseq(GNULineRef xoff, GNULineRef xlim, GNULineRef yoff, GNULineRef ylim, bool find_minimal);
    void discard_confusing_lines(file_data filevec[]);
    void shift_boundaries(file_data filevec[]);
    change *add_change(GNULineRef line0, GNULineRef line1, GNULineRef deleted, GNULineRef inserted, change *old);
    change *build_reverse_script(file_data const filevec[]);
    change *build_script(file_data const filevec[]);

    // gnudiff_io.cpp
    GNULineRef guess_lines(GNULineRef n, size_t s, size_t t);
    void find_and_hash_each_line(file_data *current);
    void find_identical_ends(file_data filevec[]);

    // gnudiff_xmalloc.cpp
    void *xmalloc(size_t n);
    void *xrealloc(void *p, size_t n);
    void xalloc_die();
}; // class GnuDiff

#endif
