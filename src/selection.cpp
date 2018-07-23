#include "gnudiff_diff.h"
#include "selection.h"

#include <QtGlobal>

int Selection::firstPosInLine(LineRef l)
{
    Q_ASSERT(firstLine != invalidRef);

    LineRef l1 = firstLine;
    LineRef l2 = lastLine;
    int p1 = firstPos;
    int p2 = lastPos;
    if(l1 > l2)
    {
        std::swap(l1, l2);
        std::swap(p1, p2);
    }
    if(l1 == l2 && p1 > p2)
    {
        std::swap(p1, p2);
    }

    if(l == l1)
        return p1;
    return 0;
}

int Selection::lastPosInLine(LineRef l)
{
    Q_ASSERT(firstLine != invalidRef);

    LineRef l1 = firstLine;
    LineRef l2 = lastLine;
    int p1 = firstPos;
    int p2 = lastPos;

    if(l1 > l2)
    {
        std::swap(l1, l2);
        std::swap(p1, p2);
    }
    if(l1 == l2 && p1 > p2)
    {
        std::swap(p1, p2);
    }

    if(l == l2)
        return p2;
    return INT_MAX;
}

bool Selection::within(LineRef l, LineRef p)
{
    if(firstLine == invalidRef)
        return false;
    
    LineRef l1 = firstLine;
    LineRef l2 = lastLine;
    int p1 = firstPos;
    int p2 = lastPos;
    if(l1 > l2)
    {
        std::swap(l1, l2);
        std::swap(p1, p2);
    }
    if(l1 == l2 && p1 > p2)
    {
        std::swap(p1, p2);
    }
    if(l1 <= l && l <= l2)
    {
        if(l1 == l2)
            return p >= p1 && p < p2;
        if(l == l1)
            return p >= p1;
        if(l == l2)
            return p < p2;
        return true;
    }
    return false;
}

bool Selection::lineWithin(LineRef l)
{
    if(firstLine == invalidRef)
        return false;
    LineRef l1 = firstLine;
    LineRef l2 = lastLine;

    if(l1 > l2)
    {
        std::swap(l1, l2);
    }

    return (l1 <= l && l <= l2);
}
