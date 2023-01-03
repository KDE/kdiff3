/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef COMBINERS_H
#define COMBINERS_H

#include <QtGlobal>

#include <boost/signals2.hpp>
#include <type_traits>

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

template<typename T> struct FirstNonEmpty
{
    //This just provides a better error message if an inappropriate parameter is passed.
    static_assert(std::is_class<typename std::remove_reference<T>::type>(), "First parameter must be a class.");
    static_assert(std::is_same<decltype(std::declval<T>().isEmpty()), bool>(),
        "First parameter must implement or inherit isEmpty().");

    typedef T result_type;
    template <typename InputIterator> T operator()(InputIterator first, InputIterator last) const
    {
        // If there are no slots to call, just return empty container
        if(first == last) return T();

        T ret = *first++;
        //return first non-true as if && were used
        while(ret.isEmpty() && first != last)
        {
            ret = *first;
            ++first;
        }

        return ret;
    }
};

//Like 'or' but default to false if there are no connections and stops looking once true is returned.
struct find
{
    typedef bool result_type;
    template <typename InputIterator> bool operator()(InputIterator first, InputIterator last) const
    {
        // If there are no slots to call, just return true
        if(first == last)
            return false;

        bool found = *first++;
        //return true if any slot returns true
        while(first != last && !found)
        {
            found = *first;
            ++first;
        }

        return found;
    }
};
#ifdef BOOST_NO_EXCEPTIONS
//Because boost doesn't define this
inline void boost::throw_exception(std::exception const& e) { Q_UNUSED(e); std::terminate();}
#endif

#endif // !COMBINERS_H
