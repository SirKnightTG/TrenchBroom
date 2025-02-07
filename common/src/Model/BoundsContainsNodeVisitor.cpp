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

#include "BoundsContainsNodeVisitor.h"
#include "Model/Brush.h"
#include "Model/Entity.h"
#include "Model/Group.h"

namespace TrenchBroom {
    namespace Model {
        BoundsContainsNodeVisitor::BoundsContainsNodeVisitor(const vm::bbox3& bounds) :
        m_bounds(bounds) {}

        void BoundsContainsNodeVisitor::doVisit(const World* world)   { setResult(false); }
        void BoundsContainsNodeVisitor::doVisit(const Layer* layer)   { setResult(false); }
        void BoundsContainsNodeVisitor::doVisit(const Group* group)   { setResult(m_bounds.contains(group->logicalBounds())); }
        void BoundsContainsNodeVisitor::doVisit(const Entity* entity) { setResult(m_bounds.contains(entity->logicalBounds())); }
        void BoundsContainsNodeVisitor::doVisit(const Brush* brush)   { setResult(m_bounds.contains(brush->logicalBounds())); }
    }
}
