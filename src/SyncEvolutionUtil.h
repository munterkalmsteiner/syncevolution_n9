/*
 * Copyright (C) 2008 Patrick Ohly
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef INCL_SYNCEVOLUTION_UTIL
# define INCL_SYNCEVOLUTION_UTIL

#include <base/test.h>

#include <vector>
#include <sstream>
#include <string>
using namespace std;

template<class T> string join(const string &sep, T begin, T end)
{
    stringstream res;

    if (begin != end) {
        res << *begin;
        ++begin;
        while (begin != end) {
            res << sep;
            res << *begin;
            ++begin;
        }
    }

    return res.str();
}

/**
 * remove multiple slashes in a row and dots directly after a slash if not followed by filename,
 * remove trailing /
 */
string normalizePath(const string &path);

/** ensure that m_path is writable, otherwise throw error */
void mkdir_p(const string &path);

/** remove a complete directory hierarchy; invoking on non-existant directory is okay */
void rm_r(const string &path);

/** true if the path refers to a directory */
bool isDir(const string &path);

/**
 * A C++ wrapper around readir() which provides the names of all
 * directory entries, excluding . and ..
 *
 */
class ReadDir {
 public:
    ReadDir(const string &path);

    typedef vector<string>::const_iterator const_iterator;
    typedef vector<string>::iterator iterator;
    iterator begin() { return m_entries.begin(); }
    iterator end() { return m_entries.end(); }
    const_iterator begin() const { return m_entries.begin(); }
    const_iterator end() const { return m_entries.end(); }

 private:
    string m_path;
    vector<string> m_entries;
};

/**
 * Using this macro ensures that tests, even if defined in
 * object files which are not normally linked into the test
 * binary, are included in the test suite under the group
 * "SyncEvolution".
 */
#define SYNCEVOLUTION_TEST_SUITE_REGISTRATION( ATestFixtureType ) \
    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( ATestFixtureType, "SyncEvolution" ); \
    extern "C" { int funambolAutoRegisterRegistry ## ATestFixtureType = 12345; }


#endif // INCL_SYNCEVOLUTION_UTIL
