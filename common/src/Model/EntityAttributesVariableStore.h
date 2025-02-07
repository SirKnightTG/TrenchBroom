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

#ifndef EntityAttributesVariableStore_h
#define EntityAttributesVariableStore_h

#include "Macros.h"
#include "EL.h"

namespace TrenchBroom {
    namespace Model {
        class EntityAttributes;

        class EntityAttributesVariableStore : public EL::VariableStore {
        private:
            const EntityAttributes& m_attributes;
        public:
            explicit EntityAttributesVariableStore(const EntityAttributes& attributes);
        private:
            VariableStore* doClone() const override;
            size_t doGetSize() const override;
            EL::Value doGetValue(const String& name) const override;
            StringSet doGetNames() const override;
            void doDeclare(const String& name, const EL::Value& value) override;
            void doAssign(const String& name, const EL::Value& value) override;

            deleteCopyAndMove(EntityAttributesVariableStore)
        };
    }
}

#endif /* EntityAttributesVariableStore_h */
