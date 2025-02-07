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

#include "VertexTool.h"

#include "Macros.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Model/Brush.h"
#include "Renderer/RenderBatch.h"
#include "View/Grid.h"
#include "View/MapDocument.h"
#include "View/VertexCommand.h"

#include <cassert>
#include <tuple>

namespace TrenchBroom {
    namespace View {
        VertexTool::VertexTool(MapDocumentWPtr document) :
        VertexToolBase(document),
        m_mode(Mode_Move),
        m_guideRenderer(document) {}

        Model::BrushSet VertexTool::findIncidentBrushes(const vm::vec3& handle) const {
            return findIncidentBrushes(m_vertexHandles, handle);
        }

        Model::BrushSet VertexTool::findIncidentBrushes(const vm::segment3& handle) const {
            return findIncidentBrushes(m_edgeHandles, handle);
        }

        Model::BrushSet VertexTool::findIncidentBrushes(const vm::polygon3& handle) const {
            return findIncidentBrushes(m_faceHandles, handle);
        }

        void VertexTool::pick(const vm::ray3& pickRay, const Renderer::Camera& camera, Model::PickResult& pickResult) const {
            MapDocumentSPtr document = lock(m_document);
            const Grid& grid = document->grid();

            m_vertexHandles.pick(pickRay, camera, pickResult);
            m_edgeHandles.pickGridHandle(pickRay, camera, grid, pickResult);
            m_faceHandles.pickGridHandle(pickRay, camera, grid, pickResult);
        }

        bool VertexTool::deselectAll() {
            if (VertexToolBase::deselectAll()) {
                resetModeAfterDeselection();
                return true;
            }
            return false;
        }

        VertexHandleManager& VertexTool::handleManager() {
            return m_vertexHandles;
        }

        const VertexHandleManager& VertexTool::handleManager() const {
            return m_vertexHandles;
        }

        bool VertexTool::startMove(const Model::Hit::List& hits) {
            const auto& hit = hits.front();
            if (hit.hasType(EdgeHandleManager::HandleHit | FaceHandleManager::HandleHit)) {
                m_vertexHandles.deselectAll();
                if (hit.hasType(EdgeHandleManager::HandleHit)) {
                    const auto& handle = std::get<0>(hit.target<EdgeHandleManager::HitType>());
                    m_edgeHandles.select(handle);
                    m_mode = Mode_Split_Edge;
                } else {
                    const auto& handle = std::get<0>(hit.target<FaceHandleManager::HitType>());
                    m_faceHandles.select(handle);
                    m_mode = Mode_Split_Face;
                }
                refreshViews();
            } else {
                m_mode = Mode_Move;
            }

            if (!VertexToolBase::startMove(hits)) {
                m_mode = Mode_Move;
                return false;
            } else {
                return true;
            }
        }

        VertexTool::MoveResult VertexTool::move(const vm::vec3& delta) {
            MapDocumentSPtr document = lock(m_document);

            if (m_mode == Mode_Move) {
                const auto handles = m_vertexHandles.selectedHandles();
                const auto brushMap = buildBrushMap(m_vertexHandles, std::begin(handles), std::end(handles));

                const MapDocument::MoveVerticesResult result = document->moveVertices(brushMap, delta);
                if (result.success) {
					if (!result.hasRemainingVertices) {
						return MR_Cancel;
					} else {
						m_dragHandlePosition = m_dragHandlePosition + delta;
						return MR_Continue;
					}
				} else {
					return MR_Deny;
				}
            } else {
                Model::BrushSet brushes;
                if (m_mode == Mode_Split_Edge) {
                    if (m_edgeHandles.selectedHandleCount() == 1) {
                        const vm::segment3 handle = m_edgeHandles.selectedHandles().front();
                        brushes = findIncidentBrushes(handle);
                    }
                } else {
                    assert(m_mode == Mode_Split_Face);
                    if (m_faceHandles.selectedHandleCount() == 1) {
                        const vm::polygon3 handle = m_faceHandles.selectedHandles().front();
                        brushes = findIncidentBrushes(handle);
                    }
                }

                if (!brushes.empty()) {
                    const Model::VertexToBrushesMap vertices { std::make_pair(m_dragHandlePosition + delta, brushes) };
                    if (document->addVertices(vertices)) {
                        m_mode = Mode_Move;
                        m_edgeHandles.deselectAll();
                        m_faceHandles.deselectAll();
                        m_dragHandlePosition = m_dragHandlePosition + delta;
                        m_vertexHandles.select(m_dragHandlePosition);
                    }
                    return MR_Continue;
                }

                // Catch all failure cases: no brushes were selected or vertices could not be added:
                return MR_Deny;
            }
        }

        void VertexTool::endMove() {
            VertexToolBase::endMove();
            m_edgeHandles.deselectAll();
            m_faceHandles.deselectAll();
            m_mode = Mode_Move;
        }
        void VertexTool::cancelMove() {
			VertexToolBase::cancelMove();
            m_edgeHandles.deselectAll();
            m_faceHandles.deselectAll();
            m_mode = Mode_Move;
        }

        const vm::vec3& VertexTool::getHandlePosition(const Model::Hit& hit) const {
            assert(hit.isMatch());
            assert(hit.hasType(VertexHandleManager::HandleHit | EdgeHandleManager::HandleHit | FaceHandleManager::HandleHit));

            if (hit.hasType(VertexHandleManager::HandleHit)) {
                return hit.target<vm::vec3>();
            } else if (hit.hasType(EdgeHandleManager::HandleHit)) {
                return std::get<1>(hit.target<EdgeHandleManager::HitType>());
            } else {
                return std::get<1>(hit.target<FaceHandleManager::HitType>());
            }
        }

        String VertexTool::actionName() const {
            switch (m_mode) {
                case Mode_Move:
                    return StringUtils::safePlural(m_vertexHandles.selectedHandleCount(), "Move Vertex", "Move Vertices");
                case Mode_Split_Edge:
                    return "Split Edge";
                case Mode_Split_Face:
                    return "Split Face";
                switchDefault();
            }
        }

        void VertexTool::removeSelection() {
            assert(canRemoveSelection());

            const auto handles = m_vertexHandles.selectedHandles();
            const auto brushMap = buildBrushMap(m_vertexHandles, std::begin(handles), std::end(handles));

            Transaction transaction(m_document, StringUtils::safePlural(handleManager().selectedHandleCount(), "Remove Vertex", "Remove Vertices"));
            lock(m_document)->removeVertices(brushMap);
        }

        void VertexTool::renderGuide(Renderer::RenderContext& renderContext, Renderer::RenderBatch& renderBatch, const vm::vec3& position) const {
            m_guideRenderer.setPosition(position);
            m_guideRenderer.setColor(Color(pref(Preferences::HandleColor), 0.5f));
            renderBatch.add(&m_guideRenderer);
        }

        bool VertexTool::doActivate() {
            VertexToolBase::doActivate();

            m_edgeHandles.clear();
            m_faceHandles.clear();

            const Model::BrushList& brushes = selectedBrushes();
            m_edgeHandles.addHandles(std::begin(brushes), std::end(brushes));
            m_faceHandles.addHandles(std::begin(brushes), std::end(brushes));

            m_mode = Mode_Move;
            return true;
        }

        bool VertexTool::doDeactivate() {
            VertexToolBase::doDeactivate();

            m_edgeHandles.clear();
            m_faceHandles.clear();
            return true;
        }

        void VertexTool::addHandles(const Model::NodeList& nodes) {
            AddHandles<vm::vec3> addVertexHandles(m_vertexHandles);
            Model::Node::accept(std::begin(nodes), std::end(nodes), addVertexHandles);

            AddHandles<vm::segment3> addEdgeHandles(m_edgeHandles);
            Model::Node::accept(std::begin(nodes), std::end(nodes), addEdgeHandles);

            AddHandles<vm::polygon3> addFaceHandles(m_faceHandles);
            Model::Node::accept(std::begin(nodes), std::end(nodes), addFaceHandles);
        }

        void VertexTool::removeHandles(const Model::NodeList& nodes) {
            RemoveHandles<vm::vec3> removeVertexHandles(m_vertexHandles);
            Model::Node::accept(std::begin(nodes), std::end(nodes), removeVertexHandles);

            RemoveHandles<vm::segment3> removeEdgeHandles(m_edgeHandles);
            Model::Node::accept(std::begin(nodes), std::end(nodes), removeEdgeHandles);

            RemoveHandles<vm::polygon3> removeFaceHandles(m_faceHandles);
            Model::Node::accept(std::begin(nodes), std::end(nodes), removeFaceHandles);
        }

        void VertexTool::addHandles(VertexCommand* command) {
            command->addHandles(m_vertexHandles);
            command->addHandles(m_edgeHandles);
            command->addHandles(m_faceHandles);
        }

        void VertexTool::removeHandles(VertexCommand* command) {
            command->removeHandles(m_vertexHandles);
            command->removeHandles(m_edgeHandles);
            command->removeHandles(m_faceHandles);
        }

        void VertexTool::resetModeAfterDeselection() {
            if (!m_vertexHandles.anySelected()) {
                m_mode = Mode_Move;
            }
        }
    }
}
