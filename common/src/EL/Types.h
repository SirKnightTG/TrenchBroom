/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef Types_h
#define Types_h

#include "StringType.h"

#include <map>
#include <vector>

namespace TrenchBroom {
    namespace EL {
        class Value;

        using BooleanType = bool;
        using StringType = String;
        using NumberType = double;
        using IntegerType = long;
        using ArrayType = std::vector<Value>;
        using MapType = std::map<String, Value>;
        using RangeType = std::vector<long>;

        typedef enum {
            Type_Boolean,
            Type_String,
            Type_Number,
            Type_Array,
            Type_Map,
            Type_Range,
            Type_Null,
            Type_Undefined
        } ValueType;

        String typeName(ValueType type);
        ValueType typeForName(const String& type);
    }
}

#endif /* Types_h */
