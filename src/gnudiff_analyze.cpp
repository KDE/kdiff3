/* Analyze file differences for GNU DIFF.

   Modified for KDiff3 by Joachim Eibl <joachim.eibl at gmx.de> 2003.
   The original file was part of GNU DIFF.


    Part of KDiff3 - Text Diff And Merge Tool

    SPDX-FileCopyrightText: 1988-2002 Free Software Foundation, Inc.
    SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
    SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* The basic algorithm is described in:
   "An O(ND) Difference Algorithm and its Variations", Eugene Myers,
   Algorithmica Vol. 1 No. 2, 1986, pp. 251-266;
   see especially section 4.2, which describes the variation used below.
   Unless the --minimal option is specified, this code uses the TOO_EXPENSIVE
   heuristic, by Paul Eggert, to limit the cost to O(N**1.5 log N)
   at the price of producing suboptimal output for large inputs with
   many differences.

   The basic algorithm was independently discovered as described in:
   "Algorithms for Approximate String Matching", E. Ukkonen,
   Information and Control Vol. 64, 1985, pp. 100-118.  */

#define GDIFF_MAIN

#include "gnudiff_diff.h"

#include <algorithm>       // for max, min
#include <stdlib.h>


static GNULineRef *xvec, *yvec;  /* Vectors being compared. */
static GNULineRef *fdiag;        /* Vector, indexed by diagonal, containing
                   1 + the X coordinate of the point furthest
                   along the given diagonal in the forward
                   search of the edit matrix. */
static GNULineRef *bdiag;        /* Vector, indexed by diagonal, containing
                   the X coordinate of the point furthest
                   along the given diagonal in the backward
                   search of the edit matrix. */
static GNULineRef too_expensive; /* Edit scripts longer than this are too
                   expensive to compute.  */

#define SNAKE_LIMIT 20 /* Snakes bigger than this are considered `big'.  */

struct partition {
    GNULineRef xmid, ymid; /* Midpoints of this partition.  */
    bool lo_minimal;       /* Nonzero if low half will be analyzed minimally.  */
    bool hi_minimal;       /* Likewise for high half.  */
};

/* Find the midpoint of the shortest edit script for a specified
   portion of the two files.

   Scan from the beginnings of the files, and simultaneously from the ends,
   doing a breadth-first search through the space of edit-sequence.
   When the two searches meet, we have found the midpoint of the shortest
   edit sequence.

   If FIND_MINIMAL is nonzero, find the minimal edit script regardless
   of expense.  Otherwise, if the search is too expensive, use
   heuristics to stop the search and report a suboptimal answer.

   Set PART->(xmid,ymid) to the midpoint (XMID,YMID).  The diagonal number
   XMID - YMID equals the number of inserted lines minus the number
   of deleted lines (counting only lines before the midpoint).
   Return the approximate edit cost; this is the total number of
   lines inserted or deleted (counting only lines before the midpoint),
   unless a heuristic is used to terminate the search prematurely.

   Set PART->lo_minimal to true iff the minimal edit script for the
   left half of the partition is known; similarly for PART->hi_minimal.

   This function assumes that the first lines of the specified portions
   of the two files do not match, and likewise that the last lines do not
   match.  The caller must trim matching lines from the beginning and end
   of the portions it is going to specify.

   If we return the "wrong" partitions,
   the worst this can do is cause suboptimal diff output.
   It cannot cause incorrect diff output.  */

GNULineRef GnuDiff::diag(GNULineRef xoff, GNULineRef xlim, GNULineRef yoff, GNULineRef ylim, bool find_minimal,
                         partition *part) const
{
    GNULineRef *const fd = fdiag;        /* Give the compiler a chance. */
    GNULineRef *const bd = bdiag;        /* Additional help for the compiler. */
    GNULineRef const *const xv = xvec;   /* Still more help for the compiler. */
    GNULineRef const *const yv = yvec;   /* And more and more . . . */
    GNULineRef const dmin = xoff - ylim; /* Minimum valid diagonal. */
    GNULineRef const dmax = xlim - yoff; /* Maximum valid diagonal. */
    GNULineRef const fmid = xoff - yoff; /* Center diagonal of top-down search. */
    GNULineRef const bmid = xlim - ylim; /* Center diagonal of bottom-up search. */
    GNULineRef fmin = fmid, fmax = fmid; /* Limits of top-down search. */
    GNULineRef bmin = bmid, bmax = bmid; /* Limits of bottom-up search. */
    GNULineRef c;                        /* Cost. */
    bool odd = (fmid - bmid) & 1;        /* True if southeast corner is on an odd
                   diagonal with respect to the northwest. */

    fd[fmid] = xoff;
    bd[bmid] = xlim;

    for(c = 1;; ++c)
    {
        GNULineRef d; /* Active diagonal. */
        bool big_snake = false;

        /* Extend the top-down search by an edit step in each diagonal. */
        fmin > dmin ? fd[--fmin - 1] = -1 : ++fmin;
        fmax < dmax ? fd[++fmax + 1] = -1 : --fmax;
        for(d = fmax; d >= fmin; d -= 2)
        {
            GNULineRef x, y, oldx, tlo = fd[d - 1], thi = fd[d + 1];

            if(tlo >= thi)
                x = tlo + 1;
            else
                x = thi;
            oldx = x;
            y = x - d;
            while(x < xlim && y < ylim && xv[x] == yv[y])
                ++x, ++y;
            if(x - oldx > SNAKE_LIMIT)
                big_snake = true;
            fd[d] = x;
            if(odd && bmin <= d && d <= bmax && bd[d] <= x)
            {
                part->xmid = x;
                part->ymid = y;
                part->lo_minimal = part->hi_minimal = true;
                return 2 * c - 1;
            }
        }

        /* Similarly extend the bottom-up search.  */
        bmin > dmin ? bd[--bmin - 1] = GNULINEREF_MAX : ++bmin;
        bmax < dmax ? bd[++bmax + 1] = GNULINEREF_MAX : --bmax;
        for(d = bmax; d >= bmin; d -= 2)
        {
            GNULineRef x, y, oldx, tlo = bd[d - 1], thi = bd[d + 1];

            if(tlo < thi)
                x = tlo;
            else
                x = thi - 1;
            oldx = x;
            y = x - d;
            while(x > xoff && y > yoff && xv[x - 1] == yv[y - 1])
                --x, --y;
            if(oldx - x > SNAKE_LIMIT)
                big_snake = true;
            bd[d] = x;
            if(!odd && fmin <= d && d <= fmax && x <= fd[d])
            {
                part->xmid = x;
                part->ymid = y;
                part->lo_minimal = part->hi_minimal = true;
                return 2 * c;
            }
        }

        if(find_minimal)
            continue;

        /* Heuristic: check occasionally for a diagonal that has made
     lots of progress compared with the edit distance.
     If we have any such, find the one that has made the most
     progress and return it as if it had succeeded.

     With this heuristic, for files with a constant small density
     of changes, the algorithm is linear in the file size.  */

        if(200 < c && big_snake && speed_large_files)
        {
            GNULineRef best;

            best = 0;
            for(d = fmax; d >= fmin; d -= 2)
            {
                GNULineRef dd = d - fmid;
                GNULineRef x = fd[d];
                GNULineRef y = x - d;
                GNULineRef v = (x - xoff) * 2 - dd;
                if(v > 12 * (c + (dd < 0 ? -dd : dd)))
                {
                    if(v > best && xoff + SNAKE_LIMIT <= x && x < xlim && yoff + SNAKE_LIMIT <= y && y < ylim)
                    {
                        /* We have a good enough best diagonal;
             now insist that it end with a significant snake.  */
                        qint32 k;

                        for(k = 1; xv[x - k] == yv[y - k]; k++)
                            if(k == SNAKE_LIMIT)
                            {
                                best = v;
                                part->xmid = x;
                                part->ymid = y;
                                break;
                            }
                    }
                }
            }
            if(best > 0)
            {
                part->lo_minimal = true;
                part->hi_minimal = false;
                return 2 * c - 1;
            }

            best = 0;
            for(d = bmax; d >= bmin; d -= 2)
            {
                GNULineRef dd = d - bmid;
                GNULineRef x = bd[d];
                GNULineRef y = x - d;
                GNULineRef v = (xlim - x) * 2 + dd;
                if(v > 12 * (c + (dd < 0 ? -dd : dd)))
                {
                    if(v > best && xoff < x && x <= xlim - SNAKE_LIMIT && yoff < y && y <= ylim - SNAKE_LIMIT)
                    {
                        /* We have a good enough best diagonal;
             now insist that it end with a significant snake.  */
                        qint32 k;

                        for(k = 0; xv[x + k] == yv[y + k]; k++)
                            if(k == SNAKE_LIMIT - 1)
                            {
                                best = v;
                                part->xmid = x;
                                part->ymid = y;
                                break;
                            }
                    }
                }
            }
            if(best > 0)
            {
                part->lo_minimal = false;
                part->hi_minimal = true;
                return 2 * c - 1;
            }
        }

        /* Heuristic: if we've gone well beyond the call of duty,
     give up and report halfway between our best results so far.  */
        if(c >= too_expensive)
        {
            GNULineRef fxybest, fxbest;
            GNULineRef bxybest, bxbest;

            fxbest = bxbest = 0; /* Pacify `gcc -Wall'.  */

            /* Find forward diagonal that maximizes X + Y.  */
            fxybest = -1;
            for(d = fmax; d >= fmin; d -= 2)
            {
                GNULineRef x = std::min(fd[d], xlim);
                GNULineRef y = x - d;
                if(ylim < y)
                    x = ylim + d, y = ylim;
                if(fxybest < x + y)
                {
                    fxybest = x + y;
                    fxbest = x;
                }
            }

            /* Find backward diagonal that minimizes X + Y.  */
            bxybest = GNULINEREF_MAX;
            for(d = bmax; d >= bmin; d -= 2)
            {
                GNULineRef x = std::max(xoff, bd[d]);
                GNULineRef y = x - d;
                if(y < yoff)
                    x = yoff + d, y = yoff;
                if(x + y < bxybest)
                {
                    bxybest = x + y;
                    bxbest = x;
                }
            }

            /* Use the better of the two diagonals.  */
            if((xlim + ylim) - bxybest < fxybest - (xoff + yoff))
            {
                part->xmid = fxbest;
                part->ymid = fxybest - fxbest;
                part->lo_minimal = true;
                part->hi_minimal = false;
            }
            else
            {
                part->xmid = bxbest;
                part->ymid = bxybest - bxbest;
                part->lo_minimal = false;
                part->hi_minimal = true;
            }
            return 2 * c - 1;
        }
    }
}

/* Compare in detail contiguous subsequences of the two files
   which are known, as a whole, to match each other.

   The results are recorded in the vectors files[N].changed, by
   storing 1 in the element for each line that is an insertion or deletion.

   The subsequence of file 0 is [XOFF, XLIM) and likewise for file 1.

   Note that XLIM, YLIM are exclusive bounds.
   All line numbers are origin-0 and discarded lines are not counted.

   If FIND_MINIMAL, find a minimal difference no matter how
   expensive it is.  */

void GnuDiff::compareseq(GNULineRef xoff, GNULineRef xlim, GNULineRef yoff, GNULineRef ylim, bool find_minimal)
{
    GNULineRef *const xv = xvec; /* Help the compiler.  */
    GNULineRef *const yv = yvec;

    /* Slide down the bottom initial diagonal. */
    while(xoff < xlim && yoff < ylim && xv[xoff] == yv[yoff])
        ++xoff, ++yoff;
    /* Slide up the top initial diagonal. */
    while(xlim > xoff && ylim > yoff && xv[xlim - 1] == yv[ylim - 1])
        --xlim, --ylim;

    /* Handle simple cases. */
    if(xoff == xlim)
        while(yoff < ylim)
            files[1].changed[files[1].realindexes[yoff++]] = true;
    else if(yoff == ylim)
        while(xoff < xlim)
            files[0].changed[files[0].realindexes[xoff++]] = true;
    else
    {
        GNULineRef c;
        partition part;

        /* Find a point of correspondence in the middle of the files.  */

        c = diag(xoff, xlim, yoff, ylim, find_minimal, &part);

        /* This should be impossible, because it implies that
         one of the two subsequences is empty,
         and that case was handled above without calling `diag'. */
        assert(c != 1);

        /* Use the partitions to split this problem into subproblems.  */
        compareseq(xoff, part.xmid, yoff, part.ymid, part.lo_minimal);
        compareseq(part.xmid, xlim, part.ymid, ylim, part.hi_minimal);
    }
}

/* Discard lines from one file that have no matches in the other file.

   A line which is discarded will not be considered by the actual
   comparison algorithm; it will be as if that line were not in the file.
   The file's `realindexes' table maps virtual line numbers
   (which don't count the discarded lines) into real line numbers;
   this is how the actual comparison algorithm produces results
   that are comprehensible when the discarded lines are counted.

   When we discard a line, we also mark it as a deletion or insertion
   so that it will be printed in the output.  */

void GnuDiff::discard_confusing_lines(file_data filevec[])
{
    qint32 f;
    GNULineRef i;
    char *discarded[2];
    GNULineRef *equiv_count[2];
    GNULineRef *p;

    /* Allocate our results.  */
    p = (GNULineRef *)xmalloc((filevec[0].buffered_lines + filevec[1].buffered_lines) * (2 * sizeof(*p)));
    for(f = 0; f < 2; ++f)
    {
        filevec[f].undiscarded = p;
        p += filevec[f].buffered_lines;
        filevec[f].realindexes = p;
        p += filevec[f].buffered_lines;
    }

    /* Set up equiv_count[F][I] as the number of lines in file F
     that fall in equivalence class I.  */

    p = (GNULineRef *)zalloc(filevec[0].equiv_max * (2 * sizeof(*p)));
    equiv_count[0] = p;
    equiv_count[1] = p + filevec[0].equiv_max;

    for(i = 0; i < filevec[0].buffered_lines; ++i)
        ++equiv_count[0][filevec[0].equivs[i]];
    for(i = 0; i < filevec[1].buffered_lines; ++i)
        ++equiv_count[1][filevec[1].equivs[i]];

    /* Set up tables of which lines are going to be discarded.  */

    discarded[0] = (char *)zalloc(filevec[0].buffered_lines + filevec[1].buffered_lines);
    discarded[1] = discarded[0] + filevec[0].buffered_lines;

    /* Mark to be discarded each line that matches no line of the other file.
     If a line matches many lines, mark it as provisionally discardable.  */

    for(f = 0; f < 2; ++f)
    {
        size_t end = filevec[f].buffered_lines;
        char *discards = discarded[f];
        GNULineRef *counts = equiv_count[1 - f];
        GNULineRef *equivs = filevec[f].equivs;
        size_t many = 5;
        size_t tem = end / 64;

        /* Multiply MANY by approximate square root of number of lines.
     That is the threshold for provisionally discardable lines.  */
        while((tem = tem >> 2) > 0)
            many *= 2;

        for(i = 0; i < (GNULineRef)end; ++i)
        {
            GNULineRef nmatch;
            if(equivs[i] == 0)
                continue;
            nmatch = counts[equivs[i]];
            if(nmatch == 0)
                discards[i] = 1;
            else if(nmatch > (GNULineRef)many)
                discards[i] = 2;
        }
    }

    /* Don't really discard the provisional lines except when they occur
     in a run of discardables, with nonprovisionals at the beginning
     and end.  */

    for(f = 0; f < 2; ++f)
    {
        GNULineRef end = filevec[f].buffered_lines;
        char *discards = discarded[f];

        for(i = 0; i < end; ++i)
        {
            /* Cancel provisional discards not in middle of run of discards.  */
            if(discards[i] == 2)
                discards[i] = 0;
            else if(discards[i] != 0)
            {
                /* We have found a nonprovisional discard.  */
                GNULineRef j;
                GNULineRef length;
                GNULineRef provisional = 0;

                /* Find end of this run of discardable lines.
         Count how many are provisionally discardable.  */
                for(j = i; j < end; ++j)
                {
                    if(discards[j] == 0)
                        break;
                    if(discards[j] == 2)
                        ++provisional;
                }

                /* Cancel provisional discards at end, and shrink the run.  */
                while(j > i && discards[j - 1] == 2)
                    discards[--j] = 0, --provisional;

                /* Now we have the length of a run of discardable lines
         whose first and last are not provisional.  */
                length = j - i;

                /* If 1/4 of the lines in the run are provisional,
         cancel discarding of all provisional lines in the run.  */
                if(provisional * 4 > length)
                {
                    while(j > i)
                        if(discards[--j] == 2)
                            discards[j] = 0;
                }
                else
                {
                    GNULineRef consec;
                    GNULineRef minimum = 1;
                    GNULineRef tem = length >> 2;

                    /* MINIMUM is approximate square root of LENGTH/4.
             A subrun of two or more provisionals can stand
             when LENGTH is at least 16.
             A subrun of 4 or more can stand when LENGTH >= 64.  */
                    while(0 < (tem >>= 2))
                        minimum <<= 1;
                    minimum++;

                    /* Cancel any subrun of MINIMUM or more provisionals
             within the larger run.  */
                    for(j = 0, consec = 0; j < length; ++j)
                        if(discards[i + j] != 2)
                            consec = 0;
                        else if(minimum == ++consec)
                            /* Back up to start of subrun, to cancel it all.  */
                            j -= consec;
                        else if(minimum < consec)
                            discards[i + j] = 0;

                    /* Scan from beginning of run
             until we find 3 or more nonprovisionals in a row
             or until the first nonprovisional at least 8 lines in.
             Until that point, cancel any provisionals.  */
                    for(j = 0, consec = 0; j < length; ++j)
                    {
                        if(j >= 8 && discards[i + j] == 1)
                            break;
                        if(discards[i + j] == 2)
                            consec = 0, discards[i + j] = 0;
                        else if(discards[i + j] == 0)
                            consec = 0;
                        else
                            consec++;
                        if(consec == 3)
                            break;
                    }

                    /* I advances to the last line of the run.  */
                    i += length - 1;

                    /* Same thing, from end.  */
                    for(j = 0, consec = 0; j < length; ++j)
                    {
                        if(j >= 8 && discards[i - j] == 1)
                            break;
                        if(discards[i - j] == 2)
                            consec = 0, discards[i - j] = 0;
                        else if(discards[i - j] == 0)
                            consec = 0;
                        else
                            consec++;
                        if(consec == 3)
                            break;
                    }
                }
            }
        }
    }

    /* Actually discard the lines. */
    for(f = 0; f < 2; ++f)
    {
        char *discards = discarded[f];
        GNULineRef end = filevec[f].buffered_lines;
        GNULineRef j = 0;
        for(i = 0; i < end; ++i)
            if(minimal || discards[i] == 0)
            {
                filevec[f].undiscarded[j] = filevec[f].equivs[i];
                filevec[f].realindexes[j++] = i;
            }
            else
                filevec[f].changed[i] = true;
        filevec[f].nondiscarded_lines = j;
    }

    free(discarded[0]);
    free(equiv_count[0]);
}

/* Adjust inserts/deletes of identical lines to join changes
   as much as possible.

   We do something when a run of changed lines include a
   line at one end and have an excluded, identical line at the other.
   We are free to choose which identical line is included.
   `compareseq' usually chooses the one at the beginning,
   but usually it is cleaner to consider the following identical line
   to be the "change".  */

void GnuDiff::shift_boundaries(file_data filevec[])
{
    qint32 f;

    for(f = 0; f < 2; ++f)
    {
        bool *changed = filevec[f].changed;
        bool const *other_changed = filevec[1 - f].changed;
        GNULineRef const *equivs = filevec[f].equivs;
        GNULineRef i = 0;
        GNULineRef j = 0;
        GNULineRef i_end = filevec[f].buffered_lines;

        while(true)
        {
            GNULineRef runlength, start, corresponding;

            /* Scan forwards to find beginning of another run of changes.
         Also keep track of the corresponding point in the other file.  */

            while(i < i_end && !changed[i])
            {
                while(other_changed[j++])
                    continue;
                i++;
            }

            if(i == i_end)
                break;

            start = i;

            /* Find the end of this run of changes.  */

            while(changed[++i])
                continue;
            while(other_changed[j])
                j++;

            do
            {
                /* Record the length of this run of changes, so that
         we can later determine whether the run has grown.  */
                runlength = i - start;

                /* Move the changed region back, so long as the
         previous unchanged line matches the last changed one.
         This merges with previous changed regions.  */

                while(start && equivs[start - 1] == equivs[i - 1])
                {
                    changed[--start] = true;
                    changed[--i] = false;
                    while(changed[start - 1])
                        start--;
                    while(other_changed[--j])
                        continue;
                }

                /* Set CORRESPONDING to the end of the changed run, at the last
         point where it corresponds to a changed run in the other file.
         CORRESPONDING == I_END means no such point has been found.  */
                corresponding = other_changed[j - 1] ? i : i_end;

                /* Move the changed region forward, so long as the
         first changed line matches the following unchanged one.
         This merges with following changed regions.
         Do this second, so that if there are no merges,
         the changed region is moved forward as far as possible.  */

                while(i != i_end && equivs[start] == equivs[i])
                {
                    changed[start++] = false;
                    changed[i++] = true;
                    while(changed[i])
                        i++;
                    while(other_changed[++j])
                        corresponding = i;
                }
            } while(runlength != i - start);

            /* If possible, move the fully-merged run of changes
         back to a corresponding run in the other file.  */

            while(corresponding < i)
            {
                changed[--start] = true;
                changed[--i] = false;
                while(other_changed[--j])
                    continue;
            }
        }
    }
}

/* Cons an additional entry onto the front of an edit script OLD.
   LINE0 and LINE1 are the first affected lines in the two files (origin 0).
   DELETED is the number of lines deleted here from file 0.
   INSERTED is the number of lines inserted here in file 1.

   If DELETED is 0 then LINE0 is the number of the line before
   which the insertion was done; vice versa for INSERTED and LINE1.  */

GnuDiff::change *GnuDiff::add_change(GNULineRef line0, GNULineRef line1, GNULineRef deleted, GNULineRef inserted, change *old)
{
    change *newChange = (change *)xmalloc(sizeof(*newChange));

    newChange->line0 = line0;
    newChange->line1 = line1;
    newChange->inserted = inserted;
    newChange->deleted = deleted;
    newChange->link = old;
    return newChange;
}

/* Scan the tables of which lines are inserted and deleted,
   producing an edit script in reverse order.  */

GnuDiff::change *GnuDiff::build_reverse_script(file_data const filevec[])
{
    change *script = nullptr;
    bool *changed0 = filevec[0].changed;
    bool *changed1 = filevec[1].changed;
    GNULineRef len0 = filevec[0].buffered_lines;
    GNULineRef len1 = filevec[1].buffered_lines;

    /* Note that changedN[len0] does exist, and is 0.  */

    GNULineRef i0 = 0, i1 = 0;

    while(i0 < len0 || i1 < len1)
    {
        if(changed0[i0] | changed1[i1])
        {
            GNULineRef line0 = i0, line1 = i1;

            /* Find # lines changed here in each file.  */
            while(changed0[i0]) ++i0;
            while(changed1[i1]) ++i1;

            /* Record this change.  */
            script = add_change(line0, line1, i0 - line0, i1 - line1, script);
        }

        /* We have reached lines in the two files that match each other.  */
        i0++, i1++;
    }

    return script;
}

/* Scan the tables of which lines are inserted and deleted,
   producing an edit script in forward order.  */

GnuDiff::change *GnuDiff::build_script(file_data const filevec[])
{
    change *script = nullptr;
    bool *changed0 = filevec[0].changed;
    bool *changed1 = filevec[1].changed;
    GNULineRef i0 = filevec[0].buffered_lines, i1 = filevec[1].buffered_lines;

    /* Note that changedN[-1] does exist, and is 0.  */

    while(i0 >= 0 || i1 >= 0)
    {
        if(changed0[i0 - 1] | changed1[i1 - 1])
        {
            GNULineRef line0 = i0, line1 = i1;

            /* Find # lines changed here in each file.  */
            while(changed0[i0 - 1]) --i0;
            while(changed1[i1 - 1]) --i1;

            /* Record this change.  */
            script = add_change(i0, i1, line0 - i0, line1 - i1, script);
        }

        /* We have reached lines in the two files that match each other.  */
        i0--, i1--;
    }

    return script;
}

/* Report the differences of two files.  */
GnuDiff::change *GnuDiff::diff_2_files(comparison *cmp)
{
    GNULineRef diags;
    qint32 f;
    change *script;

    read_files(cmp->file, files_can_be_treated_as_binary);

    {
        /* Allocate vectors for the results of comparison:
     a flag for each line of each file, saying whether that line
     is an insertion or deletion.
     Allocate an extra element, always 0, at each end of each vector.  */

        size_t s = cmp->file[0].buffered_lines + cmp->file[1].buffered_lines + 4;
        bool *flag_space = (bool *)zalloc(s * sizeof(*flag_space));
        cmp->file[0].changed = flag_space + 1;
        cmp->file[1].changed = flag_space + cmp->file[0].buffered_lines + 3;

        /* Some lines are obviously insertions or deletions
     because they don't match anything.  Detect them now, and
     avoid even thinking about them in the main comparison algorithm.  */

        discard_confusing_lines(cmp->file);

        /* Now do the main comparison algorithm, considering just the
     undiscarded lines.  */

        xvec = cmp->file[0].undiscarded;
        yvec = cmp->file[1].undiscarded;
        diags = (cmp->file[0].nondiscarded_lines + cmp->file[1].nondiscarded_lines + 3);
        fdiag = (GNULineRef *)xmalloc(diags * (2 * sizeof(*fdiag)));
        bdiag = fdiag + diags;
        fdiag += cmp->file[1].nondiscarded_lines + 1;
        bdiag += cmp->file[1].nondiscarded_lines + 1;

        /* Set TOO_EXPENSIVE to be approximate square root of input size,
     bounded below by 256.  */
        too_expensive = 1;
        for(; diags != 0; diags >>= 2)
            too_expensive <<= 1;
        too_expensive = std::max((GNULineRef)256, too_expensive);

        files[0] = cmp->file[0];
        files[1] = cmp->file[1];

        compareseq(0, cmp->file[0].nondiscarded_lines,
                   0, cmp->file[1].nondiscarded_lines, minimal);

        free(fdiag - (cmp->file[1].nondiscarded_lines + 1));

        /* Modify the results slightly to make them prettier
     in cases where that can validly be done.  */

        shift_boundaries(cmp->file);

        /* Get the results of comparison in the form of a chain
         of `change's -- an edit script.  */

        script = build_script(cmp->file);

        free(cmp->file[0].undiscarded);

        free(flag_space);

        for(f = 0; f < 2; ++f)
        {
            free(cmp->file[f].equivs);
            free(cmp->file[f].linbuf + cmp->file[f].linbuf_base);
        }
    }

    return script;
}
