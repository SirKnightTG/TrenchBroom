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

#include "Brush.h"

#include "CollectionUtils.h"
#include "Constants.h"
#include "Polyhedron_Matcher.h"
#include "Model/BrushFace.h"
#include "Model/BrushGeometry.h"
#include "Model/BrushSnapshot.h"
#include "Model/Entity.h"
#include "Model/FindContainerVisitor.h"
#include "Model/FindGroupVisitor.h"
#include "Model/FindLayerVisitor.h"
#include "Model/Group.h"
#include "Model/IssueGenerator.h"
#include "Model/NodeVisitor.h"
#include "Model/PickResult.h"
#include "Model/TagVisitor.h"
#include "Model/World.h"
#include "Renderer/BrushRendererBrushCache.h"

#include <vecmath/intersection.h>
#include <vecmath/vec.h>
#include <vecmath/vec_ext.h>
#include <vecmath/mat.h>
#include <vecmath/mat_ext.h>
#include <vecmath/ray.h>
#include <vecmath/segment.h>
#include <vecmath/polygon.h>
#include <vecmath/util.h>

#include <algorithm>
#include <iterator>

namespace TrenchBroom {
    namespace Model {
        const Hit::HitType Brush::BrushHit = Hit::freeHitType();

        BrushVertex*& Brush::ProjectToVertex::project(BrushVertex*& vertex) {
            return vertex;
        }

        BrushEdge*& Brush::ProjectToEdge::project(BrushEdge*& edge) {
            return edge;
        }

        class Brush::AddFaceToGeometryCallback : public BrushGeometry::Callback {
        private:
            BrushFace* m_addedFace;
        public:
            AddFaceToGeometryCallback(BrushFace* addedFace) :
            m_addedFace(addedFace) {
                ensure(m_addedFace != nullptr, "addedFace is null");
            }

            void faceWasCreated(BrushFaceGeometry* face) override {
                assert(m_addedFace != nullptr);
                m_addedFace->setGeometry(face);
                m_addedFace = nullptr;
            }

            void faceWasSplit(BrushFaceGeometry* original, BrushFaceGeometry* clone) override {
                auto* brushFace = original->payload();
                if (brushFace != nullptr) {
                    auto* brushFaceClone = brushFace->clone();
                    brushFaceClone->setGeometry(clone);
                }
            }

            void faceWillBeDeleted(BrushFaceGeometry* face) override {
                auto* brushFace = face->payload();
                if (brushFace != nullptr) {
                    brushFace->setGeometry(nullptr);
                    delete brushFace;
                }
            }
        };

        class Brush::HealEdgesCallback : public BrushGeometry::Callback {
        public:
            void facesWillBeMerged(BrushFaceGeometry* remainingGeometry, BrushFaceGeometry* geometryToDelete) override {
                auto* remainingFace = remainingGeometry->payload();
                if (remainingFace != nullptr) {
                    remainingFace->invalidate();
                }

                auto* faceToDelete = geometryToDelete->payload();
                if (faceToDelete != nullptr) {
                    faceToDelete->setGeometry(nullptr);
                    delete faceToDelete;
                }
            }

            void faceWillBeDeleted(BrushFaceGeometry* face) override {
                auto* brushFace = face->payload();
                if (brushFace != nullptr) {
                    brushFace->setGeometry(nullptr);
                    delete brushFace;
                }
            }
        };

        class Brush::AddFacesToGeometry {
        private:
            BrushGeometry& m_geometry;
            bool m_brushEmpty;
            bool m_brushValid;
        public:
            AddFacesToGeometry(BrushGeometry& geometry, BrushFaceList facesToAdd) :
            m_geometry(geometry),
            m_brushEmpty(false),
            m_brushValid(true) {
                // sort the faces by the weight of their plane normals like QBSP does
                Model::BrushFace::sortFaces(facesToAdd);

                for (auto it = std::begin(facesToAdd), end = std::end(facesToAdd); it != end && !m_brushEmpty; ++it) {
                    auto* brushFace = *it;
                    AddFaceToGeometryCallback addCallback(brushFace);
                    const auto result = m_geometry.clip(brushFace->boundary(), addCallback);
                    m_brushEmpty = result.empty();
                }
                if (!m_brushEmpty && m_brushValid) {
                    m_geometry.correctVertexPositions();

                    HealEdgesCallback healCallback;
                    m_brushValid = m_geometry.healEdges(healCallback);
                }
            }

            bool brushEmpty() const {
                return m_brushEmpty;
            }

            bool brushValid() const {
                return m_brushValid;
            }
        };

        class Brush::MoveVerticesCallback : public BrushGeometry::Callback {
        private:
            using IncidenceMap = std::map<vm::vec3, BrushFaceList>;
            IncidenceMap m_incidences;

            using BrushFaceGeometrySet = std::set<BrushFaceGeometry*>;

            BrushFaceGeometrySet m_addedGeometries;
            BrushFaceList m_removedFaces;
        public:
            template <typename I>
            MoveVerticesCallback(const BrushGeometry* geometry, I cur, I end, const vm::vec3& delta) {
                const auto vertices = Brush::createVertexSet(std::vector<vm::vec3>(cur, end));
                buildIncidences(geometry, vertices, delta);
            }

            MoveVerticesCallback(const BrushGeometry* geometry, const vm::vec3& vertex, const vm::vec3& delta) {
                VertexSet vertices = Brush::createVertexSet();
                vertices.insert(vertex);
                buildIncidences(geometry, vertices, delta);
            }

            MoveVerticesCallback(const BrushGeometry* geometry) {
                buildIncidences(geometry, Brush::createVertexSet(), vm::vec3::zero());
            }

            ~MoveVerticesCallback() override {
                VectorUtils::clearAndDelete(m_removedFaces);
            }
        private:
            void buildIncidences(const BrushGeometry* geometry, const VertexSet& verticesToBeMoved, const vm::vec3& delta) {
                const auto& vertices = geometry->vertices();
                const auto* firstVertex = vertices.front();
                const auto* curVertex = firstVertex;
                do {
                    const auto& position = curVertex->position();
                    if (verticesToBeMoved.count(position)) {
                        m_incidences.insert(std::make_pair(position + delta, collectIncidentFaces(curVertex)));
                    } else {
                        m_incidences.insert(std::make_pair(position, collectIncidentFaces(curVertex)));
                    }
                    curVertex = curVertex->next();
                } while (curVertex != firstVertex);
            }

            BrushFaceList collectIncidentFaces(const BrushVertex* vertex) {
                BrushFaceList result;
                auto* firstEdge = vertex->leaving();
                auto* curEdge = firstEdge;
                do {
                    result.push_back(curEdge->face()->payload());
                    curEdge = curEdge->nextIncident();
                } while (curEdge != firstEdge);
                return result;
            }
        public:
            void vertexWasAdded(BrushVertex* vertex) override {}

            void vertexWillBeRemoved(BrushVertex* vertex) override {}

            void faceWasCreated(BrushFaceGeometry* faceGeometry) override {
                m_addedGeometries.insert(faceGeometry);
            }

            void faceWillBeDeleted(BrushFaceGeometry* faceGeometry) override {
                if (m_addedGeometries.erase(faceGeometry) == 0) {
                    auto* face = faceGeometry->payload();
                    ensure(face != nullptr, "face is null");

                    m_removedFaces.push_back(face);
                    face->setGeometry(nullptr);
                }
            }

            void faceDidChange(BrushFaceGeometry* faceGeometry) override {
                ensure(false, "faceDidChange called");
            }

            void faceWasFlipped(BrushFaceGeometry* faceGeometry) override {
                auto* face = faceGeometry->payload();
                if (face != nullptr) {
                    face->invert();
                }
            }

            void faceWasSplit(BrushFaceGeometry* originalGeometry, BrushFaceGeometry* cloneGeometry) override {
                ensure(false, "faceWasSplit called");
            }

            void facesWillBeMerged(BrushFaceGeometry* remainingGeometry, BrushFaceGeometry* geometryToDelete) override {
                ensure(false, "facesWillBeMerged called");
            }
        private:
            using SharedIncidentFaceCounts = std::map<BrushFace*, size_t>;

            BrushFace* findMatchingFace(BrushFaceGeometry* geometry) const {
                const auto counts = findSharedIncidentFaces(geometry);
                ensure(!counts.empty(), "empty shared incident face counts");

                size_t bestCount = 0;
                BrushFace* bestFace = nullptr;

                for (const auto& entry : counts) {
                    auto* face = entry.first;
                    const auto count = entry.second;
                    if (count > bestCount) {
                        bestFace = face;
                        bestCount = count;
                    } else if (count == bestCount && face->geometry() == nullptr) {
                        bestFace = face;
                    }
                }

                ensure(bestFace != nullptr, "bestFace is null");
                return bestFace;
            }

            SharedIncidentFaceCounts findSharedIncidentFaces(BrushFaceGeometry* geometry) const {
                SharedIncidentFaceCounts result;

                for (const auto* curEdge : geometry->boundary()) {
                    const auto* origin = curEdge->origin();
                    const auto iIt = m_incidences.find(origin->position());
                    if (iIt != std::end(m_incidences)) {
                        const auto& incidentFaces = iIt->second;
                        for (auto* curFace : incidentFaces) {
                            auto qIt = result.find(curFace);
                            if (qIt == std::end(result))
                                result.insert(qIt, std::make_pair(curFace, 1));
                            else
                                ++qIt->second;
                        }
                    }
                }

                return result;
            }
        };

        class Brush::QueryCallback : public BrushGeometry::Callback {
        public:
            vm::plane3 getPlane(const BrushFaceGeometry* face) const override {
                return face->payload()->boundary();
            }
        };

        Brush::Brush(const vm::bbox3& worldBounds, const BrushFaceList& faces) :
        m_geometry(nullptr),
        m_transparent(false),
        m_brushRendererBrushCache(std::make_unique<Renderer::BrushRendererBrushCache>()) {
            addFaces(faces);
            try {
                buildGeometry(worldBounds);
            } catch (const GeometryException&) {
                cleanup();
                throw;
            }
        }

        Brush::~Brush() {
            cleanup();
        }

        void Brush::cleanup() {
            deleteGeometry();
            VectorUtils::clearAndDelete(m_faces);
        }

        Brush* Brush::clone(const vm::bbox3& worldBounds) const {
            return static_cast<Brush*>(Node::clone(worldBounds));
        }

        NodeSnapshot* Brush::doTakeSnapshot() {
            return new BrushSnapshot(this);
        }

        class FindBrushOwner : public NodeVisitor, public NodeQuery<AttributableNode*> {
        private:
            void doVisit(World* world) override   { setResult(world); cancel(); }
            void doVisit(Layer* layer) override   {}
            void doVisit(Group* group) override   {}
            void doVisit(Entity* entity) override { setResult(entity); cancel(); }
            void doVisit(Brush* brush) override   {}
        };

        AttributableNode* Brush::entity() const {
            if (parent() == nullptr) {
                return nullptr;
            }

            FindBrushOwner visitor;
            parent()->acceptAndEscalate(visitor);
            if (!visitor.hasResult()) {
                return nullptr;
            } else {
                return visitor.result();
            }
        }

        BrushFace* Brush::findFace(const String& textureName) const {
            for (BrushFace* face : m_faces) {
                if (face->textureName() == textureName) {
                    return face;
                }
            }
            return nullptr;
        }

        BrushFace* Brush::findFace(const vm::vec3& normal) const {
            for (auto* face : m_faces) {
                if (vm::is_equal(face->boundary().normal, normal, vm::C::almost_zero())) {
                    return face;
                }
            }
            return nullptr;
        }

        BrushFace* Brush::findFace(const vm::plane3& boundary) const {
            for (auto* face : m_faces) {
                if (vm::is_equal(face->boundary(), boundary, vm::C::almost_zero())) {
                    return face;
                }
            }
            return nullptr;
        }

        BrushFace* Brush::findFace(const vm::polygon3& vertices, const FloatType epsilon) const {
            for (auto* face : m_faces) {
                if (face->hasVertices(vertices, epsilon)) {
                    return face;
                }
            }
            return nullptr;
        }

        BrushFace* Brush::findFace(const std::vector<vm::polygon3>& candidates, const FloatType epsilon) const {
            for (const auto& candidate : candidates) {
                auto* face = findFace(candidate, epsilon);
                if (face != nullptr) {
                    return face;
                }
            }
            return nullptr;
        }

        size_t Brush::faceCount() const {
            return m_faces.size();
        }

        const BrushFaceList& Brush::faces() const {
            return m_faces;
        }

        void Brush::setFaces(const vm::bbox3& worldBounds, const BrushFaceList& faces) {
            const NotifyNodeChange nodeChange(this);

            const vm::bbox3 oldBounds = physicalBounds();
            deleteGeometry();

            detachFaces(m_faces);
            VectorUtils::clearAndDelete(m_faces);
            addFaces(faces);

            buildGeometry(worldBounds);
            nodePhysicalBoundsDidChange(oldBounds);
        }

        bool Brush::closed() const {
            ensure(m_geometry != nullptr, "geometry is null");
            return m_geometry->closed();
        }

        bool Brush::fullySpecified() const {
            ensure(m_geometry != nullptr, "geometry is null");

            for (auto* current : m_geometry->faces()) {
                if (current->payload() == nullptr) {
                    return false;
                }
            }
            return true;
        }

        void Brush::faceDidChange() {
            invalidateIssues();
        }

        void Brush::addFaces(const BrushFaceList& faces) {
            addFaces(std::begin(faces), std::end(faces), faces.size());
        }

        void Brush::addFace(BrushFace* face) {
            ensure(face != nullptr, "face is null");
            ensure(face->brush() == nullptr, "face brush is null");
            assert(!VectorUtils::contains(m_faces, face));

            m_faces.push_back(face);
            face->setBrush(this);
            invalidateVertexCache();
            if (face->selected()) {
                incChildSelectionCount(1);
            }
        }

        void Brush::removeFace(BrushFace* face) {
            m_faces.erase(doRemoveFace(std::begin(m_faces), std::end(m_faces), face), std::end(m_faces));
        }

        BrushFaceList::iterator Brush::doRemoveFace(BrushFaceList::iterator begin, BrushFaceList::iterator end, BrushFace* face) {
            ensure(face != nullptr, "face is null");

            BrushFaceList::iterator it = std::remove(begin, end, face);
            ensure(it != std::end(m_faces), "face to remove not found");
            detachFace(face);
            return it;
        }

        void Brush::detachFaces(const BrushFaceList& faces) {
            for (auto* face : faces) {
                detachFace(face);
            }
        }

        void Brush::detachFace(BrushFace* face) {
            ensure(face != nullptr, "face is null");
            ensure(face->brush() == this, "invalid face brush");

            if (face->selected()) {
                decChildSelectionCount(1);
            }
            face->setGeometry(nullptr);
            face->setBrush(nullptr);
            invalidateVertexCache();
        }

        void Brush::cloneFaceAttributesFrom(const BrushList& brushes) {
            for (const auto* brush : brushes) {
                cloneFaceAttributesFrom(brush);
            }
        }

        void Brush::cloneFaceAttributesFrom(const Brush* brush) {
            for (auto* destination : m_faces) {
                const auto* source = brush->findFace(destination->boundary());
                if (source != nullptr) {
                    destination->setAttribs(source->attribs());

                    auto snapshot = source->takeTexCoordSystemSnapshot();
                    if (snapshot != nullptr) {
                        destination->copyTexCoordSystemFromFace(*snapshot, source->attribs().takeSnapshot(), source->boundary(), WrapStyle::Projection);
                    }
                }
            }
        }

        void Brush::cloneInvertedFaceAttributesFrom(const BrushList& brushes) {
            for (const auto* brush : brushes) {
                cloneInvertedFaceAttributesFrom(brush);
            }
        }

        void Brush::cloneInvertedFaceAttributesFrom(const Brush* brush) {
            for (auto* destination : m_faces) {
                const auto* source = brush->findFace(destination->boundary().flip());
                if (source != nullptr) {
                    // Todo: invert the face attributes?
                    destination->setAttribs(source->attribs());

                    auto snapshot = source->takeTexCoordSystemSnapshot();
                    if (snapshot != nullptr) {
                        destination->copyTexCoordSystemFromFace(*snapshot, source->attribs().takeSnapshot(), destination->boundary(), WrapStyle::Projection);
                    }
                }
            }
        }

        bool Brush::clip(const vm::bbox3& worldBounds, BrushFace* face) {
            const NotifyNodeChange nodeChange(this);
            try {
                addFace(face);
                rebuildGeometry(worldBounds);
                return !m_faces.empty();
            } catch (GeometryException&) {
                return false;
            }
        }

        bool Brush::canMoveBoundary(const vm::bbox3& worldBounds, const BrushFace* face, const vm::vec3& delta) const {
            auto* testFace = face->clone();
            testFace->transform(vm::translation_matrix(delta), false);

            BrushFaceList testFaces;
            testFaces.push_back(testFace);

            for (auto* brushFace : m_faces) {
                if (brushFace != face) {
                    testFaces.push_back(brushFace->clone());
                }
            }

            try {
                const auto testBrush = Brush(worldBounds, testFaces);
                const auto inWorldBounds = worldBounds.contains(testBrush.logicalBounds());
                const auto closed = testBrush.closed();
                const auto allFaces = testBrush.faceCount() == testFaces.size();

                return inWorldBounds && closed && allFaces;
            } catch (const GeometryException&) {
                return false;
            }
        }

        void Brush::moveBoundary(const vm::bbox3& worldBounds, BrushFace* face, const vm::vec3& delta, const bool lockTexture) {
            assert(canMoveBoundary(worldBounds, face, delta));

            const NotifyNodeChange nodeChange(this);
            face->transform(vm::translation_matrix(delta), lockTexture);
            rebuildGeometry(worldBounds);
        }

        bool Brush::canExpand(const vm::bbox3& worldBounds, const FloatType delta, const bool lockTexture) const {
            Brush *testBrush = clone(worldBounds);
            const bool didExpand = testBrush->expand(worldBounds, delta, lockTexture);
            delete testBrush;

            return didExpand;
        }

        bool Brush::expand(const vm::bbox3& worldBounds, const FloatType delta, const bool lockTexture) {
            const NotifyNodeChange nodeChange(this);

            // move the faces
            for (BrushFace* face : m_faces) {
                const vm::vec3 moveAmount = face->boundary().normal * delta;
                face->transform(vm::translation_matrix(moveAmount), lockTexture);
            }

            // rebuild geometry
            try {
                rebuildGeometry(worldBounds);
                return !m_faces.empty();
            } catch (GeometryException&) {
                return false;
            }
        }

        size_t Brush::vertexCount() const {
            ensure(m_geometry != nullptr, "geometry is null");
            return m_geometry->vertexCount();
        }

        Brush::VertexList Brush::vertices() const {
            ensure(m_geometry != nullptr, "geometry is null");
            return VertexList(m_geometry->vertices());
        }

        const std::vector<vm::vec3> Brush::vertexPositions() const {
            ensure(m_geometry != nullptr, "geometry is null");
            return m_geometry->vertexPositions();
        }

        bool Brush::hasVertex(const vm::vec3& position, const FloatType epsilon) const {
            ensure(m_geometry != nullptr, "geometry is null");
            return m_geometry->findVertexByPosition(position, nullptr, epsilon) != nullptr;
        }

        bool Brush::hasVertices(const std::vector<vm::vec3>& positions, const FloatType epsilon) const {
            ensure(m_geometry != nullptr, "geometry is null");
            for (const auto& position : positions) {
                if (!m_geometry->hasVertex(position, epsilon)) {
                    return false;
                }
            }
            return true;
        }

        vm::vec3 Brush::findClosestVertexPosition(const vm::vec3& position) const {
            ensure(m_geometry != nullptr, "geometry is null");
            return m_geometry->findClosestVertex(position)->position();
        }

        bool Brush::hasEdge(const vm::segment3& edge, const FloatType epsilon) const {
            ensure(m_geometry != nullptr, "geometry is null");
            return m_geometry->findEdgeByPositions(edge.start(), edge.end(), epsilon) != nullptr;
        }

        bool Brush::hasEdges(const std::vector<vm::segment3>& edges, const FloatType epsilon) const {
            ensure(m_geometry != nullptr, "geometry is null");
            for (const auto& edge : edges) {
                if (!m_geometry->hasEdge(edge.start(), edge.end(), epsilon)) {
                    return false;
                }
            }
            return true;
        }

        bool Brush::hasFace(const vm::polygon3& face, const FloatType epsilon) const {
            ensure(m_geometry != nullptr, "geometry is null");
            return m_geometry->hasFace(face.vertices(), epsilon);
        }

        bool Brush::hasFaces(const std::vector<vm::polygon3>& faces, const FloatType epsilon) const {
            ensure(m_geometry != nullptr, "geometry is null");
            for (const auto& face : faces) {
                if (!m_geometry->hasFace(face.vertices(), epsilon)) {
                    return false;
                }
            }
            return true;
        }

        bool Brush::hasFace(const vm::vec3& p1, const vm::vec3& p2, const vm::vec3& p3, const FloatType epsilon) const {
            return hasFace(vm::polygon3(VectorUtils::create<vm::vec3>(p1, p2, p3)), epsilon);
        }

        bool Brush::hasFace(const vm::vec3& p1, const vm::vec3& p2, const vm::vec3& p3, const vm::vec3& p4, const FloatType epsilon) const {
            return hasFace(vm::polygon3(VectorUtils::create<vm::vec3>(p1, p2, p3, p4)), epsilon);
        }

        bool Brush::hasFace(const vm::vec3& p1, const vm::vec3& p2, const vm::vec3& p3, const vm::vec3& p4, const vm::vec3& p5, const FloatType epsilon) const {
            return hasFace(vm::polygon3(VectorUtils::create<vm::vec3>(p1, p2, p3, p4, p5)), epsilon);
        }


        size_t Brush::edgeCount() const {
            ensure(m_geometry != nullptr, "geometry is null");
            return m_geometry->edgeCount();
        }

        Brush::EdgeList Brush::edges() const {
            ensure(m_geometry != nullptr, "geometry is null");
            return EdgeList(m_geometry->edges());
        }

        bool Brush::containsPoint(const vm::vec3& point) const {
            if (!logicalBounds().contains(point)) {
                return false;
            } else {
                for (const auto* face : m_faces) {
                    if (face->boundary().point_status(point) == vm::plane_status::above) {
                        return false;
                    }
                }
                return true;
            }
        }

        BrushFaceList Brush::incidentFaces(const BrushVertex* vertex) const {
            BrushFaceList result;
            result.reserve(m_faces.size());

            auto* first = vertex->leaving();
            auto* current = first;
            do {
                result.push_back(current->face()->payload());
                current = current->nextIncident();
            } while (current != first);

            return result;
        }

        bool Brush::canMoveVertices(const vm::bbox3& worldBounds, const std::vector<vm::vec3>& vertices, const vm::vec3& delta) const {
            return doCanMoveVertices(worldBounds, vertices, delta, true).success;
        }

        std::vector<vm::vec3> Brush::moveVertices(const vm::bbox3& worldBounds, const std::vector<vm::vec3>& vertexPositions, const vm::vec3& delta, const bool uvLock) {
            doMoveVertices(worldBounds, vertexPositions, delta, uvLock);

            // Collect the exact new positions of the moved vertices
            std::vector<vm::vec3> result;
            result.reserve(vertexPositions.size());

            for (const auto& position : vertexPositions) {
                const auto* newVertex = m_geometry->findClosestVertex(position + delta, vm::C::almost_zero());
                if (newVertex != nullptr) {
                    result.push_back(newVertex->position());
                }
            }

            return result;
        }

        bool Brush::canAddVertex(const vm::bbox3& worldBounds, const vm::vec3& position) const {
            ensure(m_geometry != nullptr, "geometry is null");
            return worldBounds.contains(position) && !m_geometry->contains(position);
        }

        BrushVertex* Brush::addVertex(const vm::bbox3& worldBounds, const vm::vec3& position) {
            assert(canAddVertex(worldBounds, position));

            BrushGeometry newGeometry(*m_geometry);
            newGeometry.addPoint(position);

            const PolyhedronMatcher<BrushGeometry> matcher(*m_geometry, newGeometry);
            doSetNewGeometry(worldBounds, matcher, newGeometry);

            auto* newVertex = m_geometry->findClosestVertex(position, vm::C::almost_zero());
            ensure(newVertex != nullptr, "vertex could not be added");
            return newVertex;
        }


        bool Brush::canRemoveVertices(const vm::bbox3& worldBounds, const std::vector<vm::vec3>& vertexPositions) const {
            ensure(m_geometry != nullptr, "geometry is null");
            ensure(!vertexPositions.empty(), "no vertex positions");

            BrushGeometry testGeometry;
            const auto vertexSet = Brush::createVertexSet(vertexPositions);

            for (const auto* vertex : m_geometry->vertices()) {
                const auto& position = vertex->position();
                if (!vertexSet.count(position)) {
                    testGeometry.addPoint(position);
                }
            }

            return testGeometry.polyhedron();
        }

        void Brush::removeVertices(const vm::bbox3& worldBounds, const std::vector<vm::vec3>& vertexPositions) {
            ensure(m_geometry != nullptr, "geometry is null");
            ensure(!vertexPositions.empty(), "no vertex positions");
            assert(canRemoveVertices(worldBounds, vertexPositions));

            BrushGeometry newGeometry;
            const auto vertexSet = Brush::createVertexSet(vertexPositions);

            for (const auto* vertex : m_geometry->vertices()) {
                const auto& position = vertex->position();
                if (!vertexSet.count(position)) {
                    newGeometry.addPoint(position);
                }
            }

            const PolyhedronMatcher<BrushGeometry> matcher(*m_geometry, newGeometry);
            doSetNewGeometry(worldBounds, matcher, newGeometry);
        }

        bool Brush::canSnapVertices(const vm::bbox3& worldBounds, const FloatType snapToF) {
            BrushGeometry newGeometry;

            for (const auto* vertex : m_geometry->vertices()) {
                const auto& origin = vertex->position();
                const auto destination = snapToF * round(origin / snapToF);
                newGeometry.addPoint(destination);
            }

            return newGeometry.polyhedron();
        }

        void Brush::snapVertices(const vm::bbox3& worldBounds, const FloatType snapToF, const bool uvLock) {
            ensure(m_geometry != nullptr, "geometry is null");

            BrushGeometry newGeometry;

            for (const auto* vertex : m_geometry->vertices()) {
                const auto& origin = vertex->position();
                const auto destination = snapToF * round(origin / snapToF);
                newGeometry.addPoint(destination);
            }

            using VecMap = std::map<vm::vec3,vm::vec3>;
            VecMap vertexMapping;
            for (const auto* vertex : m_geometry->vertices()) {
                const auto& origin = vertex->position();
                const auto destination = snapToF * round(origin / snapToF);
                if (newGeometry.hasVertex(destination)) {
                    vertexMapping.insert(std::make_pair(origin, destination));
                }
            }

            const PolyhedronMatcher<BrushGeometry> matcher(*m_geometry, newGeometry, vertexMapping);
            doSetNewGeometry(worldBounds, matcher, newGeometry, uvLock);
        }

        bool Brush::canMoveEdges(const vm::bbox3& worldBounds, const std::vector<vm::segment3>& edgePositions, const vm::vec3& delta) const {
            ensure(m_geometry != nullptr, "geometry is null");
            ensure(!edgePositions.empty(), "no edge positions");

            std::vector<vm::vec3> vertexPositions;
            vm::segment3::get_vertices(
                std::begin(edgePositions), std::end(edgePositions),
                std::back_inserter(vertexPositions));
            const auto result = doCanMoveVertices(worldBounds, vertexPositions, delta, false);

            if (!result.success) {
                return false;
            }

            for (const auto& edge : edgePositions) {
                if (!result.geometry.hasEdge(edge.start() + delta, edge.end() + delta)) {
                    return false;
                }
            }

            return true;
        }

        std::vector<vm::segment3> Brush::moveEdges(const vm::bbox3& worldBounds, const std::vector<vm::segment3>& edgePositions, const vm::vec3& delta, const bool uvLock) {
            assert(canMoveEdges(worldBounds, edgePositions, delta));

            std::vector<vm::vec3> vertexPositions;
            vm::segment3::get_vertices(std::begin(edgePositions), std::end(edgePositions),
                                  std::back_inserter(vertexPositions));
            doMoveVertices(worldBounds, vertexPositions, delta, uvLock);

            std::vector<vm::segment3> result;
            result.reserve(edgePositions.size());

            for (const auto& edgePosition : edgePositions) {
                const auto* newEdge = m_geometry->findClosestEdge(edgePosition.start() + delta, edgePosition.end() + delta,
                    vm::C::almost_zero());
                if (newEdge != nullptr) {
                    result.push_back(vm::segment3(newEdge->firstVertex()->position(), newEdge->secondVertex()->position()));
                }
            }

            return result;
        }

        bool Brush::canMoveFaces(const vm::bbox3& worldBounds, const std::vector<vm::polygon3>& facePositions, const vm::vec3& delta) const {
            ensure(m_geometry != nullptr, "geometry is null");
            ensure(!facePositions.empty(), "no face positions");

            std::vector<vm::vec3> vertexPositions;
            vm::polygon3::get_vertices(std::begin(facePositions), std::end(facePositions), std::back_inserter(vertexPositions));
            const auto result = doCanMoveVertices(worldBounds, vertexPositions, delta, false);

            if (!result.success) {
                return false;
            }

            for (const auto& face : facePositions) {
                if (!result.geometry.hasFace(face.vertices() + delta)) {
                    return false;
                }
            }

            return true;
        }

        std::vector<vm::polygon3> Brush::moveFaces(const vm::bbox3& worldBounds, const std::vector<vm::polygon3>& facePositions, const vm::vec3& delta, const bool uvLock) {
            assert(canMoveFaces(worldBounds, facePositions, delta));

            std::vector<vm::vec3> vertexPositions;
            vm::polygon3::get_vertices(std::begin(facePositions), std::end(facePositions), std::back_inserter(vertexPositions));
            doMoveVertices(worldBounds, vertexPositions, delta, uvLock);

            std::vector<vm::polygon3> result;
            result.reserve(facePositions.size());

            for (const auto& facePosition : facePositions) {
                const auto* newFace = m_geometry->findClosestFace(facePosition.vertices() + delta, vm::C::almost_zero());
                if (newFace != nullptr) {
                    result.push_back(vm::polygon3(newFace->vertexPositions()));
                }
            }

            return result;
        }

        Brush::CanMoveVerticesResult::CanMoveVerticesResult(const bool s, const BrushGeometry& g) : success(s), geometry(g) {}

        Brush::CanMoveVerticesResult Brush::CanMoveVerticesResult::rejectVertexMove() {
            return CanMoveVerticesResult(false, BrushGeometry());
        }

        Brush::CanMoveVerticesResult Brush::CanMoveVerticesResult::acceptVertexMove(const BrushGeometry& result) {
            return CanMoveVerticesResult(true, result);
        }

        /*
         We determine whether a move is valid by considering the vertices being moved and the vertices
         remaining at their positions as polyhedra. Depending on whether or not they really are polyhedra,
         polygons, edges, points, or empty, we have to consider the following cases.

         REMAINING  || Empty   | Point  | Edge   | Polygon | Polyhedron
         ===========||=========|========|========|=========|============
         MOVING     ||         |        |        |         |
         -----------||---------|--------|--------|---------|------------
         Empty      || n/a     | n/a    | n/a    | n/a     | no
         -----------||---------|--------|--------|---------|------------
         Point      || n/a     | n/a    | n/a    | ok      | check
         -----------||---------|--------|--------|---------|------------
         Edge       || n/a     | n/a    | ok     | check   | check
         -----------||---------|--------|--------|---------|------------
         Polygon    || n/a     | invert | invert | check   | check
         -----------||---------|--------|--------|---------|------------
         Polyhedron || ok      | invert | invert | invert  | check

         n/a    - This case can never occur.
         ok     - This case is always allowed, unless the brush becomes invalid, i.e., not a polyhedron.
         no     - This case is always forbidden.
         invert - This case is handled by swapping the remaining and the moving fragments and inverting the delta. This takes us from a cell at (column, row) to the cell at (row, column).
         check  - Check whether any of the moved vertices would travel through the remaining fragment, or vice versa if inverted case. Also check whether the brush would become invalid, i.e., not a polyhedron.

         If `allowVertexRemoval` is true, vertices can be moved inside a remaining polyhedron.

         */
        Brush::CanMoveVerticesResult Brush::doCanMoveVertices(const vm::bbox3& worldBounds, const std::vector<vm::vec3>& vertexPositions, vm::vec3 delta, const bool allowVertexRemoval) const {
            // Should never occur, takes care of the first row.
            if (vertexPositions.empty() || vm::is_zero(delta, vm::C::almost_zero())) {
                return CanMoveVerticesResult::rejectVertexMove();
            }

            const auto vertexSet = Brush::createVertexSet(vertexPositions);

            BrushGeometry remaining;
            BrushGeometry moving;
            BrushGeometry result;
            for (const auto* vertex : m_geometry->vertices()) {
                const auto& position = vertex->position();
                if (!vertexSet.count(position)) {
                    // the vertex is not moving
                    remaining.addPoint(position);
                    result.addPoint(position);
                } else {
                    // the vertex is moving
                    moving.addPoint(position);
                    result.addPoint(position + delta);
                }
            }

            // Will the result go out of world bounds?
            if (!worldBounds.contains(result.bounds())) {
                return CanMoveVerticesResult::rejectVertexMove();
            }

            // Special case, takes care of the first column.
            if (moving.vertexCount() == vertexCount()) {
                return CanMoveVerticesResult::acceptVertexMove(result);
            }

            // Will vertices be removed?
            if (!allowVertexRemoval) {
                // All moving vertices must still be present in the result
                for (const auto& movingVertex : moving.vertexPositions()) {
                    if (!result.hasVertex(movingVertex + delta)) {
                        return CanMoveVerticesResult::rejectVertexMove();
                    }
                }
            }

            // Will the brush become invalid?
            if (!result.polyhedron()) {
                return CanMoveVerticesResult::rejectVertexMove();
            }

            // One of the remaining two ok cases?
            if ((moving.point() && remaining.polygon()) ||
                (moving.edge() && remaining.edge())) {
                return CanMoveVerticesResult::acceptVertexMove(result);
            }

            // Invert if necessary.
            if (remaining.point() || remaining.edge() || (remaining.polygon() && moving.polyhedron())) {
                using std::swap;
                swap(remaining, moving);
                delta = -delta;
            }

            // Now check if any of the moving vertices would travel through the remaining fragment and out the other side.
            for (const auto* vertex : moving.vertices()) {
                const auto& oldPos = vertex->position();
                const auto newPos = oldPos + delta;

                for (const auto* face : remaining.faces()) {
                    if (face->pointStatus(oldPos) == vm::plane_status::below &&
                        face->pointStatus(newPos) == vm::plane_status::above) {
                        const auto ray = vm::ray3(oldPos, normalize(newPos - oldPos));
                        const auto distance = face->intersectWithRay(ray, vm::side::back);
                        if (!vm::is_nan(distance)) {
                            return CanMoveVerticesResult::rejectVertexMove();
                        }
                    }
                }
            }

            return CanMoveVerticesResult::acceptVertexMove(result);
        }

        void Brush::doMoveVertices(const vm::bbox3& worldBounds, const std::vector<vm::vec3>& vertexPositions, const vm::vec3& delta, const bool uvLock) {
            ensure(m_geometry != nullptr, "geometry is null");
            ensure(!vertexPositions.empty(), "no vertex positions");
            assert(canMoveVertices(worldBounds, vertexPositions, delta));

            BrushGeometry newGeometry;
            const auto vertexSet = Brush::createVertexSet(vertexPositions);

            for (auto* vertex : m_geometry->vertices()) {
                const auto& position = vertex->position();
                if (vertexSet.count(position)) {
                    newGeometry.addPoint(position + delta);
                } else {
                    newGeometry.addPoint(position);
                }
            }

            using VecMap = std::map<vm::vec3, vm::vec3>;
            VecMap vertexMapping;
            for (auto* oldVertex : m_geometry->vertices()) {
                const auto& oldPosition = oldVertex->position();
                const auto moved = vertexSet.count(oldPosition);
                const auto newPosition = moved ? oldPosition + delta : oldPosition;
                const auto* newVertex = newGeometry.findClosestVertex(newPosition, vm::C::almost_zero());
                if (newVertex != nullptr) {
                    vertexMapping.insert(std::make_pair(oldPosition, newVertex->position()));
                }
            }

            const PolyhedronMatcher<BrushGeometry> matcher(*m_geometry, newGeometry, vertexMapping);
            doSetNewGeometry(worldBounds, matcher, newGeometry, uvLock);
        }

        std::tuple<bool, vm::mat4x4> Brush::findTransformForUVLock(const PolyhedronMatcher<BrushGeometry>& matcher, BrushFaceGeometry* left, BrushFaceGeometry* right) {
            std::vector<vm::vec3> unmovedVerts;
            std::vector<std::pair<vm::vec3, vm::vec3>> movedVerts;

            matcher.visitMatchingVertexPairs(left, right, [&](BrushVertex* leftVertex, BrushVertex* rightVertex){
                const auto leftPosition = leftVertex->position();
                const auto rightPosition = rightVertex->position();

                if (vm::is_equal(leftPosition, rightPosition, vm::constants<FloatType>::almost_zero())) {
                    unmovedVerts.push_back(leftPosition);
                } else {
                    movedVerts.emplace_back(leftPosition, rightPosition);
                }
            });

            // If 3 or more are unmoving, give up.
            // (Picture a square with one corner being moved, we can't possibly lock the UV's of all 4 corners.)
            if (unmovedVerts.size() >= 3) {
                return std::make_tuple(false, vm::mat4x4());
            }

            std::vector<std::pair<vm::vec3, vm::vec3>> referenceVerts;

            // Use unmoving, then moving
            for (const auto& unmovedVert : unmovedVerts) {
                referenceVerts.emplace_back(unmovedVert, unmovedVert);
            }
            // TODO: When there are multiple choices of moving verts (unmovedVerts.size() + movedVerts.size() > 3)
            // we should sort them somehow. This can be seen if you select and move 3/5 verts of a pentagon;
            // which of the 3 moving verts currently gets UV lock is arbitrary.
            VectorUtils::append(referenceVerts, movedVerts);

            if (referenceVerts.size() < 3) {
                // Can't create a transform as there are not enough verts
                return std::make_tuple(false, vm::mat4x4());
            }

            const auto M = vm::points_transformation_matrix(
                    referenceVerts[0].first, referenceVerts[1].first, referenceVerts[2].first,
                    referenceVerts[0].second, referenceVerts[1].second, referenceVerts[2].second);

            if (!(M == M)) {
                // Transform contains nan
                return std::make_tuple(false, vm::mat4x4());
            }

            return std::make_tuple(true, M);
        }

        void Brush::applyUVLock(const PolyhedronMatcher<BrushGeometry>& matcher, BrushFaceGeometry* left, BrushFaceGeometry* right) {
            const auto [success, M] = findTransformForUVLock(matcher, left, right);
            if (!success) {
                return;
            }

            auto* leftFace = left->payload();
            auto* rightFace = right->payload();

            // We want to re-set the texturing of `rightFace` using the texturing from M * leftFace.
            // We don't want to disturb the actual geometry of `rightFace` which is already finalized.
            // So the idea is, clone `leftFace`, transform it by M using texture lock, then copy the texture
            // settings from the transformed clone (which should have an identical plane to `rightFace` within
            // FP error) to `rightFace`.
            auto leftClone = std::unique_ptr<BrushFace>(leftFace->clone());

            try {
                leftClone->transform(M, true);

                auto snapshot = std::unique_ptr<TexCoordSystemSnapshot>(leftClone->takeTexCoordSystemSnapshot());
                rightFace->setAttribs(leftClone->attribs());
                if (snapshot) {
                    // Note, the wrap style doesn't matter because the source and destination faces should have the same plane
                    rightFace->copyTexCoordSystemFromFace(*snapshot, leftClone->attribs().takeSnapshot(),
                                                          leftClone->boundary(), WrapStyle::Rotation);
                }
                rightFace->resetTexCoordSystemCache();
            } catch (const GeometryException&) {
                // do nothing
            }
        }

        void Brush::doSetNewGeometry(const vm::bbox3& worldBounds, const PolyhedronMatcher<BrushGeometry>& matcher, const BrushGeometry& newGeometry, const bool uvLock) {
            matcher.processRightFaces([&](BrushFaceGeometry* left, BrushFaceGeometry* right){
                auto* leftFace = left->payload();
                auto* rightFace = leftFace->clone();

                rightFace->setGeometry(right);
                rightFace->updatePointsFromVertices();

                if (uvLock) {
                    applyUVLock(matcher, left, right);
                }
            });

            const NotifyNodeChange nodeChange(this);
            VectorUtils::clearAndDelete(m_faces);
            updateFacesFromGeometry(worldBounds, newGeometry);
            rebuildGeometry(worldBounds);
        }

        Brush::VertexSet Brush::createVertexSet(const std::vector<vm::vec3>& vertices) {
            return VertexSet(std::begin(vertices), std::end(vertices));
        }

        BrushList Brush::subtract(const ModelFactory& factory, const vm::bbox3& worldBounds, const String& defaultTextureName, const BrushList& subtrahends) const {
            auto result = std::list<BrushGeometry>{*m_geometry};

            for (auto* subtrahend : subtrahends) {
                auto nextResults = std::list<BrushGeometry>();

                for (const BrushGeometry& fragment : result) {
                    const auto subFragments = fragment.subtract(*subtrahend->m_geometry);
                    ListUtils::append(nextResults, subFragments);
                }

                result = nextResults;
            }

            BrushList brushes;
            brushes.reserve(result.size());

            for (const auto& geometry : result) {
                try {
                    auto* brush = createBrush(factory, worldBounds, defaultTextureName, geometry, subtrahends);
                    brushes.push_back(brush);
                } catch (const GeometryException&) {}
            }

            return brushes;
        }

        BrushList Brush::subtract(const ModelFactory& factory, const vm::bbox3& worldBounds, const String& defaultTextureName, Brush* subtrahend) const {
            return subtract(factory, worldBounds, defaultTextureName, BrushList{subtrahend});
        }

        void Brush::intersect(const vm::bbox3& worldBounds, const Brush* brush) {
            for (const auto* face : brush->faces()) {
                addFace(face->clone());
            }

            rebuildGeometry(worldBounds);
        }

        bool Brush::canTransform(const vm::mat4x4& transformation, const vm::bbox3& worldBounds) const {
            auto* testBrush = clone(worldBounds);
            bool result = true;

            try {
                testBrush->doTransform(transformation, false, worldBounds);
            } catch (GeometryException&) {
                result = false;
            }

            delete testBrush;
            return result;
        }

        Brush* Brush::createBrush(const ModelFactory& factory, const vm::bbox3& worldBounds, const String& defaultTextureName, const BrushGeometry& geometry, const BrushList& subtrahends) const {
            BrushFaceList faces(0);
            faces.reserve(geometry.faceCount());

            for (const auto* face : geometry.faces()) {
                const auto* h1 = face->boundary().front();
                const auto* h0 = h1->next();
                const auto* h2 = h0->next();

                const auto& p0 = h0->origin()->position();
                const auto& p1 = h1->origin()->position();
                const auto& p2 = h2->origin()->position();

                BrushFaceAttributes attribs(defaultTextureName);
                faces.push_back(factory.createFace(p0, p1, p2, attribs));
            }

            auto* brush = factory.createBrush(worldBounds, faces);
            brush->cloneFaceAttributesFrom(this);
            for (const auto* subtrahend : subtrahends) {
                brush->cloneInvertedFaceAttributesFrom(subtrahend);
            }
            return brush;
        }

        void Brush::updateFacesFromGeometry(const vm::bbox3& worldBounds, const BrushGeometry& brushGeometry) {
            m_faces.clear();

            for (const auto* faceG : brushGeometry.faces()) {
                auto* face = faceG->payload();
                if (face != nullptr) { // could happen if the brush isn't fully specified
                    assert(face->geometry() == faceG);
                    if (face->brush() == nullptr) {
                        addFace(face);
                    } else {
                        m_faces.push_back(face);
                    }
                    face->resetTexCoordSystemCache();
                }
            }

            invalidateVertexCache();
        }

        void Brush::updatePointsFromVertices(const vm::bbox3& worldBounds) {
            for (auto* geometry : m_geometry->faces()) {
                auto* face = geometry->payload();
                face->updatePointsFromVertices();
            }

            rebuildGeometry(worldBounds);
        }

        void Brush::rebuildGeometry(const vm::bbox3& worldBounds) {
            const vm::bbox3 oldBounds = physicalBounds();
            deleteGeometry();
            buildGeometry(worldBounds);
            nodePhysicalBoundsDidChange(oldBounds);
        }

        void Brush::buildGeometry(const vm::bbox3& worldBounds) {
            assert(m_geometry == nullptr);

            m_geometry = new BrushGeometry(worldBounds.expand(1.0));

            AddFacesToGeometry addFacesToGeometry(*m_geometry, m_faces);
            updateFacesFromGeometry(worldBounds, *m_geometry);

            if (addFacesToGeometry.brushEmpty()) {
                throw GeometryException("Brush is empty");
            } else  if (!addFacesToGeometry.brushValid()) {
                throw GeometryException("Brush is invalid");
            } else if (!fullySpecified()) {
                throw GeometryException("Brush is not fully specified");
            }
        }

        void Brush::deleteGeometry() {
            assert(m_geometry != nullptr);

            // clear brush face geometry
            for (auto* brushFace : m_faces) {
                brushFace->setGeometry(nullptr);
            }
            delete m_geometry;
            m_geometry = nullptr;
        }

        bool Brush::checkGeometry() const {
            for (const auto* face : m_faces) {
                if (face->geometry() == nullptr) {
                    return false;
                }
                if (!m_geometry->faces().contains(face->geometry())) {
                    return false;
                }
            }

            for (const auto* geometry : m_geometry->faces()) {
                if (geometry->payload() == nullptr) {
                    return false;
                }
                if (!VectorUtils::contains(m_faces, geometry->payload())) {
                    return false;
                }
            }

            return true;
        }

        void Brush::findIntegerPlanePoints(const vm::bbox3& worldBounds) {
            const NotifyNodeChange nodeChange(this);

            for (auto* face : m_faces) {
                face->findIntegerPlanePoints();
            }
            rebuildGeometry(worldBounds);
        }

        const String& Brush::doGetName() const {
            static const String name("brush");
            return name;
        }

        const vm::bbox3& Brush::doGetLogicalBounds() const {
            ensure(m_geometry != nullptr, "geometry is null");
            return m_geometry->bounds();
        }

        const vm::bbox3& Brush::doGetPhysicalBounds() const {
            return logicalBounds();
        }

        Node* Brush::doClone(const vm::bbox3& worldBounds) const {
            BrushFaceList faceClones;
            faceClones.reserve(m_faces.size());

            for (const auto* face : m_faces) {
                faceClones.push_back(face->clone());
            }

            auto* brush = new Brush(worldBounds, faceClones);
            cloneAttributes(brush);
            return brush;
        }

        bool Brush::doCanAddChild(const Node* child) const {
            return false;
        }

        bool Brush::doCanRemoveChild(const Node* child) const {
            return false;
        }

        bool Brush::doRemoveIfEmpty() const {
            return false;
        }

        bool Brush::doShouldAddToSpacialIndex() const {
            return true;
        }

        bool Brush::doSelectable() const {
            return true;
        }

        void Brush::doGenerateIssues(const IssueGenerator* generator, IssueList& issues) {
            generator->generate(this, issues);
        }

        void Brush::doAccept(NodeVisitor& visitor) {
            visitor.visit(this);
        }

        void Brush::doAccept(ConstNodeVisitor& visitor) const {
            visitor.visit(this);
        }

        void Brush::doPick(const vm::ray3& ray, PickResult& pickResult) const {
            const auto hit = findFaceHit(ray);
            if (hit.face != nullptr) {
                ensure(!vm::is_nan(hit.distance), "nan hit distance");
                const auto hitPoint = vm::point_at_distance(ray, hit.distance);
                pickResult.addHit(Hit(BrushHit, hit.distance, hitPoint, hit.face));
            }
        }

        void Brush::doFindNodesContaining(const vm::vec3& point, NodeList& result) {
            if (containsPoint(point)) {
                result.push_back(this);
            }
        }

        Brush::BrushFaceHit::BrushFaceHit() : face(nullptr), distance(vm::nan<FloatType>()) {}

        Brush::BrushFaceHit::BrushFaceHit(BrushFace* i_face, const FloatType i_distance) : face(i_face), distance(i_distance) {}

        Brush::BrushFaceHit Brush::findFaceHit(const vm::ray3& ray) const {
            if (vm::is_nan(vm::intersect_ray_bbox(ray, logicalBounds()))) {
                return BrushFaceHit();
            }

            for (auto* face : m_faces) {
                const auto distance = face->intersectWithRay(ray);
                if (!vm::is_nan(distance)) {
                    return BrushFaceHit(face, distance);
                }
            }
            return BrushFaceHit();
        }

        Node* Brush::doGetContainer() const {
            FindContainerVisitor visitor;
            escalate(visitor);
            return visitor.hasResult() ? visitor.result() : nullptr;
        }

        Layer* Brush::doGetLayer() const {
            FindLayerVisitor visitor;
            escalate(visitor);
            return visitor.hasResult() ? visitor.result() : nullptr;
        }

        Group* Brush::doGetGroup() const {
            FindGroupVisitor visitor;
            escalate(visitor);
            return visitor.hasResult() ? visitor.result() : nullptr;
        }

        void Brush::doTransform(const vm::mat4x4& transformation, bool lockTextures, const vm::bbox3& worldBounds) {
            const NotifyNodeChange nodeChange(this);

            for (auto* face : m_faces) {
                face->transform(transformation, lockTextures);
            }

            rebuildGeometry(worldBounds);
        }

        class Brush::Contains : public ConstNodeVisitor, public NodeQuery<bool> {
        private:
            const Brush* m_this;
        public:
            Contains(const Brush* i_this) :
            m_this(i_this) {}
        private:
            void doVisit(const World* world) override   { setResult(false); }
            void doVisit(const Layer* layer) override   { setResult(false); }
            void doVisit(const Group* group) override   { setResult(contains(group->logicalBounds())); }
            void doVisit(const Entity* entity) override { setResult(contains(entity->logicalBounds())); }
            void doVisit(const Brush* brush) override   { setResult(contains(brush)); }

            bool contains(const vm::bbox3& bounds) const {
                if (m_this->logicalBounds().contains(bounds)) {
                    return true;
                }

                for (const auto& vertex : bounds.vertices()) {
                    if (!m_this->containsPoint(vertex)) {
                        return false;
                    }
                }

                return true;
            }

            bool contains(const Brush* brush) const {
                return m_this->m_geometry->contains(*brush->m_geometry, QueryCallback());
            }
        };

        bool Brush::doContains(const Node* node) const {
            Contains contains(this);
            node->accept(contains);
            assert(contains.hasResult());
            return contains.result();
        }

        class Brush::Intersects : public ConstNodeVisitor, public NodeQuery<bool> {
        private:
            const Brush* m_this;
        public:
            Intersects(const Brush* i_this) :
            m_this(i_this) {}
        private:
            void doVisit(const World* world) override   { setResult(false); }
            void doVisit(const Layer* layer) override   { setResult(false); }
            void doVisit(const Group* group) override   { setResult(intersects(group->logicalBounds())); }
            void doVisit(const Entity* entity) override { setResult(intersects(entity->logicalBounds())); }
            void doVisit(const Brush* brush) override   { setResult(intersects(brush)); }

            bool intersects(const vm::bbox3& bounds) const {
                return m_this->logicalBounds().intersects(bounds);
            }

            bool intersects(const Brush* brush) {
                return m_this->m_geometry->intersects(*brush->m_geometry, QueryCallback());
            }
        };

        bool Brush::doIntersects(const Node* node) const {
            Intersects intersects(this);
            node->accept(intersects);
            assert(intersects.hasResult());
            return intersects.result();
        }

        void Brush::invalidateVertexCache() {
            m_brushRendererBrushCache->invalidateVertexCache();
        }

        Renderer::BrushRendererBrushCache& Brush::brushRendererBrushCache() const {
            return *m_brushRendererBrushCache;
        }

        void Brush::initializeTags(TagManager& tagManager) {
            Taggable::initializeTags(tagManager);
            for (auto* face : m_faces) {
                face->initializeTags(tagManager);
            }
        }

        void Brush::clearTags() {
            for (auto* face : m_faces) {
                face->clearTags();
            }
            Taggable::clearTags();
        }

        bool Brush::allFacesHaveAnyTagInMask(Tag::TagType tagMask) const {
            // Possible optimization: Store the shared face tag mask in the brush and updated it when a face changes.

            Tag::TagType sharedFaceTags = ~Tag::TagType(0); // set all bits to 1
            for (const auto* face : m_faces) {
                sharedFaceTags &= face->tagMask();
            }
            return (sharedFaceTags & tagMask) != 0;
        }

        bool Brush::anyFaceHasAnyTag() const {
            for (const auto* face : m_faces) {
                if (face->hasAnyTag()) {
                    return true;
                }
            }
            return false;
        }

        bool Brush::anyFacesHaveAnyTagInMask(Tag::TagType tagMask) const {
            // Possible optimization: Store the shared face tag mask in the brush and updated it when a face changes.

            for (const auto* face : m_faces) {
                if (face->hasTag(tagMask)) {
                    return true;
                }
            }
            return false;
        }

        void Brush::doAcceptTagVisitor(TagVisitor& visitor) {
            visitor.visit(*this);
        }

        void Brush::doAcceptTagVisitor(ConstTagVisitor& visitor) const {
            visitor.visit(*this);
        }
    }
}
