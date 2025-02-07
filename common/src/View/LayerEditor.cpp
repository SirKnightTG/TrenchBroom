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

#include "LayerEditor.h"

#include "Model/Brush.h"
#include "Model/CollectSelectableNodesVisitor.h"
#include "Model/Entity.h"
#include "Model/FindGroupVisitor.h"
#include "Model/FindLayerVisitor.h"
#include "Model/Group.h"
#include "Model/Layer.h"
#include "Model/World.h"
#include "View/BorderLine.h"
#include "View/LayerListBox.h"
#include "View/MapDocument.h"
#include "View/wxUtils.h"

#include <QInputDialog>
#include <QMenu>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QAbstractButton>

#include <set>

namespace TrenchBroom {
    namespace View {
        LayerEditor::LayerEditor(MapDocumentWPtr document, QWidget *parent) :
        QWidget(parent),
        m_document(document),
        m_layerList(nullptr) {
            createGui();

            updateButtons();
        }

        void LayerEditor::onSetCurrentLayer(Model::Layer* layer) {
            auto document = lock(m_document);
            if (layer->locked()) {
                document->resetLock(Model::NodeList(1, layer));
            }
            if (layer->hidden()) {
                document->resetVisibility(Model::NodeList(1, layer));
            }
            document->setCurrentLayer(layer);

            updateButtons();
        }

        void LayerEditor::onLayerRightClick(Model::Layer* layer) {
            QMenu popupMenu;
            QAction* moveSelectionToLayerAction = popupMenu.addAction(tr("Move selection to layer"), this, &LayerEditor::onMoveSelectionToLayer);
            popupMenu.addAction(tr("Select all in layer"), this, &LayerEditor::onSelectAllInLayer);
            popupMenu.addSeparator();
            QAction* toggleLayerVisibleAction = popupMenu.addAction(layer->hidden() ? tr("Show layer") : tr("Hide layer"), this, &LayerEditor::onToggleLayerVisibleFromMenu);
            QAction* toggleLayerLockedAction = popupMenu.addAction(layer->locked() ? tr("Unlock layer") : tr("Lock layer"), this, &LayerEditor::onToggleLayerLockedFromMenu);
            popupMenu.addSeparator();
            QAction* removeLayerAction = popupMenu.addAction(tr("Remove layer"), this, &LayerEditor::onRemoveLayer);

            moveSelectionToLayerAction->setEnabled(canMoveSelectionToLayer());
            toggleLayerVisibleAction->setEnabled(canToggleLayerVisible());
            toggleLayerLockedAction->setEnabled(canToggleLayerLocked());
            removeLayerAction->setEnabled(canRemoveLayer());

            popupMenu.exec(QCursor::pos());
        }

        void LayerEditor::onToggleLayerVisibleFromMenu() {
            toggleLayerVisible(m_layerList->selectedLayer());
        }

        void LayerEditor::onToggleLayerVisibleFromList(Model::Layer* layer) {
            toggleLayerVisible(layer);
        }

        bool LayerEditor::canToggleLayerVisible() const {
            auto* layer = m_layerList->selectedLayer();
            if (layer == nullptr) {
                return false;
            }

            auto document = lock(m_document);
            if (!layer->hidden() && layer == document->currentLayer()) {
                return false;
            }

            return true;
        }

        void LayerEditor::toggleLayerVisible(Model::Layer* layer) {
            ensure(layer != nullptr, "layer is null");
            auto document = lock(m_document);
            if (!layer->hidden()) {
                document->hide(Model::NodeList(1, layer));
            } else {
                document->resetVisibility(Model::NodeList(1, layer));
            }
        }

        void LayerEditor::onToggleLayerLockedFromMenu() {
            toggleLayerLocked(m_layerList->selectedLayer());
        }

        void LayerEditor::onToggleLayerLockedFromList(Model::Layer* layer) {
            toggleLayerLocked(layer);
        }

        bool LayerEditor::canToggleLayerLocked() const {
            auto* layer = m_layerList->selectedLayer();
            if (layer == nullptr) {
                return false;
            }

            auto document = lock(m_document);
            if (!layer->locked() && layer == document->currentLayer()) {
                return false;
            }

            return true;
        }

        void LayerEditor::toggleLayerLocked(Model::Layer* layer) {
            ensure(layer != nullptr, "layer is null");
            auto document = lock(m_document);
            if (!layer->locked()) {
                document->lock(Model::NodeList(1, layer));
            } else {
                document->resetLock(Model::NodeList(1, layer));
            }
        }

        class LayerEditor::CollectMoveableNodes : public Model::NodeVisitor {
        private:
            Model::World* m_world;
            Model::NodeSet m_selectNodes;
            Model::NodeSet m_moveNodes;
        public:
            CollectMoveableNodes(Model::World* world) : m_world(world) {}

            const Model::NodeList selectNodes() const {
                return Model::NodeList(std::begin(m_selectNodes), std::end(m_selectNodes));
            }

            const Model::NodeList moveNodes() const {
                return Model::NodeList(std::begin(m_moveNodes), std::end(m_moveNodes));
            }
        private:
            void doVisit(Model::World* world) override   {}
            void doVisit(Model::Layer* layer) override   {}

            void doVisit(Model::Group* group) override   {
                assert(group->selected());

                if (!group->grouped()) {
                    m_moveNodes.insert(group);
                    m_selectNodes.insert(group);
                }
            }

            void doVisit(Model::Entity* entity) override {
                assert(entity->selected());

                if (!entity->grouped()) {
                    m_moveNodes.insert(entity);
                    m_selectNodes.insert(entity);
                }
            }

            void doVisit(Model::Brush* brush) override   {
                assert(brush->selected());
                if (!brush->grouped()) {
                    auto* entity = brush->entity();
                    if (entity == m_world) {
                        m_moveNodes.insert(brush);
                        m_selectNodes.insert(brush);
                    } else {
                        if (m_moveNodes.insert(entity).second) {
                            const Model::NodeList& siblings = entity->children();
                            m_selectNodes.insert(std::begin(siblings), std::end(siblings));
                        }
                    }
                }
            }
        };

        void LayerEditor::onMoveSelectionToLayer() {
            auto* layer = m_layerList->selectedLayer();
            ensure(layer != nullptr, "layer is null");

            auto document = lock(m_document);
            Transaction transaction(document, "Move Nodes to " + layer->name());
            moveSelectedNodesToLayer(document, layer);
        }

        bool LayerEditor::canMoveSelectionToLayer() const {
            const auto* layer = m_layerList->selectedLayer();
            if (layer == nullptr) {
                return false;
            }

            auto document = lock(m_document);
            const auto& nodes = document->selectedNodes().nodes();
            if (nodes.empty()) {
                return false;
            }

            for (auto* node : nodes) {
                auto* nodeGroup = Model::findGroup(node);
                if (nodeGroup != nullptr) {
                    return false;
                }
            }

            for (auto* node : nodes) {
                auto* nodeLayer = Model::findLayer(node);
                if (nodeLayer != layer) {
                    return true;
                }
            }

            return true;
        }

        void LayerEditor::onSelectAllInLayer() {
            auto* layer = m_layerList->selectedLayer();
            ensure(layer != nullptr, "layer is null");

            auto document = lock(m_document);

            Model::CollectSelectableNodesVisitor visitor(document->editorContext());
            layer->recurse(visitor);

            const auto& nodes = visitor.nodes();
            document->deselectAll();
            document->select(nodes);
        }

        void LayerEditor::onAddLayer() {
            const String name = queryLayerName();
            if (!name.empty()) {
                auto document = lock(m_document);
                auto* world = document->world();
                auto* layer = world->createLayer(name, document->worldBounds());

                Transaction transaction(document, "Create Layer " + layer->name());
                document->addNode(layer, world);
                document->setCurrentLayer(layer);
                m_layerList->setSelectedLayer(layer);
            }
        }

        String LayerEditor::queryLayerName() {
            while (true) {
                bool ok = false;
                const String name = QInputDialog::getText(this, "Enter a name", "Layer Name", QLineEdit::Normal, "Unnamed", &ok).toStdString();

                if (!ok) {
                    return "";
                }

                if (StringUtils::isBlank(name)) {
                    if (QMessageBox::warning(this, "Error", "Layer names cannot be blank.", QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) != QMessageBox::Ok) {
                        return "";
                    }
                } else if (StringUtils::containsCaseInsensitive(name, "\"")) {
                    if (QMessageBox::warning(this, "Error", "Layer names cannot contain double quotes.", QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) != QMessageBox::Ok) {
                        return "";
                    }
                } else {
                    return name;
                }
            }
        }

        void LayerEditor::onRemoveLayer() {
            auto* layer = m_layerList->selectedLayer();
            ensure(layer != nullptr, "layer is null");

            auto document = lock(m_document);
            auto* defaultLayer = document->world()->defaultLayer();

            Transaction transaction(document, "Remove Layer " + layer->name());
            document->deselectAll();
            if (layer->hasChildren()) {
                document->reparentNodes(defaultLayer, layer->children());
            }
            if (document->currentLayer() == layer) {
                document->setCurrentLayer(defaultLayer);
            }
            document->removeNode(layer);
        }

        bool LayerEditor::canRemoveLayer() const {
            const auto* layer = m_layerList->selectedLayer();
            if (layer == nullptr) {
                return false;
            }

            if (findVisibleAndUnlockedLayer(layer) == nullptr) {
                return false;
            }

            auto document = lock(m_document);
            return (layer != document->world()->defaultLayer());
        }

        void LayerEditor::onShowAllLayers() {
            auto document = lock(m_document);
            const auto layers = document->world()->allLayers();
            document->resetVisibility(Model::NodeList(std::begin(layers), std::end(layers)));
        }

        Model::Layer* LayerEditor::findVisibleAndUnlockedLayer(const Model::Layer* except) const {
            auto document = lock(m_document);
            if (!document->world()->defaultLayer()->locked() && !document->world()->defaultLayer()->hidden()) {
                return document->world()->defaultLayer();
            }

            const auto& layers = document->world()->customLayers();
            for (auto* layer : layers) {
                if (layer != except && !layer->locked() && !layer->hidden()) {
                    return layer;
                }
            }

            return nullptr;
        }

        void LayerEditor::moveSelectedNodesToLayer(MapDocumentSPtr document, Model::Layer* layer) {
            const auto& selectedNodes = document->selectedNodes().nodes();

            CollectMoveableNodes visitor(document->world());
            Model::Node::accept(std::begin(selectedNodes), std::end(selectedNodes), visitor);

            const auto moveNodes = visitor.moveNodes();
            if (!moveNodes.empty()) {
                document->deselectAll();
                document->reparentNodes(layer, visitor.moveNodes());
                if (!layer->hidden() && !layer->locked()) {
                    document->select(visitor.selectNodes());
                }
            }
        }

        void LayerEditor::createGui() {
            m_layerList = new LayerListBox(m_document, this);
            connect(m_layerList, &LayerListBox::layerSetCurrent, this, &LayerEditor::onSetCurrentLayer);
            connect(m_layerList, &LayerListBox::layerRightClicked, this, &LayerEditor::onLayerRightClick);
            connect(m_layerList, &LayerListBox::layerVisibilityToggled, this, &LayerEditor::onToggleLayerVisibleFromList);
            connect(m_layerList, &LayerListBox::layerLockToggled, this, &LayerEditor::onToggleLayerLockedFromList);
            connect(m_layerList, &LayerListBox::itemSelectionChanged, this, &LayerEditor::updateButtons);

            m_addLayerButton = createBitmapButton("Add.png", tr("Add a new layer from the current selection"));
            m_removeLayerButton = createBitmapButton("Remove.png", tr("Remove the selected layer and move its objects to the default layer"));
            m_showAllLayersButton = createBitmapButton("Hidden_off.png", tr("Show all layers"));

            connect(m_addLayerButton, &QAbstractButton::pressed, this, &LayerEditor::onAddLayer);
            connect(m_removeLayerButton, &QAbstractButton::pressed, this, &LayerEditor::onRemoveLayer);
            connect(m_showAllLayersButton, &QAbstractButton::pressed, this, &LayerEditor::onShowAllLayers);

            auto* buttonSizer = new QHBoxLayout();
            buttonSizer->addWidget(m_addLayerButton);
            buttonSizer->addWidget(m_removeLayerButton);
            buttonSizer->addWidget(m_showAllLayersButton);
            buttonSizer->addStretch(1);

            auto* sizer = new QVBoxLayout();
            sizer->setContentsMargins(0, 0, 0, 0);
            sizer->setSpacing(0);
            sizer->addWidget(m_layerList, 1);
            sizer->addWidget(new BorderLine(BorderLine::Direction_Horizontal), 0);
            sizer->addLayout(buttonSizer, 0);
            setLayout(sizer);
        }

        void LayerEditor::updateButtons() {
            m_removeLayerButton->setEnabled(canRemoveLayer());
        }
    }
}
