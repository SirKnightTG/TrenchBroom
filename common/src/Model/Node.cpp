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

#include "Node.h"

#include "CollectionUtils.h"
#include "Macros.h"
#include "Model/Issue.h"
#include "Model/IssueGenerator.h"

#include <algorithm>
#include <cassert>

namespace TrenchBroom {
    namespace Model {
        Node::Node() :
        m_parent(nullptr),
        m_descendantCount(0),
        m_selected(false),
        m_childSelectionCount(0),
        m_descendantSelectionCount(0),
        m_visibilityState(Visibility_Inherited),
        m_lockState(Lock_Inherited),
        m_lineNumber(0),
        m_lineCount(0),
        m_issuesValid(false),
        m_hiddenIssues(0) {}

        Node::~Node() {
            clearChildren();
            clearIssues();
        }

        const String& Node::name() const {
            return doGetName();
        }

        const vm::bbox3& Node::logicalBounds() const {
            return doGetLogicalBounds();
        }

        const vm::bbox3& Node::physicalBounds() const {
            return doGetPhysicalBounds();
        }

        Node* Node::clone(const vm::bbox3& worldBounds) const {
            return doClone(worldBounds);
        }

        Node* Node::cloneRecursively(const vm::bbox3& worldBounds) const {
            return doCloneRecursively(worldBounds);
        }

        NodeSnapshot* Node::takeSnapshot() {
            return doTakeSnapshot();
        }

        void Node::cloneAttributes(Node* node) const {
            node->setVisibilityState(m_visibilityState);
            node->setLockState(m_lockState);
        }

        NodeList Node::clone(const vm::bbox3& worldBounds, const NodeList& nodes) {
            NodeList clones;
            clones.reserve(nodes.size());
            clone(worldBounds, std::begin(nodes), std::end(nodes), std::back_inserter(clones));
            return clones;
        }

        NodeList Node::cloneRecursively(const vm::bbox3& worldBounds, const NodeList& nodes) {
            NodeList clones;
            clones.reserve(nodes.size());
            cloneRecursively(worldBounds, std::begin(nodes), std::end(nodes), std::back_inserter(clones));
            return clones;
        }

        size_t Node::depth() const {
            if (m_parent == nullptr)
                return 0;
            return m_parent->depth() + 1;
        }

        Node* Node::parent() const {
            return m_parent;
        }

        bool Node::isAncestorOf(const Node* node) const {
            return node->isDescendantOf(this);
        }

        bool Node::isAncestorOf(const NodeList& nodes) const {
            return std::any_of(std::begin(nodes), std::end(nodes), [this](const Node* node) { return isAncestorOf(node); });
        }

        bool Node::isDescendantOf(const Node* node) const {
            Node* parent = m_parent;
            while (parent != nullptr) {
                if (parent == node)
                    return true;
                parent = parent->parent();
            }
            return false;
        }

        bool Node::isDescendantOf(const NodeList& nodes) const {
            return any_of(std::begin(nodes), std::end(nodes), [this](const Node* node) { return isDescendantOf(node); });
        }

        NodeList Node::findDescendants(const NodeList& nodes) const {
            NodeList result;
            std::copy_if(std::begin(nodes), std::end(nodes), std::back_inserter(result), [this](const Node* node) { return node->isDescendantOf(this); });
            return result;
        }

        bool Node::removeIfEmpty() const {
            return doRemoveIfEmpty();
        }

        bool Node::hasChildren() const {
            return !m_children.empty();
        }

        size_t Node::childCount() const {
            return m_children.size();
        }

        const NodeList& Node::children() const {
            return m_children;
        }

        size_t Node::descendantCount() const {
            return m_descendantCount;
        }

        size_t Node::familySize() const {
            return m_descendantCount + 1;
        }

        bool Node::shouldAddToSpacialIndex() const {
            return doShouldAddToSpacialIndex();
        }

        void Node::addChildren(const NodeList& children) {
            addChildren(std::begin(children), std::end(children), children.size());
        }

        void Node::addChild(Node* child) {
            doAddChild(child);
            incDescendantCount(child->descendantCount() + 1);
            incChildSelectionCount(child->selected() ? 1 : 0);
            incDescendantSelectionCount(child->descendantSelectionCount());
        }

        void Node::removeChild(Node* child) {
            doRemoveChild(child);
            decDescendantCount(child->descendantCount() + 1);
            decChildSelectionCount(child->selected() ? 1 : 0);
            decDescendantSelectionCount(child->descendantSelectionCount());
        }

        bool Node::canAddChild(const Node* child) const {
            if (child == this || isDescendantOf(child))
                return false;
            return doCanAddChild(child);
        }

        bool Node::canRemoveChild(const Node* child) const {
            return doCanRemoveChild(child);
        }

        void Node::doAddChild(Node* child) {
            ensure(child != nullptr, "child is null");
            assert(!VectorUtils::contains(m_children, child));
            assert(child->parent() == nullptr);
            assert(canAddChild(child));

            childWillBeAdded(child);
            // nodeWillChange();
            m_children.push_back(child);
            child->setParent(this);
            childWasAdded(child);
            // nodeDidChange();
        }

        void Node::doRemoveChild(Node* child) {
            ensure(child != nullptr, "child is null");
            assert(child->parent() == this);
            assert(canRemoveChild(child));

            childWillBeRemoved(child);
            // nodeWillChange();
            child->setParent(nullptr);
            VectorUtils::erase(m_children, child);
            childWasRemoved(child);
            // nodeDidChange();
        }

        void Node::clearChildren() {
            VectorUtils::clearAndDelete(m_children);
        }

        void Node::childWillBeAdded(Node* node) {
            doChildWillBeAdded(node);
            descendantWillBeAdded(this, node, 1);
        }

        void Node::childWasAdded(Node* node) {
            doChildWasAdded(node);
            descendantWasAdded(node, 1);
        }

        void Node::childWillBeRemoved(Node* node) {
            doChildWillBeRemoved(node);
            descendantWillBeRemoved(node, 1);
        }

        void Node::childWasRemoved(Node* node) {
            doChildWasRemoved(node);
            descendantWasRemoved(this, node, 1);
        }

        void Node::descendantWillBeAdded(Node* newParent, Node* node, const size_t depth) {
            doDescendantWillBeAdded(newParent, node, depth);
            if (shouldPropagateDescendantEvents() && m_parent != nullptr)
                m_parent->descendantWillBeAdded(newParent, node, depth + 1);
        }

        void Node::descendantWasAdded(Node* node, const size_t depth) {
            doDescendantWasAdded(node, depth);
            if (shouldPropagateDescendantEvents() && m_parent != nullptr)
                m_parent->descendantWasAdded(node, depth + 1);
            invalidateIssues();
        }

        void Node::descendantWillBeRemoved(Node* node, const size_t depth) {
            doDescendantWillBeRemoved(node, depth);
            if (shouldPropagateDescendantEvents() && m_parent != nullptr)
                m_parent->descendantWillBeRemoved(node, depth + 1);
        }

        void Node::descendantWasRemoved(Node* oldParent, Node* node, const size_t depth) {
            doDescendantWasRemoved(oldParent, node, depth);
            if (shouldPropagateDescendantEvents() && m_parent != nullptr)
                m_parent->descendantWasRemoved(oldParent, node, depth + 1);
            invalidateIssues();
        }

        bool Node::shouldPropagateDescendantEvents() const {
            return doShouldPropagateDescendantEvents();
        }

        void Node::incDescendantCount(const size_t delta) {
            if (delta == 0)
                return;
            m_descendantCount += delta;
            if (m_parent != nullptr)
                m_parent->incDescendantCount(delta);
        }

        void Node::decDescendantCount(const size_t delta) {
            if (delta == 0)
                return;
            assert(m_descendantCount >= delta);
            m_descendantCount -= delta;
            if (m_parent != nullptr)
                m_parent->decDescendantCount(delta);
        }

        void Node::setParent(Node* parent) {
            assert((m_parent == nullptr) ^ (parent == nullptr));
            assert(parent != this);
            if (parent == m_parent)
                return;

            parentWillChange();
            m_parent = parent;
            parentDidChange();
        }

        void Node::parentWillChange() {
            doParentWillChange();
            ancestorWillChange();
        }

        void Node::parentDidChange() {
            doParentDidChange();
            ancestorDidChange();
        }

        void Node::ancestorWillChange() {
            doAncestorWillChange();
            std::for_each(std::begin(m_children), std::end(m_children), [](Node* child) { child->ancestorWillChange(); });
            invalidateIssues();
        }

        void Node::ancestorDidChange() {
            doAncestorDidChange();
            std::for_each(std::begin(m_children), std::end(m_children), [](Node* child) { child->ancestorDidChange(); });
            invalidateIssues();
        }

        void Node::nodeWillChange() {
            if (m_parent != nullptr)
                m_parent->childWillChange(this);
            invalidateIssues();
        }

        void Node::nodeDidChange() {
            if (m_parent != nullptr)
                m_parent->childDidChange(this);
            invalidateIssues();
        }

        Node::NotifyNodeChange::NotifyNodeChange(Node* node) :
        m_node(node) {
            ensure(m_node != nullptr, "node is null");
            m_node->nodeWillChange();
        }

        Node::NotifyNodeChange::~NotifyNodeChange() {
            m_node->nodeDidChange();
        }

        // notice that we take a copy here so that we can safely propagate the old bounds up
        void Node::nodePhysicalBoundsDidChange(const vm::bbox3 oldBounds) {
            doNodePhysicalBoundsDidChange(oldBounds);
            if (m_parent != nullptr)
                m_parent->childPhysicalBoundsDidChange(this, oldBounds);
        }

        void Node::childWillChange(Node* node) {
            doChildWillChange(node);
            descendantWillChange(node);
        }

        void Node::childDidChange(Node* node) {
            doChildDidChange(node);
            descendantDidChange(node);
        }

        void Node::descendantWillChange(Node* node) {
            doDescendantWillChange(node);
            if (shouldPropagateDescendantEvents() && m_parent != nullptr) {
                m_parent->descendantWillChange(node);
            }
            invalidateIssues();
        }

        void Node::descendantDidChange(Node* node) {
            doDescendantDidChange(node);
            if (shouldPropagateDescendantEvents() && m_parent != nullptr) {
                m_parent->descendantDidChange(node);
            }
            invalidateIssues();
        }

        void Node::childPhysicalBoundsDidChange(Node* node, const vm::bbox3& oldBounds) {
            const vm::bbox3 myOldBounds = physicalBounds();
            if (!myOldBounds.encloses(oldBounds) && !myOldBounds.encloses(node->physicalBounds())) {
                // Our bounds will change only if the child's bounds potentially contributed to our own bounds.
                nodePhysicalBoundsDidChange(myOldBounds);
            }

            doChildPhysicalBoundsDidChange(node, oldBounds);
            descendantPhysicalBoundsDidChange(node, oldBounds, 1);
        }

        void Node::descendantPhysicalBoundsDidChange(Node* node, const vm::bbox3& oldBounds, const size_t depth) {
            doDescendantPhysicalBoundsDidChange(node, oldBounds, depth);
            if (shouldPropagateDescendantEvents() && m_parent != nullptr) {
                m_parent->descendantPhysicalBoundsDidChange(node, oldBounds, depth + 1);
            }
        }

        bool Node::selected() const {
            return m_selected;
        }

        void Node::select() {
            if (!selectable())
                return;
            assert(!m_selected);
            m_selected = true;
            if (m_parent != nullptr)
                m_parent->childWasSelected();
        }

        void Node::deselect() {
            if (!selectable())
                return;
            assert(m_selected);
            m_selected = false;
            if (m_parent != nullptr)
                m_parent->childWasDeselected();
        }

        bool Node::transitivelySelected() const {
            return selected() || parentSelected();
        }

        bool Node::parentSelected() const {
            if (m_parent == nullptr)
                return false;
            if (m_parent->selected())
                return true;
            return m_parent->parentSelected();
        }

        bool Node::childSelected() const {
            return m_childSelectionCount > 0;
        }

        size_t Node::childSelectionCount() const {
            return m_childSelectionCount;
        }


        bool Node::descendantSelected() const {
            return m_descendantSelectionCount > 0;
        }

        size_t Node::descendantSelectionCount() const {
            return m_descendantSelectionCount;
        }

        void Node::childWasSelected() {
            incChildSelectionCount(1);
        }

        void Node::childWasDeselected() {
            decChildSelectionCount(1);
        }

        NodeList Node::nodesRequiredForViewSelection() {
            return NodeList{this};
        }

        void Node::incChildSelectionCount(const size_t delta) {
            if (delta == 0)
                return;
            m_childSelectionCount += delta;
            incDescendantSelectionCount(delta);
        }

        void Node::decChildSelectionCount(const size_t delta) {
            if (delta == 0)
                return;
            assert(m_childSelectionCount >= delta);
            m_childSelectionCount -= delta;
            decDescendantSelectionCount(delta);
        }

        void Node::incDescendantSelectionCount(const size_t delta) {
            if (delta == 0)
                return;
            m_descendantSelectionCount += delta;
            if (m_parent != nullptr)
                m_parent->incDescendantSelectionCount(delta);
        }

        void Node::decDescendantSelectionCount(const size_t delta) {
            if (delta == 0)
                return;
            assert(m_descendantSelectionCount >= delta);
            m_descendantSelectionCount -= delta;
            if (m_parent != nullptr)
                m_parent->decDescendantSelectionCount(delta);
        }

        bool Node::selectable() const {
            return doSelectable();
        }

        bool Node::visible() const {
            switch (m_visibilityState) {
                case Visibility_Inherited:
                    return m_parent == nullptr || m_parent->visible();
                case Visibility_Hidden:
                    return false;
                case Visibility_Shown:
                    return true;
                switchDefault()
            }
        }

        bool Node::shown() const {
            return m_visibilityState == Visibility_Shown;
        }

        bool Node::hidden() const {
            return m_visibilityState == Visibility_Hidden;
        }

        VisibilityState Node::visibilityState() const {
            return m_visibilityState;
        }

        bool Node::setVisibilityState(const VisibilityState visibility) {
            if (visibility != m_visibilityState) {
                m_visibilityState = visibility;
                return true;
            }
            return false;
        }

        bool Node::ensureVisible() {
            if (!visible())
                return setVisibilityState(Visibility_Shown);
            return false;
        }

        bool Node::editable() const {
            switch (m_lockState) {
                case Lock_Inherited:
                    return m_parent == nullptr || m_parent->editable();
                case Lock_Locked:
                    return false;
                case Lock_Unlocked:
                    return true;
                switchDefault()
            }
        }

        bool Node::locked() const {
            return !editable();
        }

        LockState Node::lockState() const {
            return m_lockState;
        }

        bool Node::setLockState(const LockState lockState) {
            if (lockState != m_lockState) {
                m_lockState = lockState;
                return true;
            }
            return false;

        }

        void Node::pick(const vm::ray3& ray, PickResult& pickResult) const {
            doPick(ray, pickResult);
        }

        void Node::findNodesContaining(const vm::vec3& point, NodeList& result) {
            doFindNodesContaining(point, result);
        }

        size_t Node::lineNumber() const {
            return m_lineNumber;
        }

        void Node::setFilePosition(const size_t lineNumber, const size_t lineCount) {
            m_lineNumber = lineNumber;
            m_lineCount = lineCount;
        }

        bool Node::containsLine(const size_t lineNumber) const {
            return lineNumber >= m_lineNumber && lineNumber < m_lineNumber + m_lineCount;
        }

        const IssueList& Node::issues(const IssueGeneratorList& issueGenerators) {
            validateIssues(issueGenerators);
            return m_issues;
        }

        bool Node::issueHidden(const IssueType type) const {
            return (type & m_hiddenIssues) != 0;
        }

        void Node::setIssueHidden(const IssueType type, const bool hidden) {
            if (hidden) {
                m_hiddenIssues |= type;
            } else {
                m_hiddenIssues &= ~type;
            }
        }

        void Node::validateIssues(const IssueGeneratorList& issueGenerators) {
            if (!m_issuesValid) {
                for (const auto* generator : issueGenerators) {
                    doGenerateIssues(generator, m_issues);
                }
                m_issuesValid = true;
            }
        }

        void Node::invalidateIssues() const {
            clearIssues();
            m_issuesValid = false;
        }

        void Node::clearIssues() const {
            VectorUtils::clearAndDelete(m_issues);
        }

        void Node::findAttributableNodesWithAttribute(const AttributeName& name, const AttributeValue& value, AttributableNodeList& result) const {
            return doFindAttributableNodesWithAttribute(name, value, result);
        }

        void Node::findAttributableNodesWithNumberedAttribute(const AttributeName& prefix, const AttributeValue& value, AttributableNodeList& result) const {
            return doFindAttributableNodesWithNumberedAttribute(prefix, value, result);
        }

        void Node::addToIndex(AttributableNode* attributable, const AttributeName& name, const AttributeValue& value) {
            doAddToIndex(attributable, name, value);
        }

        void Node::removeFromIndex(AttributableNode* attributable, const AttributeName& name, const AttributeValue& value) {
            doRemoveFromIndex(attributable, name, value);
        }

        Node* Node::doCloneRecursively(const vm::bbox3& worldBounds) const {
            Node* clone = Node::clone(worldBounds);
            clone->addChildren(Node::cloneRecursively(worldBounds, children()));
            return clone;
        }

        NodeSnapshot* Node::doTakeSnapshot() {
            return nullptr;
        }

        void Node::doChildWillBeAdded(Node* node) {}
        void Node::doChildWasAdded(Node* node) {}
        void Node::doChildWillBeRemoved(Node* node) {}
        void Node::doChildWasRemoved(Node* node) {}

        void Node::doDescendantWillBeAdded(Node* newParent, Node* node, const size_t depth) {}
        void Node::doDescendantWasAdded(Node* node, const size_t depth) {}
        void Node::doDescendantWillBeRemoved(Node* node, const size_t depth) {}
        void Node::doDescendantWasRemoved(Node* oldParent, Node* node, const size_t depth) {}
        bool Node::doShouldPropagateDescendantEvents() const { return true; }

        void Node::doParentWillChange() {}
        void Node::doParentDidChange() {}
        void Node::doAncestorWillChange() {}
        void Node::doAncestorDidChange() {}

        void Node::doNodePhysicalBoundsDidChange(const vm::bbox3& oldBounds) {}
        void Node::doChildPhysicalBoundsDidChange(Node* node, const vm::bbox3& oldBounds) {}
        void Node::doDescendantPhysicalBoundsDidChange(Node* node, const vm::bbox3& oldBounds, const size_t depth) {}

        void Node::doChildWillChange(Node* node) {}
        void Node::doChildDidChange(Node* node) {}
        void Node::doDescendantWillChange(Node* node) {}
        void Node::doDescendantDidChange(Node* node)  {}

        void Node::doFindAttributableNodesWithAttribute(const AttributeName& name, const AttributeValue& value, AttributableNodeList& result) const {
            if (m_parent != nullptr)
                m_parent->findAttributableNodesWithAttribute(name, value, result);
        }

        void Node::doFindAttributableNodesWithNumberedAttribute(const AttributeName& prefix, const AttributeValue& value, AttributableNodeList& result) const {
            if (m_parent != nullptr)
                m_parent->findAttributableNodesWithNumberedAttribute(prefix, value, result);
        }

        void Node::doAddToIndex(AttributableNode* attributable, const AttributeName& name, const AttributeValue& value) {
            if (m_parent != nullptr)
                m_parent->addToIndex(attributable, name, value);
        }

        void Node::doRemoveFromIndex(AttributableNode* attributable, const AttributeName& name, const AttributeValue& value) {
            if (m_parent != nullptr)
                m_parent->removeFromIndex(attributable, name, value);
        }
    }
}
