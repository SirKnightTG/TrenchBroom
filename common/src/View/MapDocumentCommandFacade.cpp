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

#include "MapDocumentCommandFacade.h"

#include "CollectionUtils.h"
#include "Preferences.h"
#include "PreferenceManager.h"
#include "Assets/EntityDefinitionFileSpec.h"
#include "Assets/TextureManager.h"
#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/ChangeBrushFaceAttributesRequest.h"
#include "Model/CollectNodesWithDescendantSelectionCountVisitor.h"
#include "Model/CollectRecursivelySelectedNodesVisitor.h"
#include "Model/CollectSelectableBrushFacesVisitor.h"
#include "Model/CollectSelectableNodesVisitor.h"
#include "Model/EditorContext.h"
#include "Model/Entity.h"
#include "Model/Game.h"
#include "Model/Group.h"
#include "Model/Issue.h"
#include "Model/ModelUtils.h"
#include "Model/Snapshot.h"
#include "Model/TransformObjectVisitor.h"
#include "Model/World.h"
#include "Model/NodeVisitor.h"
#include "View/Selection.h"

namespace TrenchBroom {
    namespace View {
        MapDocumentSPtr MapDocumentCommandFacade::newMapDocument() {
            return MapDocumentSPtr(new MapDocumentCommandFacade());
        }

        MapDocumentCommandFacade::MapDocumentCommandFacade() :
        m_commandProcessor(this) {
            bindObservers();
        }

        void MapDocumentCommandFacade::performSelect(const Model::NodeList& nodes) {
            selectionWillChangeNotifier();
            updateLastSelectionBounds();

            Model::NodeList selected;
            selected.reserve(nodes.size());

            Model::CollectNodesWithDescendantSelectionCountVisitor ancestors(0);
            Model::CollectRecursivelySelectedNodesVisitor descendants(false);

            for (Model::Node* initialNode : nodes) {
                ensure(initialNode->isDescendantOf(m_world.get()) || initialNode == m_world.get(), "to select a node, it must be world or a descendant");
                const auto nodesToSelect = initialNode->nodesRequiredForViewSelection();
                for (Model::Node* node : nodesToSelect) {
                    if (!node->selected() /* && m_editorContext->selectable(node) remove check to allow issue objects to be selected */) {
                        node->escalate(ancestors);
                        node->recurse(descendants);
                        node->select();
                        selected.push_back(node);
                    }
                }
            }

            const Model::NodeList& partiallySelected = ancestors.nodes();
            const Model::NodeList& recursivelySelected = descendants.nodes();

            m_selectedNodes.addNodes(selected);
            m_partiallySelectedNodes.addNodes(partiallySelected);

            Selection selection;
            selection.addSelectedNodes(selected);
            selection.addPartiallySelectedNodes(partiallySelected);
            selection.addRecursivelySelectedNodes(recursivelySelected);

            selectionDidChangeNotifier(selection);
            invalidateSelectionBounds();
        }

        void MapDocumentCommandFacade::performSelect(const Model::BrushFaceList& faces) {
            selectionWillChangeNotifier();

            Model::BrushFaceList selected;
            selected.reserve(faces.size());

            Model::CollectNodesWithDescendantSelectionCountVisitor visitor(0);

            for (Model::BrushFace* face : faces) {
                if (!face->selected() && m_editorContext->selectable(face)) {
                    face->brush()->acceptAndEscalate(visitor);
                    face->select();
                    selected.push_back(face);
                }
            }

            const Model::NodeList& partiallySelected = visitor.nodes();

            VectorUtils::append(m_selectedBrushFaces, selected);
            m_partiallySelectedNodes.addNodes(partiallySelected);

            Selection selection;
            selection.addSelectedBrushFaces(selected);
            selection.addPartiallySelectedNodes(partiallySelected);

            selectionDidChangeNotifier(selection);
        }

        void MapDocumentCommandFacade::performSelectAllNodes() {
            performDeselectAll();

            Model::CollectSelectableNodesVisitor visitor(*m_editorContext);

            Model::Node* target = currentGroup();
            if (target == nullptr) {
                target = m_world.get();
            }

            target->recurse(visitor);
            performSelect(visitor.nodes());
        }

        void MapDocumentCommandFacade::performSelectAllBrushFaces() {
            performDeselectAll();

            Model::CollectSelectableBrushFacesVisitor visitor(*m_editorContext);
            m_world->acceptAndRecurse(visitor);
            performSelect(visitor.faces());
        }

        void MapDocumentCommandFacade::performConvertToBrushFaceSelection() {
            Model::CollectSelectableBrushFacesVisitor visitor(*m_editorContext);
            Model::Node::acceptAndRecurse(std::begin(m_selectedNodes), std::end(m_selectedNodes), visitor);

            performDeselectAll();
            performSelect(visitor.faces());
        }

        void MapDocumentCommandFacade::performDeselect(const Model::NodeList& nodes) {
            selectionWillChangeNotifier();
            updateLastSelectionBounds();

            Model::NodeList deselected;
            deselected.reserve(nodes.size());

            Model::CollectNodesWithDescendantSelectionCountVisitor ancestors(0);
            Model::CollectRecursivelySelectedNodesVisitor descendants(false);

            for (Model::Node* node : nodes) {
                if (node->selected()) {
                    node->deselect();
                    deselected.push_back(node);
                    node->escalate(ancestors);
                    node->recurse(descendants);
                }
            }

            const Model::NodeList& partiallyDeselected = ancestors.nodes();
            const Model::NodeList& recursivelyDeselected = descendants.nodes();

            m_selectedNodes.removeNodes(deselected);
            m_partiallySelectedNodes.removeNodes(partiallyDeselected);

            Selection selection;
            selection.addDeselectedNodes(deselected);
            selection.addPartiallyDeselectedNodes(partiallyDeselected);
            selection.addRecursivelyDeselectedNodes(recursivelyDeselected);

            selectionDidChangeNotifier(selection);
            invalidateSelectionBounds();
        }

        void MapDocumentCommandFacade::performDeselect(const Model::BrushFaceList& faces) {
            selectionWillChangeNotifier();

            Model::BrushFaceList deselected;
            deselected.reserve(faces.size());

            Model::CollectNodesWithDescendantSelectionCountVisitor visitor(0);

            for (Model::BrushFace* face : faces) {
                if (face->selected()) {
                    face->deselect();
                    deselected.push_back(face);
                    face->brush()->acceptAndEscalate(visitor);
                }
            }

            const Model::NodeList& partiallyDeselected = visitor.nodes();

            VectorUtils::eraseAll(m_selectedBrushFaces, deselected);
            m_selectedNodes.removeNodes(partiallyDeselected);

            Selection selection;
            selection.addDeselectedBrushFaces(deselected);
            selection.addPartiallyDeselectedNodes(partiallyDeselected);

            selectionDidChangeNotifier(selection);
        }

        void MapDocumentCommandFacade::performDeselectAll() {
            if (hasSelectedNodes())
                deselectAllNodes();
            if (hasSelectedBrushFaces())
                deselectAllBrushFaces();
        }

        void MapDocumentCommandFacade::deselectAllNodes() {
            selectionWillChangeNotifier();
            updateLastSelectionBounds();

            Model::CollectRecursivelySelectedNodesVisitor descendants(false);

            for (Model::Node* node : m_selectedNodes) {
                node->deselect();
                node->recurse(descendants);
            }

            Selection selection;
            selection.addDeselectedNodes(m_selectedNodes.nodes());
            selection.addPartiallyDeselectedNodes(m_partiallySelectedNodes.nodes());
            selection.addRecursivelyDeselectedNodes(descendants.nodes());

            m_selectedNodes.clear();
            m_partiallySelectedNodes.clear();

            selectionDidChangeNotifier(selection);
            invalidateSelectionBounds();
        }

        void MapDocumentCommandFacade::deselectAllBrushFaces() {
            selectionWillChangeNotifier();

            for (Model::BrushFace* face : m_selectedBrushFaces)
                face->deselect();

            Selection selection;
            selection.addDeselectedBrushFaces(m_selectedBrushFaces);
            selection.addPartiallyDeselectedNodes(m_partiallySelectedNodes.nodes());

            m_selectedBrushFaces.clear();
            m_partiallySelectedNodes.clear();

            selectionDidChangeNotifier(selection);
        }

        void MapDocumentCommandFacade::performAddNodes(const Model::ParentChildrenMap& nodes) {
            const Model::NodeList parents = collectParents(nodes);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);

            Model::NodeList addedNodes;
            for (const auto& entry : nodes) {
                Model::Node* parent = entry.first;
                const Model::NodeList& children = entry.second;
                parent->addChildren(children);
                VectorUtils::append(addedNodes, children);
            }

            setEntityDefinitions(addedNodes);
            setEntityModels(addedNodes);
            setTextures(addedNodes);
            invalidateSelectionBounds();

            nodesWereAddedNotifier(addedNodes);
        }

        void MapDocumentCommandFacade::performRemoveNodes(const Model::ParentChildrenMap& nodes) {
            const Model::NodeList parents = collectParents(nodes);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);

            const Model::NodeList allChildren = collectChildren(nodes);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyChildren(nodesWillBeRemovedNotifier, nodesWereRemovedNotifier, allChildren);

            for (const auto& entry : nodes) {
                Model::Node* parent = entry.first;
                const Model::NodeList& children = entry.second;
                unsetEntityModels(children);
                unsetEntityDefinitions(children);
                unsetTextures(children);
                parent->removeChildren(std::begin(children), std::end(children));
            }

            invalidateSelectionBounds();
        }

        Model::VisibilityMap MapDocumentCommandFacade::setVisibilityState(const Model::NodeList& nodes, const Model::VisibilityState visibilityState) {
            Model::VisibilityMap result;

            Model::NodeList changedNodes;
            changedNodes.reserve(nodes.size());

            for (Model::Node* node : nodes) {
                const Model::VisibilityState oldState = node->visibilityState();
                if (node->setVisibilityState(visibilityState)) {
                    changedNodes.push_back(node);
                    result[node] = oldState;
                }
            }

            nodeVisibilityDidChangeNotifier(changedNodes);
            return result;
        }

        Model::VisibilityMap MapDocumentCommandFacade::setVisibilityEnsured(const Model::NodeList& nodes) {
            Model::VisibilityMap result;

            Model::NodeList changedNodes;
            changedNodes.reserve(nodes.size());

            for (Model::Node* node : nodes) {
                const Model::VisibilityState oldState = node->visibilityState();
                if (node->ensureVisible()) {
                    changedNodes.push_back(node);
                    result[node] = oldState;
                }
            }

            nodeVisibilityDidChangeNotifier(changedNodes);
            return result;
        }

        void MapDocumentCommandFacade::restoreVisibilityState(const Model::VisibilityMap& nodes) {
            Model::NodeList changedNodes;
            changedNodes.reserve(nodes.size());

            for (const auto& entry : nodes) {
                Model::Node* node = entry.first;
                const Model::VisibilityState state = entry.second;
                if (node->setVisibilityState(state))
                    changedNodes.push_back(node);
            }

            nodeVisibilityDidChangeNotifier(changedNodes);
        }

        Model::LockStateMap MapDocumentCommandFacade::setLockState(const Model::NodeList& nodes, const Model::LockState lockState) {
            Model::LockStateMap result;

            Model::NodeList changedNodes;
            changedNodes.reserve(nodes.size());

            for (Model::Node* node : nodes) {
                const Model::LockState oldState = node->lockState();
                if (node->setLockState(lockState)) {
                    changedNodes.push_back(node);
                    result[node] = oldState;
                }
            }

            nodeLockingDidChangeNotifier(changedNodes);
            return result;
        }

        void MapDocumentCommandFacade::restoreLockState(const Model::LockStateMap& nodes) {
            Model::NodeList changedNodes;
            changedNodes.reserve(nodes.size());

            for (const auto& entry : nodes) {
                Model::Node* node = entry.first;
                const Model::LockState state = entry.second;
                if (node->setLockState(state))
                    changedNodes.push_back(node);
            }

            nodeLockingDidChangeNotifier(changedNodes);
        }

        class MapDocumentCommandFacade::RenameGroupsVisitor : public Model::NodeVisitor {
        private:
            const String& m_newName;
            Model::GroupNameMap m_oldNames;
        public:
            RenameGroupsVisitor(const String& newName) : m_newName(newName) {}
            const Model::GroupNameMap& oldNames() const { return m_oldNames; }
        private:
            void doVisit(Model::World* world) override   {}
            void doVisit(Model::Layer* layer) override   {}
            void doVisit(Model::Group* group) override   {
                m_oldNames[group] = group->name();
                group->setName(m_newName);
            }
            void doVisit(Model::Entity* entity) override {}
            void doVisit(Model::Brush* brush) override   {}
        };

        class MapDocumentCommandFacade::UndoRenameGroupsVisitor : public Model::NodeVisitor {
        private:
            const Model::GroupNameMap& m_newNames;
        public:
            UndoRenameGroupsVisitor(const Model::GroupNameMap& newNames) : m_newNames(newNames) {}
        private:
            void doVisit(Model::World* world) override   {}
            void doVisit(Model::Layer* layer) override   {}
            void doVisit(Model::Group* group) override   {
                assert(m_newNames.count(group) == 1);
                const String& newName = MapUtils::find(m_newNames, group, group->name());
                group->setName(newName);
            }
            void doVisit(Model::Entity* entity) override {}
            void doVisit(Model::Brush* brush) override   {}
        };

        Model::GroupNameMap MapDocumentCommandFacade::performRenameGroups(const String& newName) {
            const Model::NodeList& nodes = m_selectedNodes.nodes();
            const Model::NodeList parents = collectParents(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

            RenameGroupsVisitor visitor(newName);
            Model::Node::accept(std::begin(nodes), std::end(nodes), visitor);
            return visitor.oldNames();
        }

        void MapDocumentCommandFacade::performUndoRenameGroups(const Model::GroupNameMap& newNames) {
            const Model::NodeList& nodes = m_selectedNodes.nodes();
            const Model::NodeList parents = collectParents(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

            UndoRenameGroupsVisitor visitor(newNames);
            Model::Node::accept(std::begin(nodes), std::end(nodes), visitor);
        }

        void MapDocumentCommandFacade::performPushGroup(Model::Group* group) {
            m_editorContext->pushGroup(group);
            groupWasOpenedNotifier(group);
        }

        void MapDocumentCommandFacade::performPopGroup() {
            Model::Group* previousGroup = m_editorContext->currentGroup();
            m_editorContext->popGroup();
            groupWasClosedNotifier(previousGroup);
        }

        class CanTransformVisitor : public Model::ConstNodeVisitor, public Model::NodeQuery<bool> {
        private:
            vm::mat4x4 m_transform;
            vm::bbox3 m_worldBounds;
        public:
            CanTransformVisitor(const vm::mat4x4& transform, const vm::bbox3& worldBounds) :
                m_transform(transform),
                m_worldBounds(worldBounds) {}
        private:
            void doVisit(const Model::World* world) override { setResult(true); }
            void doVisit(const Model::Layer* layer) override { setResult(true); }
            void doVisit(const Model::Group* group) override { setResult(true); }
            void doVisit(const Model::Entity* entity) override { setResult(true); }
            void doVisit(const Model::Brush* brush) override { setResult(brush->canTransform(m_transform, m_worldBounds)); }
            bool doCombineResults(bool oldResult, bool newResult) const override {
                return newResult && oldResult;
            }
        };

        bool MapDocumentCommandFacade::performTransform(const vm::mat4x4 &transform, const bool lockTextures) {
          // Test whether all brushes can be transformed; abort if any fail.
          CanTransformVisitor canTransform(transform, m_worldBounds);
          for (const auto* node : m_selectedNodes.nodes()) {
              node->acceptAndRecurse(canTransform);
          }
          if (canTransform.hasResult() && !canTransform.result()) {
              return false;
          }

          const Model::NodeList &nodes = m_selectedNodes.nodes();
          const Model::NodeList parents = collectParents(nodes);

          Notifier<const Model::NodeList &>::NotifyBeforeAndAfter
              notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier,
                            parents);
          Notifier<const Model::NodeList &>::NotifyBeforeAndAfter notifyNodes(
              nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

          Model::TransformObjectVisitor visitor(transform, lockTextures,
                                                m_worldBounds);
          Model::Node::accept(std::begin(nodes), std::end(nodes), visitor);

          invalidateSelectionBounds();
          return true;
        }

        Model::EntityAttributeSnapshot::Map MapDocumentCommandFacade::performSetAttribute(const Model::AttributeName& name, const Model::AttributeValue& value) {
            const Model::AttributableNodeList attributableNodes = allSelectedAttributableNodes();
            const Model::NodeList nodes(std::begin(attributableNodes), std::end(attributableNodes));
            const Model::NodeList parents = collectParents(std::begin(nodes), std::end(nodes));
            const Model::NodeList descendants = collectDescendants(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyDescendants(nodesWillChangeNotifier, nodesDidChangeNotifier, descendants);

            Model::EntityAttributeSnapshot::Map snapshot;

            for (Model::AttributableNode* node : attributableNodes) {
                snapshot[node].push_back(node->attributeSnapshot(name));
                node->addOrUpdateAttribute(name, value);
            }

            setEntityDefinitions(nodes);
            setEntityModels(nodes);

            return snapshot;
        }

        Model::EntityAttributeSnapshot::Map MapDocumentCommandFacade::performRemoveAttribute(const Model::AttributeName& name) {
            const Model::AttributableNodeList attributableNodes = allSelectedAttributableNodes();
            const Model::NodeList nodes(std::begin(attributableNodes), std::end(attributableNodes));
            const Model::NodeList parents = collectParents(std::begin(nodes), std::end(nodes));
            const Model::NodeList descendants = collectDescendants(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyDescendants(nodesWillChangeNotifier, nodesDidChangeNotifier, descendants);

            Model::EntityAttributeSnapshot::Map snapshot;

            for (Model::AttributableNode* node : attributableNodes) {
                snapshot[node].push_back(node->attributeSnapshot(name));
                node->removeAttribute(name);
            }

            setEntityDefinitions(nodes);
            setEntityModels(nodes);

            return snapshot;
        }

        Model::EntityAttributeSnapshot::Map MapDocumentCommandFacade::performUpdateSpawnflag(const Model::AttributeName& name, const size_t flagIndex, const bool setFlag) {
            const Model::AttributableNodeList attributableNodes = allSelectedAttributableNodes();
            const Model::NodeList nodes(attributableNodes.begin(), attributableNodes.end());
            const Model::NodeList parents = collectParents(nodes.begin(), nodes.end());
            const Model::NodeList descendants = collectDescendants(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyDescendants(nodesWillChangeNotifier, nodesDidChangeNotifier, descendants);

            Model::EntityAttributeSnapshot::Map snapshot;

            Model::AttributableNodeList::const_iterator it, end;
            for (it = attributableNodes.begin(), end = attributableNodes.end(); it != end; ++it) {
                Model::AttributableNode* node = *it;
                snapshot[node].push_back(node->attributeSnapshot(name));

                int intValue = node->hasAttribute(name) ? std::atoi(node->attribute(name).c_str()) : 0;
                const int flagValue = (1 << flagIndex);

                if (setFlag)
                    intValue |= flagValue;
                else
                    intValue &= ~flagValue;

                StringStream str;
                str << intValue;
                node->addOrUpdateAttribute(name, str.str());
            }

            setEntityDefinitions(nodes);
            setEntityModels(nodes);

            return snapshot;
        }

        Model::EntityAttributeSnapshot::Map MapDocumentCommandFacade::performConvertColorRange(const Model::AttributeName& name, Assets::ColorRange::Type colorRange) {
            const Model::AttributableNodeList attributableNodes = allSelectedAttributableNodes();
            const Model::NodeList nodes(std::begin(attributableNodes), std::end(attributableNodes));
            const Model::NodeList parents = collectParents(std::begin(nodes), std::end(nodes));
            const Model::NodeList descendants = collectDescendants(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyDescendants(nodesWillChangeNotifier, nodesDidChangeNotifier, descendants);

            static const Model::AttributeValue DefaultValue = "";
            Model::EntityAttributeSnapshot::Map snapshot;

            for (Model::AttributableNode* node : attributableNodes) {
                const Model::AttributeValue& oldValue = node->attribute(name, DefaultValue);
                if (oldValue != DefaultValue) {
                    snapshot[node].push_back(node->attributeSnapshot(name));
                    node->addOrUpdateAttribute(name, Model::convertEntityColor(oldValue, colorRange));
                }
            }

            return snapshot;
        }

        Model::EntityAttributeSnapshot::Map MapDocumentCommandFacade::performRenameAttribute(const Model::AttributeName& oldName, const Model::AttributeName& newName) {
            const Model::AttributableNodeList attributableNodes = allSelectedAttributableNodes();
            const Model::NodeList nodes(std::begin(attributableNodes), std::end(attributableNodes));
            const Model::NodeList parents = collectParents(std::begin(nodes), std::end(nodes));
            const Model::NodeList descendants = collectDescendants(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyDescendants(nodesWillChangeNotifier, nodesDidChangeNotifier, descendants);

            Model::EntityAttributeSnapshot::Map snapshot;
            for (Model::AttributableNode* node : attributableNodes) {
                snapshot[node].push_back(node->attributeSnapshot(oldName));
                snapshot[node].push_back(node->attributeSnapshot(newName));
                node->renameAttribute(oldName, newName);
            }

            setEntityDefinitions(nodes);
            setEntityModels(nodes);

            return snapshot;
        }

        void MapDocumentCommandFacade::restoreAttributes(const Model::EntityAttributeSnapshot::Map& attributes) {
            const Model::AttributableNodeList attributableNodes = MapUtils::keyList(attributes);
            const Model::NodeList nodes(std::begin(attributableNodes), std::end(attributableNodes));
            const Model::NodeList parents = collectParents(std::begin(nodes), std::end(nodes));
            const Model::NodeList descendants = collectDescendants(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyDescendants(nodesWillChangeNotifier, nodesDidChangeNotifier, descendants);

            for (const auto& entry : attributes) {
                auto* node = entry.first;
                assert(node->parent() == nullptr || node->selected() || node->descendantSelected());

                const auto& snapshots = entry.second;
                for (const auto& snapshot : snapshots) {
                    snapshot.restore(node);
                }
            }

            setEntityDefinitions(nodes);
            setEntityModels(nodes);
        }

        std::vector<vm::polygon3> MapDocumentCommandFacade::performResizeBrushes(const std::vector<vm::polygon3>& polygons, const vm::vec3& delta) {
            std::vector<vm::polygon3> result;

            const Model::BrushList& selectedBrushes = m_selectedNodes.brushes();
            Model::NodeList changedNodes;
            Model::BrushFaceList faces;

            for (Model::Brush* brush : selectedBrushes) {
                Model::BrushFace* face = brush->findFace(polygons);
                if (face != nullptr) {
                    if (!brush->canMoveBoundary(m_worldBounds, face, delta))
                        return result;

                    changedNodes.push_back(brush);
                    faces.push_back(face);
                }
            }

            const auto parents = collectParents(std::begin(changedNodes), std::end(changedNodes));
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, changedNodes);

            for (auto* face : faces) {
                auto* brush = face->brush();
                assert(brush->selected());
                brush->moveBoundary(m_worldBounds, face, delta, pref(Preferences::TextureLock));
                result.push_back(face->polygon());
            }

            invalidateSelectionBounds();

            return result;
        }

        void MapDocumentCommandFacade::performMoveTextures(const vm::vec3f& cameraUp, const vm::vec3f& cameraRight, const vm::vec2f& delta) {
            for (auto* face : m_selectedBrushFaces) {
                face->moveTexture(vm::vec3(cameraUp), vm::vec3(cameraRight), delta);
            }
            brushFacesDidChangeNotifier(m_selectedBrushFaces);
        }

        void MapDocumentCommandFacade::performRotateTextures(const float angle) {
            for (auto* face : m_selectedBrushFaces) {
                face->rotateTexture(angle);
            }
            brushFacesDidChangeNotifier(m_selectedBrushFaces);
        }

        void MapDocumentCommandFacade::performShearTextures(const vm::vec2f& factors) {
            for (auto* face : m_selectedBrushFaces) {
                face->shearTexture(factors);
            }
            brushFacesDidChangeNotifier(m_selectedBrushFaces);
        }

        void MapDocumentCommandFacade::performCopyTexCoordSystemFromFace(const Model::TexCoordSystemSnapshot& coordSystemSnapshot, const Model::BrushFaceAttributes& attribs, const vm::plane3& sourceFacePlane, const Model::WrapStyle wrapStyle) {
            for (auto* face : m_selectedBrushFaces) {
                face->copyTexCoordSystemFromFace(coordSystemSnapshot, attribs, sourceFacePlane, wrapStyle);
            }
            brushFacesDidChangeNotifier(m_selectedBrushFaces);
        }

        void MapDocumentCommandFacade::performChangeBrushFaceAttributes(const Model::ChangeBrushFaceAttributesRequest& request) {
            const auto& faces = allSelectedBrushFaces();
            if (request.evaluate(faces)) {
                setTextures(faces);
                brushFacesDidChangeNotifier(faces);
            }
        }

        bool MapDocumentCommandFacade::performFindPlanePoints() {
            const Model::BrushList& brushes = m_selectedNodes.brushes();

            const Model::NodeList nodes(std::begin(brushes), std::end(brushes));
            const Model::NodeList parents = collectParents(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

            for (Model::Brush* brush : brushes) {
                brush->findIntegerPlanePoints(m_worldBounds);
            }

            return true;
        }

        bool MapDocumentCommandFacade::performSnapVertices(const FloatType snapTo) {
            const Model::BrushList& brushes = m_selectedNodes.brushes();

            const Model::NodeList nodes(std::begin(brushes), std::end(brushes));
            const Model::NodeList parents = collectParents(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

            size_t succeededBrushCount = 0;
            size_t failedBrushCount = 0;

            for (Model::Brush* brush : brushes) {
                if (brush->canSnapVertices(m_worldBounds, snapTo)) {
                    brush->snapVertices(m_worldBounds, snapTo, pref(Preferences::UVLock));
                    succeededBrushCount += 1;
                } else {
                    failedBrushCount += 1;
                }
            }

            invalidateSelectionBounds();

            if (succeededBrushCount > 0) {
                StringStream msg;
                msg << "Snapped vertices of " << succeededBrushCount << " " << StringUtils::safePlural(succeededBrushCount, "brush", "brushes");
                info(msg.str());
            }
            if (failedBrushCount > 0) {
                StringStream msg;
                msg << "Failed to snap vertices of " << failedBrushCount << " " << StringUtils::safePlural(failedBrushCount, "brush", "brushes");
                info(msg.str());
            }

            return true;
        }

        std::vector<vm::vec3> MapDocumentCommandFacade::performMoveVertices(const Model::BrushVerticesMap& vertices, const vm::vec3& delta) {
            const Model::NodeList& nodes = m_selectedNodes.nodes();
            const Model::NodeList parents = collectParents(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

            std::vector<vm::vec3> newVertexPositions;
            for (const auto& entry : vertices) {
                Model::Brush* brush = entry.first;
                const std::vector<vm::vec3>& oldPositions = entry.second;
                const std::vector<vm::vec3> newPositions = brush->moveVertices(m_worldBounds, oldPositions, delta, pref(Preferences::UVLock));
                VectorUtils::append(newVertexPositions, newPositions);
            }

            invalidateSelectionBounds();

            VectorUtils::sortAndRemoveDuplicates(newVertexPositions);
            return newVertexPositions;
        }

        std::vector<vm::segment3> MapDocumentCommandFacade::performMoveEdges(const Model::BrushEdgesMap& edges, const vm::vec3& delta) {
            const Model::NodeList& nodes = m_selectedNodes.nodes();
            const Model::NodeList parents = collectParents(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

            std::vector<vm::segment3> newEdgePositions;
            for (const auto& entry : edges) {
                Model::Brush* brush = entry.first;
                const std::vector<vm::segment3>& oldPositions = entry.second;
                const std::vector<vm::segment3> newPositions = brush->moveEdges(m_worldBounds, oldPositions, delta, pref(Preferences::UVLock));
                VectorUtils::append(newEdgePositions, newPositions);
            }

            invalidateSelectionBounds();

            VectorUtils::sortAndRemoveDuplicates(newEdgePositions);
            return newEdgePositions;
        }

        std::vector<vm::polygon3> MapDocumentCommandFacade::performMoveFaces(const Model::BrushFacesMap& faces, const vm::vec3& delta) {
            const Model::NodeList& nodes = m_selectedNodes.nodes();
            const Model::NodeList parents = collectParents(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

            std::vector<vm::polygon3> newFacePositions;
            for (const auto& entry : faces) {
                Model::Brush* brush = entry.first;
                const std::vector<vm::polygon3>& oldPositions = entry.second;
                const std::vector<vm::polygon3> newPositions = brush->moveFaces(m_worldBounds, oldPositions, delta, pref(Preferences::UVLock));
                VectorUtils::append(newFacePositions, newPositions);
            }

            invalidateSelectionBounds();

            VectorUtils::sortAndRemoveDuplicates(newFacePositions);
            return newFacePositions;
        }

        void MapDocumentCommandFacade::performAddVertices(const Model::VertexToBrushesMap& vertices) {
            const Model::NodeList& nodes = m_selectedNodes.nodes();
            const Model::NodeList parents = collectParents(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

            for (const auto& entry : vertices) {
                const vm::vec3& position = entry.first;
                const Model::BrushSet& brushes = entry.second;
                for (Model::Brush* brush : brushes)
                    brush->addVertex(m_worldBounds, position);
            }

            invalidateSelectionBounds();
        }

        void MapDocumentCommandFacade::performRemoveVertices(const Model::BrushVerticesMap& vertices) {
            const Model::NodeList& nodes = m_selectedNodes.nodes();
            const Model::NodeList parents = collectParents(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

            for (const auto& entry : vertices) {
                Model::Brush* brush = entry.first;
                const std::vector<vm::vec3>& positions = entry.second;
                brush->removeVertices(m_worldBounds, positions);
            }

            invalidateSelectionBounds();
        }

        void MapDocumentCommandFacade::performRebuildBrushGeometry(const Model::BrushList& brushes) {
            const Model::NodeList nodes = VectorUtils::cast<Model::Node*>(brushes);
            const Model::NodeList parents = collectParents(nodes);

            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

            for (Model::Brush* brush : brushes)
                brush->rebuildGeometry(m_worldBounds);

            invalidateSelectionBounds();
        }

        void MapDocumentCommandFacade::restoreSnapshot(Model::Snapshot* snapshot) {
            if (!m_selectedNodes.empty()) {
                const Model::NodeList& nodes = m_selectedNodes.nodes();
                const Model::NodeList parents = collectParents(nodes);

                Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyParents(nodesWillChangeNotifier, nodesDidChangeNotifier, parents);
                Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);

                snapshot->restoreNodes(m_worldBounds);

                invalidateSelectionBounds();
            }

            const Model::BrushFaceList brushFaces = allSelectedBrushFaces();
            if (!brushFaces.empty()) {
                snapshot->restoreBrushFaces();
                setTextures(brushFaces);
                brushFacesDidChangeNotifier(brushFaces);
            }
        }

        void MapDocumentCommandFacade::performSetEntityDefinitionFile(const Assets::EntityDefinitionFileSpec& spec) {
            const Model::NodeList nodes(1, m_world.get());
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);
            Notifier<>::NotifyAfter notifyEntityDefinitions(entityDefinitionsDidChangeNotifier);

            // to avoid backslashes being misinterpreted as escape sequences
            const String formatted = StringUtils::replaceAll(spec.asString(), "\\", "/");
            m_world->addOrUpdateAttribute(Model::AttributeNames::EntityDefinitions, formatted);
            reloadEntityDefinitionsInternal();
        }

        void MapDocumentCommandFacade::performSetTextureCollections(const IO::Path::List& paths) {
            const Model::NodeList nodes(1, m_world.get());
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);
            Notifier<>::NotifyBeforeAndAfter notifyTextureCollections(textureCollectionsWillChangeNotifier, textureCollectionsDidChangeNotifier);

            m_game->updateTextureCollections(*m_world, paths);
            unsetTextures();
            loadTextures();
            setTextures();
        }

        void MapDocumentCommandFacade::performSetMods(const StringList& mods) {
            const Model::NodeList nodes(1, m_world.get());
            Notifier<const Model::NodeList&>::NotifyBeforeAndAfter notifyNodes(nodesWillChangeNotifier, nodesDidChangeNotifier, nodes);
            Notifier<>::NotifyAfter notifyMods(modsDidChangeNotifier);

            unsetEntityModels();
            unsetEntityDefinitions();
            clearEntityModels();

            if (mods.empty()) {
                m_world->removeAttribute(Model::AttributeNames::Mods);
            } else {
                const String newValue = StringUtils::join(mods, ";");
                m_world->addOrUpdateAttribute(Model::AttributeNames::Mods, newValue);
            }

            updateGameSearchPaths();
            setEntityDefinitions();
            setEntityModels();
        }

        void MapDocumentCommandFacade::doSetIssueHidden(Model::Issue* issue, const bool hidden) {
            if (issue->hidden() != hidden) {
                issue->setHidden(hidden);
                incModificationCount();
            }
        }

        void MapDocumentCommandFacade::incModificationCount(const size_t delta) {
            m_modificationCount += delta;
            documentModificationStateDidChangeNotifier();
        }

        void MapDocumentCommandFacade::decModificationCount(const size_t delta) {
            assert(m_modificationCount >= delta);
            m_modificationCount -= delta;
            documentModificationStateDidChangeNotifier();
        }

        void MapDocumentCommandFacade::bindObservers() {
            m_commandProcessor.commandDoNotifier.addObserver(commandDoNotifier);
            m_commandProcessor.commandDoneNotifier.addObserver(commandDoneNotifier);
            m_commandProcessor.commandDoFailedNotifier.addObserver(commandDoFailedNotifier);
            m_commandProcessor.commandUndoNotifier.addObserver(commandUndoNotifier);
            m_commandProcessor.commandUndoneNotifier.addObserver(commandUndoneNotifier);
            m_commandProcessor.commandUndoFailedNotifier.addObserver(commandUndoFailedNotifier);
            m_commandProcessor.transactionDoneNotifier.addObserver(transactionDoneNotifier);
            m_commandProcessor.transactionUndoneNotifier.addObserver(transactionUndoneNotifier);
            documentWasNewedNotifier.addObserver(this, &MapDocumentCommandFacade::documentWasNewed);
            documentWasLoadedNotifier.addObserver(this, &MapDocumentCommandFacade::documentWasLoaded);
        }

        void MapDocumentCommandFacade::documentWasNewed(MapDocument* document) {
            m_commandProcessor.clear();
        }

        void MapDocumentCommandFacade::documentWasLoaded(MapDocument* document) {
            m_commandProcessor.clear();
        }

        bool MapDocumentCommandFacade::doCanUndoLastCommand() const {
            return m_commandProcessor.hasLastCommand();
        }

        bool MapDocumentCommandFacade::doCanRedoNextCommand() const {
            return m_commandProcessor.hasNextCommand();
        }

        const String& MapDocumentCommandFacade::doGetLastCommandName() const {
            return m_commandProcessor.lastCommandName();
        }

        const String& MapDocumentCommandFacade::doGetNextCommandName() const {
            return m_commandProcessor.nextCommandName();
        }

        void MapDocumentCommandFacade::doUndoLastCommand() {
            m_commandProcessor.undoLastCommand();
        }

        void MapDocumentCommandFacade::doRedoNextCommand() {
            m_commandProcessor.redoNextCommand();
        }

        bool MapDocumentCommandFacade::doHasRepeatableCommands() const {
            return m_commandProcessor.hasRepeatableCommands();
        }

        bool MapDocumentCommandFacade::doRepeatLastCommands() {
            return m_commandProcessor.repeatLastCommands();
        }

        void MapDocumentCommandFacade::doClearRepeatableCommands() {
            m_commandProcessor.clearRepeatableCommands();
        }

        void MapDocumentCommandFacade::doBeginTransaction(const String& name) {
            m_commandProcessor.beginGroup(name);
        }

        void MapDocumentCommandFacade::doEndTransaction() {
            m_commandProcessor.endGroup();
        }

        void MapDocumentCommandFacade::doRollbackTransaction() {
            m_commandProcessor.rollbackGroup();
        }

        bool MapDocumentCommandFacade::doSubmit(Command::Ptr command) {
            return m_commandProcessor.submitCommand(command);
        }

        bool MapDocumentCommandFacade::doSubmitAndStore(UndoableCommand::Ptr command) {
            return m_commandProcessor.submitAndStoreCommand(command);
        }
    }
}
