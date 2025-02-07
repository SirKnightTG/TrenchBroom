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

#ifndef TrenchBroom_Entity
#define TrenchBroom_Entity

#include "TrenchBroom.h"
#include "Hit.h"
#include "Model/AttributableNode.h"
#include "Model/EntityRotationPolicy.h"
#include "Model/Object.h"

#include <vecmath/forward.h>
#include <vecmath/bbox.h>
#include <vecmath/util.h>

namespace TrenchBroom {
    namespace Assets {
        class EntityModelFrame;
    }

    namespace Model {
        class PickResult;

        class Entity : public AttributableNode, public Object, private EntityRotationPolicy {
        public:
            static const Hit::HitType EntityHit;
            static const vm::bbox3 DefaultBounds;
        private:
            mutable vm::bbox3 m_definitionBounds;
            mutable vm::bbox3 m_modelBounds;
            mutable vm::bbox3 m_logicalBounds;
            mutable vm::bbox3 m_physicalBounds;
            mutable bool m_boundsValid;
            mutable vm::vec3 m_cachedOrigin;
            mutable vm::mat4x4 m_cachedRotation;

            const Assets::EntityModelFrame* m_modelFrame;
        public:
            Entity();

            bool brushEntity() const;
            bool pointEntity() const;
            bool hasEntityDefinition() const;
            bool hasBrushEntityDefinition() const;
            bool hasPointEntityDefinition() const;
            bool hasPointEntityModel() const;

            const vm::bbox3& definitionBounds() const;

            const vm::vec3& origin() const;
            const vm::mat4x4& rotation() const;
            const vm::mat4x4 modelTransformation() const;
            FloatType area(vm::axis::type axis) const;
        private:
            void cacheAttributes();
            void setOrigin(const vm::vec3& origin);
            void applyRotation(const vm::mat4x4& transformation);
        public: // entity model
            Assets::ModelSpecification modelSpecification() const;
            const vm::bbox3& modelBounds() const;
            const Assets::EntityModelFrame* modelFrame() const;
            void setModelFrame(const Assets::EntityModelFrame* modelFrame);
        private: // implement Node interface
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

            NodeList nodesRequiredForViewSelection() override;
        private: // implement AttributableNode interface
            void doAttributesDidChange(const vm::bbox3& oldBounds) override;
            bool doIsAttributeNameMutable(const AttributeName& name) const override;
            bool doIsAttributeValueMutable(const AttributeName& name) const override;
            vm::vec3 doGetLinkSourceAnchor() const override;
            vm::vec3 doGetLinkTargetAnchor() const override;
        private: // implement Object interface
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
            deleteCopyAndMove(Entity)
        };
    }
}

#endif /* defined(TrenchBroom_Entity) */
