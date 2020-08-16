/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMBINERS_H
#define COMBINERS_H

#include <boost/signals2.hpp>

#include <QtGlobal>

struct or
{
    typedef bool result_type;
    template <typename InputIterator> bool operator()(InputIterator first, InputIterator last) const
    {
        // If there are no slots to call, just return true
        if(first == last) return true;

        bool ret = *first++;
        //return true if any slot returns true
        while(first != last)
        {
            if(!ret)
                ret = *first;
            ++first;
        }

        return ret;
    }
};

struct and
{
    typedef bool result_type;
    template <typename InputIterator> bool operator()(InputIterator first, InputIterator last) const
    {
        // If there are no slots to call, just return true
        if(first == last) return true;

        bool ret = *first++;
        //return first non-true as if && were used
        while(ret && first != last)
        {
            ret = *first;
            ++first;
        }

        return ret;
    }
};

#ifdef BOOST_NO_EXCEPTIONS
//Because boost doesn't define this
inline void boost::throw_exception(std::exception const& e) { Q_UNUSED(e); std::terminate();}
#endif

#endif // !COMBINERS_H
