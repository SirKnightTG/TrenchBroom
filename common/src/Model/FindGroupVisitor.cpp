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

#include "FindGroupVisitor.h"

#include "Model/Node.h"
#include "Model/Group.h"

namespace TrenchBroom {
    namespace Model {
        // FindGroupVisitor

        void FindGroupVisitor::doVisit(World* world) {}
        void FindGroupVisitor::doVisit(Layer* layer) {}

        void FindGroupVisitor::doVisit(Group* group) {
            setResult(group);
            cancel();
        }

        void FindGroupVisitor::doVisit(Entity* entity) {}
        void FindGroupVisitor::doVisit(Brush* brush) {}

        // FindOutermostClosedGroupVisitor

        void FindOutermostClosedGroupVisitor::doVisit(World* world) {}
        void FindOutermostClosedGroupVisitor::doVisit(Layer* layer) {}

        void FindOutermostClosedGroupVisitor::doVisit(Group* group) {
            const bool closed = !(group->opened() || group->hasOpenedDescendant());

            if (closed) {
                setResult(group);
            }
        }

        void FindOutermostClosedGroupVisitor::doVisit(Entity* entity) {}
        void FindOutermostClosedGroupVisitor::doVisit(Brush* brush) {}

        // Helper functions

        Model::Group* findGroup(Model::Node* node) {
            FindGroupVisitor visitor;
            node->escalate(visitor);
            return visitor.hasResult() ? visitor.result() : nullptr;
        }

        Model::Group* findOutermostClosedGroup(Model::Node* node) {
            FindOutermostClosedGroupVisitor visitor;
            node->escalate(visitor);
            return visitor.hasResult() ? visitor.result() : nullptr;
        }
    }
}
