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

#ifndef TrenchBroom_Group
#define TrenchBroom_Group

#include "TrenchBroom.h"
#include "StringType.h"
#include "Macros.h"
#include "Model/ModelTypes.h"
#include "Model/Node.h"
#include "Model/Object.h"

#include <vecmath/bbox.h>

namespace TrenchBroom {
    namespace Model {
        class PickResult;

        class Group : public Node, public Object {
        private:
            typedef enum {
                Edit_Open,
                Edit_Closed,
                Edit_DescendantOpen
            } EditState;

            String m_name;
            EditState m_editState;
            mutable vm::bbox3 m_logicalBounds;
            mutable vm::bbox3 m_physicalBounds;
            mutable bool m_boundsValid;
        public:
            Group(const String& name);

            void setName(const String& name);

            bool opened() const;
            bool hasOpenedDescendant() const;
            void open();
            void close();
        private:
            void setEditState(EditState editState);

            class SetEditStateVisitor;
            void openAncestors();
            void closeAncestors();
        private: // implement methods inherited from Node
            const String& doGetName() const override;
            const vm::bbox3& doGetLogicalBounds() const override;
            const vm::bbox3& doGetPhysicalBounds() const override;

            Node* doClone(const vm::bbox3& worldBounds) const override;
            NodeSnapshot* doTakeSnapshot() override;

            bool doCanAddChild(const Node* child) const override;
            bool doCanRemoveChild(const Node* child) const override;
            bool doRemoveIfEmpty() const override;

            bool doShouldAddToSpacialIndex() const override;

            void doChildWasAdded(Node* node) override;
            void doChildWasRemoved(Node* node) override;

            void doNodePhysicalBoundsDidChange(const vm::bbox3& oldBounds) override;
            void doChildPhysicalBoundsDidChange(Node* node, const vm::bbox3& oldBounds) override;

            bool doSelectable() const override;

            void doPick(const vm::ray3& ray, PickResult& pickResult) const override;
            void doFindNodesContaining(const vm::vec3& point, NodeList& result) override;

            void doGenerateIssues(const IssueGenerator* generator, IssueList& issues) override;
            void doAccept(NodeVisitor& visitor) override;
            void doAccept(ConstNodeVisitor& visitor) const override;
        private: // implement methods inherited from Object
            Node* doGetContainer() const override;
            Layer* doGetLayer() const override;
            Group* doGetGroup() const override;

            void doTransform(const vm::mat4x4& transformation, bool lockTextures, const vm::bbox3& worldBounds) override;
            bool doContains(const Node* node) const override;
            bool doIntersects(const Node* node) const override;
        private:
            void invalidateBounds();
            void validateBounds() const;
        private: // implement Taggable interface
            void doAcceptTagVisitor(TagVisitor& visitor) override;
            void doAcceptTagVisitor(ConstTagVisitor& visitor) const override;
        private:
            deleteCopyAndMove(Group)
        };
    }
}

#endif /* defined(TrenchBroom_Group) */
