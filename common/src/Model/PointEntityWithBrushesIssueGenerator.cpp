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

#include "PointEntityWithBrushesIssueGenerator.h"

#include "Assets/EntityDefinition.h"
#include "Model/Brush.h"
#include "Model/Entity.h"
#include "Model/Issue.h"
#include "Model/IssueQuickFix.h"
#include "Model/MapFacade.h"
#include "StringUtils.h"
#include "VectorUtilsMinimal.h"

#include <cassert>

namespace TrenchBroom {
    namespace Model {
        class PointEntityWithBrushesIssueGenerator::PointEntityWithBrushesIssue : public Issue {
        public:
            static const IssueType Type;
        public:
            PointEntityWithBrushesIssue(Entity* entity) :
            Issue(entity) {}
        private:
            IssueType doGetType() const override {
                return Type;
            }

            const String doGetDescription() const override {
                const Entity* entity = static_cast<Entity*>(node());
                return entity->classname() + " contains brushes";
            }
        };

        const IssueType PointEntityWithBrushesIssueGenerator::PointEntityWithBrushesIssue::Type = Issue::freeType();

        class PointEntityWithBrushesIssueGenerator::PointEntityWithBrushesIssueQuickFix : public IssueQuickFix {
        public:
            PointEntityWithBrushesIssueQuickFix() :
            IssueQuickFix(PointEntityWithBrushesIssue::Type, "Move brushes to world") {}
        private:
            void doApply(MapFacade* facade, const IssueList& issues) const override {
                NodeList affectedNodes;
                ParentChildrenMap nodesToReparent;

                for (const Issue* issue : issues) {
                    Node* node = issue->node();
                    nodesToReparent[node->parent()] = node->children();

                    affectedNodes.push_back(node);
                    VectorUtils::append(affectedNodes, node->children());
                }

                facade->deselectAll();
                facade->reparentNodes(nodesToReparent);
                facade->select(affectedNodes);
            }
        };

        PointEntityWithBrushesIssueGenerator::PointEntityWithBrushesIssueGenerator() :
        IssueGenerator(PointEntityWithBrushesIssue::Type, "Point entity with brushes") {
            addQuickFix(new PointEntityWithBrushesIssueQuickFix());
        }

        void PointEntityWithBrushesIssueGenerator::doGenerate(Entity* entity, IssueList& issues) const {
            ensure(entity != nullptr, "entity is null");
            const Assets::EntityDefinition* definition = entity->definition();
            if (definition != nullptr && definition->type() == Assets::EntityDefinition::Type_PointEntity && entity->hasChildren())
                issues.push_back(new PointEntityWithBrushesIssue(entity));
        }
    }
}
