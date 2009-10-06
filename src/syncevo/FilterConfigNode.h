/*
 * Copyright (C) 2008-2009 Patrick Ohly <patrick.ohly@gmx.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef INCL_EVOLUTION_FILTER_CONFIG_NODE
# define INCL_EVOLUTION_FILTER_CONFIG_NODE

#include <syncevo/ConfigNode.h>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string.hpp>

#include <syncevo/util.h>

#include <map>
#include <utility>
#include <vector>
#include <string>

#include <syncevo/declarations.h>
SE_BEGIN_CXX
using namespace std;

/**
 * This class acts as filter between a real config node and its user:
 * reads which match properties which are set in the filter will
 * return the value set in the filter. Writes will go to the underlying
 * node and future reads will return the written value.
 *
 * The purpose of this class is temporarily overriding saved values
 * during one run without having to modify the saved values.
 */
class FilterConfigNode : public ConfigNode {
 public:
    /** a case-insensitive string to string mapping */
    class ConfigFilter : public map<string, string, Nocase<string> > {
    public:
        /** format as <key> = <value> lines */
        operator string () const;
    };

    /** read-write access to underlying node */
    FilterConfigNode(const boost::shared_ptr<ConfigNode> &node,
                     const ConfigFilter &filter = ConfigFilter());

    /** read-only access to underlying node */
    FilterConfigNode(const boost::shared_ptr<const ConfigNode> &node,
                     const ConfigFilter &filter = ConfigFilter());

    virtual string getName() const { return m_readOnlyNode->getName(); }

    /** add another entry to the list of filter properties */
    void addFilter(const string &property,
                   const string &value);

    /** replace current filter list with new one */
    void setFilter(const ConfigFilter &filter);
    const ConfigFilter &getFilter() const { return m_filter; }

    /* ConfigNode API */
    virtual void flush();
    virtual string readProperty(const string &property) const;
    virtual void setProperty(const string &property,
                             const string &value,
                             const string &comment = "",
                             const string *defValue = NULL);
    virtual void readProperties(map<string, string> &props) const;
    virtual void removeProperty(const string &property);
    virtual bool exists() const { return m_readOnlyNode->exists(); }

 private:
    ConfigFilter m_filter;
    boost::shared_ptr<ConfigNode> m_node;
    boost::shared_ptr<const ConfigNode> m_readOnlyNode;
};


SE_END_CXX
#endif