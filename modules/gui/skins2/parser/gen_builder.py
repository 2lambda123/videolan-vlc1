#!/usr/bin/python

# Script to generate builder_data.hpp, with the definitions given in
# builder_data.def
# Each line of the definition file is in the following format:
# ClassName param1:type1 param2:type2 ...

import string

deffile = open("builder_data.def")
hppfile = open("builder_data.hpp","w")

hppfile.write(
"""/*****************************************************************************
 * builder_data.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gen_builder.py,v 1.2 2004/03/02 21:45:15 ipkiss Exp $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teuli�re <ipkiss@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

//File generated by gen_builder.py
//DO NOT EDIT BY HAND !

#ifndef BUILDER_DATA_HPP
#define BUILDER_DATA_HPP

#include <vlc/vlc.h>
#include <list>
#include <map>
#include <string>

using namespace std;

/// Structure for mapping data from XML file
struct BuilderData
{

""")

while 1:
    line = string.strip(deffile.readline())
    if line == "":
        break
    items = string.split(line, ' ')
    name = items[0]
    str = "    /// Type definition\n"
    str += "    struct " + name + "\n    {\n"
    str += "        " + name + "( "
    constructor = ""
    initlist = ""
    vars = ""
    for var in items[1:]:
        vardef = string.split(var, ':');
        varname = vardef[0]
        vartype = vardef[1]
        if vartype == "string":
            vartype = "const string &"
        if constructor != "":
            constructor += ", "
        constructor += vartype + " " + varname
        if initlist != "":
            initlist += ", "
        initlist += "m_" + varname + "( " + varname + " )"
        vartype = vardef[1]
        if vartype == "string":
            vartype = "const string"
        vars += "        " + vartype + " m_" + varname + ";\n"
    str += constructor + " ):\n" + initlist + " {}\n\n"
    str += vars + "    };\n"
    str += "    /// List\n"
    str += "    list<" + name + "> m_list" + name + ";\n"
    str += "\n"
    hppfile.write(str)

hppfile.write(
"""
};

#endif
""")

deffile.close()
hppfile.close()
