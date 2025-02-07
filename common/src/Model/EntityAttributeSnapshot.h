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

#ifndef TrenchBroom_EntityAttributeSnapshot
#define TrenchBroom_EntityAttributeSnapshot

#include "Model/ModelTypes.h"

#include <map>
#include <list>

namespace TrenchBroom {
    namespace Model {
        class EntityAttributeSnapshot {
        public:
            using List = std::list<EntityAttributeSnapshot>;
            using Map = std::map<AttributableNode*, List>;
        private:
            AttributeName m_name;
            AttributeValue m_value;
            bool m_present;
        public:
            EntityAttributeSnapshot(const AttributeName& name, const AttributeValue& value);
            EntityAttributeSnapshot(const AttributeName& name);

            void restore(AttributableNode* node) const;
        };
    }
}

#endif /* defined(TrenchBroom_EntityAttributeSnapshot) */
