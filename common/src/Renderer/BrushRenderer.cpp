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

#include "BrushRenderer.h"

#include "Preferences.h"
#include "PreferenceManager.h"
#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/BrushGeometry.h"
#include "Model/EditorContext.h"
#include "Model/TagAttribute.h"
#include "Renderer/IndexArrayMapBuilder.h"
#include "Renderer/BrushRendererArrays.h"
#include "Renderer/BrushRendererBrushCache.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderUtils.h"

#include <cassert>
#include <cstring>

namespace TrenchBroom {
    namespace Renderer {
        // Filter

        BrushRenderer::Filter::Filter() {}

        BrushRenderer::Filter::Filter(const Filter& other) {}

        BrushRenderer::Filter::~Filter() {}

        BrushRenderer::Filter& BrushRenderer::Filter::operator=(const Filter& other) { return *this; }

        BrushRenderer::Filter::RenderSettings BrushRenderer::Filter::renderNothing() {
            return std::make_tuple(FaceRenderPolicy::RenderNone, EdgeRenderPolicy::RenderNone);
        }

        // DefaultFilter

        BrushRenderer::DefaultFilter::~DefaultFilter() {}
        BrushRenderer::DefaultFilter::DefaultFilter(const Model::EditorContext& context) : m_context(context) {}

        BrushRenderer::DefaultFilter::DefaultFilter(const DefaultFilter& other) :
        Filter(),
        m_context(other.m_context) {}

        bool BrushRenderer::DefaultFilter::visible(const Model::Brush* brush) const    {
            return m_context.visible(brush);
        }

        bool BrushRenderer::DefaultFilter::visible(const Model::BrushFace* face) const {
            return m_context.visible(face);
        }

        bool BrushRenderer::DefaultFilter::visible(const Model::BrushEdge* edge) const {
            return m_context.visible(edge->firstFace()->payload()) || m_context.visible(edge->secondFace()->payload());
        }

        bool BrushRenderer::DefaultFilter::editable(const Model::Brush* brush) const {
            return m_context.editable(brush);
        }

        bool BrushRenderer::DefaultFilter::editable(const Model::BrushFace* face) const {
            return m_context.editable(face);
        }

        bool BrushRenderer::DefaultFilter::selected(const Model::Brush* brush) const {
            return brush->selected() || brush->parentSelected();
        }

        bool BrushRenderer::DefaultFilter::selected(const Model::BrushFace* face) const {
            return face->selected();
        }

        bool BrushRenderer::DefaultFilter::selected(const Model::BrushEdge* edge) const {
            const Model::BrushFace* first = edge->firstFace()->payload();
            const Model::BrushFace* second = edge->secondFace()->payload();
            const Model::Brush* brush = first->brush();
            assert(second->brush() == brush);
            return selected(brush) || selected(first) || selected(second);
        }

        bool BrushRenderer::DefaultFilter::hasSelectedFaces(const Model::Brush* brush) const {
            return brush->descendantSelected();
        }

        // NoFilter

        BrushRenderer::Filter::RenderSettings BrushRenderer::NoFilter::markFaces(const Model::Brush* brush) const {
            for (Model::BrushFace* face : brush->faces()) {
                face->setMarked(true);
            }
            return std::make_tuple(FaceRenderPolicy::RenderMarked,
                                   EdgeRenderPolicy::RenderAll);
        }

        // BrushRenderer

        BrushRenderer::BrushRenderer() :
        m_filter(std::make_unique<NoFilter>()),
        m_showEdges(false),
        m_grayscale(false),
        m_tint(false),
        m_showOccludedEdges(false),
        m_transparent(false),
        m_transparencyAlpha(1.0f),
        m_showHiddenBrushes(false) {
            clear();
        }

        void BrushRenderer::addBrushes(const Model::BrushList& brushes) {
            for (auto* brush : brushes) {
                addBrush(brush);
            }
        }

        void BrushRenderer::setBrushes(const Model::BrushList& brushes) {
            // start with adding nothing, and removing everything
            std::set<const Model::Brush*> toAdd;
            std::set<const Model::Brush*> toRemove = m_allBrushes;

            // update toAdd and toRemove using the input list
            for (const auto* brush : brushes) {
                const auto it = toRemove.find(brush);
                if (it != toRemove.end()) {
                    toRemove.erase(it);
                } else {
                    toAdd.insert(brush);
                }
            }

            for (auto brush : toRemove) {
                removeBrush(brush);
            }
            for (auto brush : toAdd) {
                addBrush(brush);
            }
        }

        void BrushRenderer::invalidate() {
            for (auto& brush : m_allBrushes) {
                // this will also invalidate already invalid brushes, which
                // is unnecessary
                removeBrushFromVbo(brush);
            }
            m_invalidBrushes = m_allBrushes;

            assert(m_brushInfo.empty());
            assert(m_transparentFaces->empty());
            assert(m_opaqueFaces->empty());
        }

        void BrushRenderer::invalidateBrushes(const Model::BrushList& brushes) {
            for (auto& brush : brushes) {
                // skip brushes that are not in the renderer
                if (m_allBrushes.find(brush) == m_allBrushes.end()) {
                    assert(m_brushInfo.find(brush) == m_brushInfo.end());
                    assert(m_invalidBrushes.find(brush) == m_invalidBrushes.end());
                    continue;
                }
                // if it's not in the invalid set, put it in
                const auto it = m_invalidBrushes.find(brush);
                if (it == m_invalidBrushes.end()) {
                    removeBrushFromVbo(brush);
                    m_invalidBrushes.insert(brush);
                }
            }
        }

        bool BrushRenderer::valid() const {
            return m_invalidBrushes.empty();
        }

        void BrushRenderer::clear() {
            m_brushInfo.clear();
            m_allBrushes.clear();
            m_invalidBrushes.clear();

            m_vertexArray = std::make_shared<BrushVertexArray>();
            m_edgeIndices = std::make_shared<BrushIndexArray>();
            m_transparentFaces = std::make_shared<TextureToBrushIndicesMap>();
            m_opaqueFaces = std::make_shared<TextureToBrushIndicesMap>();

            m_opaqueFaceRenderer = FaceRenderer(m_vertexArray, m_opaqueFaces, m_faceColor);
            m_transparentFaceRenderer = FaceRenderer(m_vertexArray, m_transparentFaces, m_faceColor);
            m_edgeRenderer = IndexedEdgeRenderer(m_vertexArray, m_edgeIndices);
        }

        void BrushRenderer::setFaceColor(const Color& faceColor) {
            m_faceColor = faceColor;
        }

        void BrushRenderer::setShowEdges(const bool showEdges) {
            m_showEdges = showEdges;
        }

        void BrushRenderer::setEdgeColor(const Color& edgeColor) {
            m_edgeColor = edgeColor;
        }

        void BrushRenderer::setGrayscale(const bool grayscale) {
            m_grayscale = grayscale;
        }

        void BrushRenderer::setTint(const bool tint) {
            m_tint = tint;
        }

        void BrushRenderer::setTintColor(const Color& tintColor) {
            m_tintColor = tintColor;
        }

        void BrushRenderer::setShowOccludedEdges(const bool showOccludedEdges) {
            m_showOccludedEdges = showOccludedEdges;
        }

        void BrushRenderer::setOccludedEdgeColor(const Color& occludedEdgeColor) {
            m_occludedEdgeColor = occludedEdgeColor;
        }

        void BrushRenderer::setForceTransparent(const bool transparent) {
            m_transparent = transparent;
        }

        void BrushRenderer::setTransparencyAlpha(const float transparencyAlpha) {
            m_transparencyAlpha = transparencyAlpha;
        }

        void BrushRenderer::setShowHiddenBrushes(const bool showHiddenBrushes) {
            if (showHiddenBrushes != m_showHiddenBrushes) {
                m_showHiddenBrushes = showHiddenBrushes;
                invalidate();
            }
        }

        void BrushRenderer::render(RenderContext& renderContext, RenderBatch& renderBatch) {
            renderOpaque(renderContext, renderBatch);
            renderTransparent(renderContext, renderBatch);
        }

        void BrushRenderer::renderOpaque(RenderContext& renderContext, RenderBatch& renderBatch) {
            if (!m_allBrushes.empty()) {
                if (!valid()) {
                    validate();
                }
                if (renderContext.showFaces()) {
                    renderOpaqueFaces(renderBatch);
                }
                if (renderContext.showEdges() || m_showEdges) {
                    renderEdges(renderBatch);
                }
            }
        }

        void BrushRenderer::renderTransparent(RenderContext& renderContext, RenderBatch& renderBatch) {
            if (!m_allBrushes.empty()) {
                if (!valid()) {
                    validate();
                }
                if (renderContext.showFaces()) {
                    renderTransparentFaces(renderBatch);
                }
            }
        }

        void BrushRenderer::renderOpaqueFaces(RenderBatch& renderBatch) {
            m_opaqueFaceRenderer.setGrayscale(m_grayscale);
            m_opaqueFaceRenderer.setTint(m_tint);
            m_opaqueFaceRenderer.setTintColor(m_tintColor);
            m_opaqueFaceRenderer.render(renderBatch);
        }

        void BrushRenderer::renderTransparentFaces(RenderBatch& renderBatch) {
            m_transparentFaceRenderer.setGrayscale(m_grayscale);
            m_transparentFaceRenderer.setTint(m_tint);
            m_transparentFaceRenderer.setTintColor(m_tintColor);
            m_transparentFaceRenderer.setAlpha(m_transparencyAlpha);
            m_transparentFaceRenderer.render(renderBatch);
        }

        void BrushRenderer::renderEdges(RenderBatch& renderBatch) {
            if (m_showOccludedEdges) {
                m_edgeRenderer.renderOnTop(renderBatch, m_occludedEdgeColor);
            }
            m_edgeRenderer.render(renderBatch, m_edgeColor);
        }

        class BrushRenderer::FilterWrapper : public BrushRenderer::Filter {
        private:
            const Filter& m_filter;
            bool m_showHiddenBrushes;
            NoFilter m_noFilter;

            const Filter& resolve() const {
                if (m_showHiddenBrushes) {
                    return m_noFilter;
                } else {
                    return m_filter;
                }
            }
        public:
            FilterWrapper(const Filter& filter, const bool showHiddenBrushes) :
            m_filter(filter),
            m_showHiddenBrushes(showHiddenBrushes) {}

            RenderSettings markFaces(const Model::Brush* brush) const override {
                return resolve().markFaces(brush);
            }
        };

        void BrushRenderer::validate() {
            assert(!valid());

            for (auto brush : m_invalidBrushes) {
                validateBrush(brush);
            }
            m_invalidBrushes.clear();
            assert(valid());

            m_opaqueFaceRenderer = FaceRenderer(m_vertexArray, m_opaqueFaces, m_faceColor);
            m_transparentFaceRenderer = FaceRenderer(m_vertexArray, m_transparentFaces, m_faceColor);
            m_edgeRenderer = IndexedEdgeRenderer(m_vertexArray, m_edgeIndices);
        }

        static size_t triIndicesCountForPolygon(const size_t vertexCount) {
            assert(vertexCount >= 3);
            const size_t indexCount = 3 * (vertexCount - 2);
            return indexCount;
        }

        static void addTriIndicesForPolygon(GLuint* dest, const GLuint baseIndex, const size_t vertexCount) {
            assert(vertexCount >= 3);
            for (size_t i = 0; i < vertexCount - 2; ++i) {
                *(dest++) = baseIndex;
                *(dest++) = baseIndex + static_cast<GLuint>(i + 1);
                *(dest++) = baseIndex + static_cast<GLuint>(i + 2);
            }
        }

        static inline bool shouldRenderEdge(const BrushRendererBrushCache::CachedEdge& edge,
                                            const BrushRenderer::Filter::EdgeRenderPolicy policy) {
            using EdgeRenderPolicy = BrushRenderer::Filter::EdgeRenderPolicy;

            switch (policy) {
                case EdgeRenderPolicy::RenderAll:
                    return true;
                case EdgeRenderPolicy::RenderIfEitherFaceMarked:
                    return (edge.face1 && edge.face1->isMarked()) || (edge.face2 && edge.face2->isMarked());
                case EdgeRenderPolicy::RenderIfBothFacesMarked:
                    return (edge.face1 && edge.face1->isMarked()) && (edge.face2 && edge.face2->isMarked());
                case EdgeRenderPolicy::RenderNone:
                    return false;
                switchDefault()
            }
        }

        static size_t countMarkedEdgeIndices(const Model::Brush* brush, const BrushRenderer::Filter::EdgeRenderPolicy policy) {
            using EdgeRenderPolicy = BrushRenderer::Filter::EdgeRenderPolicy;

            if (policy == EdgeRenderPolicy::RenderNone) {
                return 0;
            }

            size_t indexCount = 0;
            for (const auto& edge : brush->brushRendererBrushCache().cachedEdges()) {
                if (shouldRenderEdge(edge, policy)) {
                    indexCount += 2;
                }
            }
            return indexCount;
        }

        static void getMarkedEdgeIndices(const Model::Brush* brush,
                                         const BrushRenderer::Filter::EdgeRenderPolicy policy,
                                         const GLuint brushVerticesStartIndex,
                                         GLuint* dest) {
            using EdgeRenderPolicy = BrushRenderer::Filter::EdgeRenderPolicy;

            if (policy == EdgeRenderPolicy::RenderNone) {
                return;
            }

            size_t i = 0;
            for (const auto& edge : brush->brushRendererBrushCache().cachedEdges()) {
                if (shouldRenderEdge(edge, policy)) {
                    dest[i++] = static_cast<GLuint>(brushVerticesStartIndex + edge.vertexIndex1RelativeToBrush);
                    dest[i++] = static_cast<GLuint>(brushVerticesStartIndex + edge.vertexIndex2RelativeToBrush);
                }
            }
        }

        void BrushRenderer::validateBrush(const Model::Brush* brush) {
            assert(m_allBrushes.find(brush) != m_allBrushes.end());
            assert(m_invalidBrushes.find(brush) != m_invalidBrushes.end());
            assert(m_brushInfo.find(brush) == m_brushInfo.end());

            const FilterWrapper wrapper(*m_filter, m_showHiddenBrushes);

            // evaluate filter. only evaluate the filter once per brush.
            const auto settings = wrapper.markFaces(brush);
            const auto [facePolicy, edgePolicy] = settings;

            if (facePolicy == Filter::FaceRenderPolicy::RenderNone &&
                edgePolicy == Filter::EdgeRenderPolicy::RenderNone) {
                // NOTE: this skips inserting the brush into m_brushInfo
                return;
            }

            BrushInfo& info = m_brushInfo[brush];

            // collect vertices
            auto& brushCache = brush->brushRendererBrushCache();
            brushCache.validateVertexCache(brush);
            const auto& cachedVertices = brushCache.cachedVertices();
            ensure(!cachedVertices.empty(), "Brush must have cached vertices");

            assert(m_vertexArray != nullptr);
            auto [vertBlock, dest] = m_vertexArray->getPointerToInsertVerticesAt(cachedVertices.size());
            std::memcpy(dest, cachedVertices.data(), cachedVertices.size() * sizeof(*dest));
            info.vertexHolderKey = vertBlock;

            const auto brushVerticesStartIndex = static_cast<GLuint>(vertBlock->pos);

            // insert edge indices into VBO
            {
                const size_t edgeIndexCount = countMarkedEdgeIndices(brush, edgePolicy);
                if (edgeIndexCount > 0) {
                    auto[key, dest] = m_edgeIndices->getPointerToInsertElementsAt(edgeIndexCount);
                    info.edgeIndicesKey = key;
                    getMarkedEdgeIndices(brush, edgePolicy, brushVerticesStartIndex, dest);
                } else {
                    // it's possible to have no edges to render
                    // e.g. select all faces of a brush, and the unselected brush renderer
                    // will hit this branch.
                    ensure(info.edgeIndicesKey == nullptr, "BrushInfo not initialized");
                }
            }

            // insert face indices

            auto& facesSortedByTex = brushCache.cachedFacesSortedByTexture();
            const size_t facesSortedByTexSize = facesSortedByTex.size();
            const auto forceTransparent = m_transparent || brush->hasAttribute(Model::TagAttributes::Transparency);

            size_t nextI;
            for (size_t i = 0; i < facesSortedByTexSize; i = nextI) {
                const Assets::Texture* texture = facesSortedByTex[i].texture;

                size_t opaqueIndexCount = 0;
                size_t transparentIndexCount = 0;

                // find the i value for the next texture
                for (nextI = i + 1; nextI < facesSortedByTexSize && facesSortedByTex[nextI].texture == texture; ++nextI) {}

                // process all faces with this texture (they'll be consecutive)
                for (size_t j = i; j < nextI; ++j) {
                    const BrushRendererBrushCache::CachedFace& cache = facesSortedByTex[j];
                    if (cache.face->isMarked()) {
                        assert(cache.texture == texture);
                        if (forceTransparent || cache.face->hasAttribute(Model::TagAttributes::Transparency)) {
                            transparentIndexCount += triIndicesCountForPolygon(cache.vertexCount);
                        } else {
                            opaqueIndexCount += triIndicesCountForPolygon(cache.vertexCount);
                        }
                    }
                }

                if (transparentIndexCount > 0) {
                    TextureToBrushIndicesMap& faceVboMap = *m_transparentFaces;
                    auto& holderPtr = faceVboMap[texture];
                    if (holderPtr == nullptr) {
                        // inserts into map!
                        holderPtr = std::make_shared<BrushIndexArray>();
                    }

                    auto [key, dest] = holderPtr->getPointerToInsertElementsAt(transparentIndexCount);
                    info.transparentFaceIndicesKeys.push_back({texture, key});

                    // process all faces with this texture (they'll be consecutive)
                    GLuint *currentDest = dest;
                    for (size_t j = i; j < nextI; ++j) {
                        const BrushRendererBrushCache::CachedFace& cache = facesSortedByTex[j];
                        if (cache.face->isMarked() && (forceTransparent || cache.face->hasAttribute(Model::TagAttributes::Transparency))) {
                            addTriIndicesForPolygon(currentDest,
                                                    static_cast<GLuint>(brushVerticesStartIndex +
                                                                        cache.indexOfFirstVertexRelativeToBrush),
                                                    cache.vertexCount);

                            currentDest += triIndicesCountForPolygon(cache.vertexCount);
                        }
                    }
                    assert(currentDest == (dest + transparentIndexCount));
                }

                if (opaqueIndexCount > 0) {
                    TextureToBrushIndicesMap& faceVboMap = *m_opaqueFaces;
                    auto& holderPtr = faceVboMap[texture];
                    if (holderPtr == nullptr) {
                        // inserts into map!
                        holderPtr = std::make_shared<BrushIndexArray>();
                    }

                    auto [key, dest] = holderPtr->getPointerToInsertElementsAt(opaqueIndexCount);
                    info.opaqueFaceIndicesKeys.push_back({texture, key});

                    // process all faces with this texture (they'll be consecutive)
                    GLuint *currentDest = dest;
                    for (size_t j = i; j < nextI; ++j) {
                        const BrushRendererBrushCache::CachedFace& cache = facesSortedByTex[j];
                        if (cache.face->isMarked() && !(forceTransparent || cache.face->hasAttribute(Model::TagAttributes::Transparency))) {
                            addTriIndicesForPolygon(currentDest,
                                                    static_cast<GLuint>(brushVerticesStartIndex +
                                                                        cache.indexOfFirstVertexRelativeToBrush),
                                                    cache.vertexCount);

                            currentDest += triIndicesCountForPolygon(cache.vertexCount);
                        }
                    }
                    assert(currentDest == (dest + opaqueIndexCount));
                }
            }
        }

        void BrushRenderer::addBrush(const Model::Brush* brush) {
            // i.e. insert the brush as "invalid" if it's not already present.
            // if it is present, its validity is unchanged.
            if (m_allBrushes.find(brush) == m_allBrushes.end()) {
                assert(m_brushInfo.find(brush) == m_brushInfo.end());

                [[maybe_unused]] auto result = m_allBrushes.insert(brush);
                [[maybe_unused]] auto result2 = m_invalidBrushes.insert(brush);
            }
        }

        void BrushRenderer::removeBrush(const Model::Brush* brush) {
            // update m_brushValid
            {
                auto it = m_allBrushes.find(brush);
                assert(it != m_allBrushes.end());
                m_allBrushes.erase(it);
            }

            {
                auto it = m_invalidBrushes.find(brush);
                if (it != m_invalidBrushes.end()) {
                    m_invalidBrushes.erase(it);
                    // invalid brushes are not in the VBO, so we can return  now.
                    assert(m_brushInfo.find(brush) == m_brushInfo.end());
                    return;
                }
            }

            removeBrushFromVbo(brush);
        }

        void BrushRenderer::removeBrushFromVbo(const Model::Brush* brush) {
            auto it = m_brushInfo.find(brush);

            if (it == m_brushInfo.end()) {
                // This means BrushRenderer::validateBrush skipped rendering the brush, so it was never
                // uploaded to the VBO's
                return;
            }

            const BrushInfo& info = it->second;

            // update Vbo's
            m_vertexArray->deleteVerticesWithKey(info.vertexHolderKey);
            if (info.edgeIndicesKey != nullptr) {
                m_edgeIndices->zeroElementsWithKey(info.edgeIndicesKey);
            }

            for (const auto& [texture, opaqueKey] : info.opaqueFaceIndicesKeys) {
                std::shared_ptr<BrushIndexArray> faceIndexHolder = m_opaqueFaces->at(texture);
                faceIndexHolder->zeroElementsWithKey(opaqueKey);

                if (!faceIndexHolder->hasValidIndices()) {
                    // There are no indices left to render for this texture, so delete the <Texture, BrushIndexArray> entry from the map
                    m_opaqueFaces->erase(texture);
                }
            }
            for (const auto& [texture, transparentKey] : info.transparentFaceIndicesKeys) {
                std::shared_ptr<BrushIndexArray> faceIndexHolder = m_transparentFaces->at(texture);
                faceIndexHolder->zeroElementsWithKey(transparentKey);

                if (!faceIndexHolder->hasValidIndices()) {
                    // There are no indices left to render for this texture, so delete the <Texture, BrushIndexArray> entry from the map
                    m_transparentFaces->erase(texture);
                }
            }

            m_brushInfo.erase(it);
        }
    }
}
