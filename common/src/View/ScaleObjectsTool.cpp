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

#include "ScaleObjectsTool.h"

#include "Preferences.h"
#include "PreferenceManager.h"
#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/BrushGeometry.h"
#include "Model/CollectMatchingBrushFacesVisitor.h"
#include "Model/FindMatchingBrushFaceVisitor.h"
#include "Model/HitAdapter.h"
#include "Model/HitQuery.h"
#include "Model/NodeVisitor.h"
#include "Model/PickResult.h"
#include "Renderer/Camera.h"
#include "View/Grid.h"
#include "View/MapDocument.h"

#include <algorithm>
#include <iterator>
#include <array>
#include <array>

namespace TrenchBroom {
    namespace View {
        const Model::Hit::HitType ScaleObjectsTool::ScaleToolFaceHit = Model::Hit::freeHitType();
        const Model::Hit::HitType ScaleObjectsTool::ScaleToolEdgeHit = Model::Hit::freeHitType();
        const Model::Hit::HitType ScaleObjectsTool::ScaleToolCornerHit = Model::Hit::freeHitType();
        
        static const std::vector<BBoxSide> AllSides() {
            std::vector<BBoxSide> result;
            result.reserve(6);
            
            const BBox3 box{{-1, -1, -1}, {1, 1, 1}};
            auto op = [&](const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& normal) {
                result.push_back(BBoxSide(normal));
            };
            eachBBoxFace(box, op);
            
            assert(result.size() == 6);
            return result;
        };
        
        static Vec3 normalForBBoxSide(const BBoxSide side) {
            return side.normal;
        }

        static BBoxSide sideForNormal(const Vec3& normal) {
            assert(normal == normal.firstAxis());
            return BBoxSide(normal);
        }
        
        static std::vector<BBoxEdge> AllEdges() {
            std::vector<BBoxEdge> result;
            result.reserve(12);
            
            const BBox3 box{{-1, -1, -1}, {1, 1, 1}};
            auto op = [&](const Vec3& p0, const Vec3& p1) {
                result.push_back(BBoxEdge(p0, p1));
            };
            eachBBoxEdge(box, op);
            
            assert(result.size() == 12);
            return result;
        }
        
        static std::vector<BBoxCorner> AllCorners() {
            std::vector<BBoxCorner> result;
            result.reserve(8);
            
            const BBox3 box{{-1, -1, -1}, {1, 1, 1}};
            auto op = [&](const Vec3& point) {
                result.push_back(BBoxCorner(point));
            };
            eachBBoxVertex(box, op);
            
            assert(result.size() == 8);
            return result;
        }
        
        static Vec3 pointForBBoxCorner(const BBox3& box, const BBoxCorner corner) {
            Vec3 res;
            for (size_t i = 0; i < 3; ++i) {
                assert(corner.corner[i] == 1.0 || corner.corner[i] == -1.0);
                
                res[i] = (corner.corner[i] == 1.0) ? box.max[i] : box.min[i];
            }
            return res;
        }
        
        static Vec3 normalForBBoxCorner(const BBoxCorner corner) {
            // HACK: Due to the representation of corners, all we need to do is normalize it
            return corner.corner.normalized();
        }
        
        static BBoxSide oppositeSide(const BBoxSide side) {
            return BBoxSide(side.normal * -1.0);
        }
        
        static BBoxCorner oppositeCorner(const BBoxCorner corner) {
            return BBoxCorner(Vec3(-corner.corner.x(),
                                   -corner.corner.y(),
                                   -corner.corner.z()));
        }
        
        static BBoxEdge oppositeEdge(const BBoxEdge edge) {
            return BBoxEdge(oppositeCorner(BBoxCorner(edge.point0)).corner,
                            oppositeCorner(BBoxCorner(edge.point1)).corner);
        }
        
        static Edge3 pointsForBBoxEdge(const BBox3& box, const BBoxEdge edge) {
            return Edge3(pointForBBoxCorner(box, BBoxCorner(edge.point0)),
                         pointForBBoxCorner(box, BBoxCorner(edge.point1)));
        }

        static size_t axisIndexParallelToBBoxEdge(const BBoxEdge edge) {
            size_t answer = 0;
            size_t timesSet = 0;
            for (size_t i = 0; i < 3; ++i) {
                if (edge.point0[i] != edge.point1[i]) {
                    answer = i;
                    ++timesSet;
                }
            }
            assert(timesSet == 1);
            return answer;
        }
        
        static Vec3 normalForBBoxEdge(const BBoxEdge edge) {
            const Vec3 corner0Normal = edge.point0;
            const Vec3 corner1Normal = edge.point1;
            return (corner0Normal + corner1Normal).normalized();
        }

        static Polygon3 polygonForBBoxSide(const BBox3& box, const BBoxSide side) {
            const Vec3 wantedNormal = normalForBBoxSide(side);
            
            Polygon3 res;
            auto visitor = [&](const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& n){
                if (n == wantedNormal) {
                    const Polygon3 poly {p0, p1, p2, p3};
                    res = poly;
                }
            };
            eachBBoxFace(box, visitor);
            
            assert(res.vertexCount() == 4);
            return res;
        }
        
        static Vec3 centerForBBoxSide(const BBox3& box, const BBoxSide side) {
            const Vec3 wantedNormal = normalForBBoxSide(side);
            
            Vec3 result;
            bool setResult = false;
            
            auto visitor = [&](const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& n){
                if (n == wantedNormal) {
                    result = (p0 + p1 + p2 + p3) / 4.0;
                    setResult = true;
                }
            };
            eachBBoxFace(box, visitor);
            assert(setResult);
            return result;
        }

        // manipulating bboxes

        static BBox3 moveBBoxGeneral(const BBox3& in,
                                     const Vec3& dragLineStart,
                                     const Vec3& dragLineEnd,
                                     const size_t numAxes) {

            const Vec3 resizeDir = (dragLineEnd - dragLineStart).normalized();
            const std::array<size_t, 3> axes {
                    resizeDir.firstComponent(),
                    resizeDir.secondComponent(),
                    resizeDir.thirdComponent()
            };

            Vec3 newSize = in.size();

            assert(numAxes >= 1 && numAxes <= 3);

            for (size_t i = 0; i < numAxes; ++i) {
                const size_t axisIndex = axes[i];
                newSize[axisIndex] = std::abs(dragLineEnd[axisIndex] - dragLineStart[axisIndex]);
            }

            const auto matrix = scaleBBoxMatrixWithAnchor(in, newSize, dragLineStart);

            return BBox3(matrix * in.min, matrix * in.max);
        }

        static BBox3 moveBBoxFace(const BBox3& in, const BBoxSide side, const FloatType sideLength, const bool proportional) {
            if (sideLength <= 0) {
                std::cerr << "moveBBoxFace: given invalid side length " << sideLength << "\n";
                return in;
            }

            const Vec3 n = side.normal;
            const size_t axis1 = n.firstComponent();
            const size_t axis2 = n.secondComponent();
            const size_t axis3 = n.thirdComponent();

            Vec3 newSize = in.size();

            newSize[axis1] = sideLength;
            if (proportional) {
                const FloatType ratio = sideLength / in.size()[axis1];
                newSize[axis2] *= ratio;
                newSize[axis3] *= ratio;
            }

            const Vec3 anchor = centerForBBoxSide(in, oppositeSide(side));

            const auto matrix = scaleBBoxMatrixWithAnchor(in, newSize, anchor);

            return BBox3(matrix * in.min, matrix * in.max);
        }


        static BBox3 moveBBoxCorner(const BBox3& in,
                                    const Line3& dragLine,
                                    const FloatType sideLength,
                                    const BBoxCorner corner) {

            const BBoxCorner opposite = oppositeCorner(corner);
            const Vec3 anchor = pointForBBoxCorner(in, opposite);

            const size_t axis1 = dragLine.direction.firstComponent();
            const size_t axis2 = dragLine.direction.secondComponent();
            const size_t axis3 = dragLine.direction.thirdComponent();

            Vec3 newSize = in.size();

            newSize[axis1] = sideLength;
            const FloatType ratio = sideLength / in.size()[axis1];
            newSize[axis2] *= ratio;
            newSize[axis3] *= ratio;

            const auto matrix = scaleBBoxMatrixWithAnchor(in, newSize, anchor);

            return BBox3(matrix * in.min, matrix * in.max);
        }

        static BBox3 moveBBoxEdge(const BBox3& in,
                                    const Line3& dragLine,
                                    const FloatType sideLength,
                                    const BBoxEdge edge) {

            const BBoxEdge opposite = oppositeEdge(edge);
            const Vec3 anchor = pointsForBBoxEdge(in, opposite).center();

            // get the ratio
            const size_t axis1 = dragLine.direction.firstComponent();
            const FloatType ratio = sideLength / in.size()[axis1];

            const size_t edgeAxis = axisIndexParallelToBBoxEdge(edge);

            Vec3 newSize = in.size();
            for (size_t i = 0; i < 3; ++i) {
                if (i != edgeAxis) {
                    newSize[i] *= ratio;
                }
            }

            const auto matrix = scaleBBoxMatrixWithAnchor(in, newSize, anchor);

            return BBox3(matrix * in.min, matrix * in.max);
        }
        
        ScaleObjectsTool::ScaleObjectsTool(MapDocumentWPtr document) :
        Tool(false),
        m_document(document),
        m_toolPage(nullptr),
        m_dragStartHit(Model::Hit::NoHit),
        m_resizing(false)
        {
            bindObservers();
        }
        
        ScaleObjectsTool::~ScaleObjectsTool() {
            unbindObservers();
        }
        
        bool ScaleObjectsTool::applies() const {
            MapDocumentSPtr document = lock(m_document);
            return !document->selectedNodes().empty();
        }
    
        Model::Hit ScaleObjectsTool::pick(const Ray3& pickRay, const Renderer::Camera& camera, const Model::PickResult& pickResult, const bool is3D) {
            const BBox3& myBounds = bounds();
            
            // origin in bbox
            if (myBounds.contains(pickRay.origin))
                return Model::Hit::NoHit;

            Model::PickResult localPickResult;

            // these handles only work in 3D.
            if (camera.perspectiveProjection()) {
                // corners
                for (const BBoxCorner& corner : AllCorners()) {
                    const Vec3 point = pointForBBoxCorner(myBounds, corner);
                    
                    // make the spheres for the corner handles slightly larger than the
                    // cylinders of the edge handles, so they take priority where they overlap.
                    const FloatType cornerRadius = pref(Preferences::HandleRadius) * 2.0;
                    const FloatType dist = camera.pickPointHandle(pickRay, point, cornerRadius);
                    if (!Math::isnan(dist)) {
                        localPickResult.addHit(Model::Hit(ScaleToolCornerHit, dist, pickRay.pointAtDistance(dist), corner));
                    }
                }
                
                // edges
                for (const BBoxEdge& edge : AllEdges()) {
                    const Edge3 points = pointsForBBoxEdge(myBounds, edge);
                    
                    const FloatType dist = camera.pickLineSegmentHandle(pickRay, points, pref(Preferences::HandleRadius));
                    if (!Math::isnan(dist)) {
                        localPickResult.addHit(Model::Hit(ScaleToolEdgeHit, dist, pickRay.pointAtDistance(dist), edge));
                    }
                }
                
                // faces
                for (const BBoxSide side : AllSides()) {
                    const auto poly = polygonForBBoxSide(myBounds, side);
                    
                    const FloatType dist = intersectPolygonWithRay(pickRay, poly.begin(), poly.end());
                    if (!Math::isnan(dist)) {
                        localPickResult.addHit(Model::Hit(ScaleToolFaceHit, dist, pickRay.pointAtDistance(dist), side));
                    }
                }
            }
            
            // bbox corners in 2d views
            if (camera.orthographicProjection()) {
                for (const BBoxEdge& edge : AllEdges()) {
                    const Edge3 points = pointsForBBoxEdge(myBounds, edge);
                    
                    // in 2d views, only use edges that are parallel to the camera
                    if (points.direction().parallelTo(camera.direction())) {
                        // could figure out which endpoint is closer to camera, or just test both.
                        for (const Vec3& point : Vec3::List{points.start(), points.end()}) {
                            const FloatType dist = camera.pickPointHandle(pickRay, point, pref(Preferences::HandleRadius));
                            if (!Math::isnan(dist)) {
                                localPickResult.addHit(Model::Hit(ScaleToolEdgeHit, dist, pickRay.pointAtDistance(dist), edge));
                            }
                        }
                    }
                }
            }

            // select back faces
            if (localPickResult.empty()) {
                
                FloatType closestDistToRay = std::numeric_limits<FloatType>::max();
                FloatType bestDistAlongRay = std::numeric_limits<FloatType>::max();
                Vec3 bestNormal;
                
                // idea is: find the closest point on an edge of the cube, belonging
                // to a face that's facing away from the pick ray.
                auto visitor = [&](const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& n){
                    const FloatType cosAngle = n.dot(pickRay.direction);
                    if (cosAngle >= 0.0 && cosAngle < 1.0) {
                        // the face is pointing away from the camera (or exactly perpendicular)
                        // but not equal to the camera direction (important for 2D views)
                        
                        const std::array<Vec3, 4> points{p0, p1, p2, p3};
                        for (size_t i = 0; i < 4; i++) {
                            const Ray3::LineDistance result = pickRay.distanceToSegment(points[i], points[(i + 1) % 4]);
                            if (!Math::isnan(result.distance) && result.distance < closestDistToRay) {
                                closestDistToRay = result.distance;
                                bestNormal = n;
                                bestDistAlongRay = result.rayDistance;
                            }
                        }
                    }
                };
                eachBBoxFace(myBounds, visitor);
                
                assert(bestNormal != Vec3::Null);
                localPickResult.addHit(Model::Hit(ScaleToolFaceHit, bestDistAlongRay, pickRay.pointAtDistance(bestDistAlongRay), BBoxSide{bestNormal}));
                
                //std::cout << "closest: " << pickRay.pointAtDistance(bestDistAlongRay) << "\n";
            }
            
            auto hit = localPickResult.query().first();

#if 0
            if (hit.type() == ScaleToolFaceHit)
                std::cout << "hit face " << normalForBBoxSide(hit.target<BBoxSide>()) << "\n";
            else if (hit.type() == ScaleToolEdgeHit)
                printf("hit edge\n");
            else if (hit.type() == ScaleToolCornerHit)
                printf("hit corner\n");
            else
                printf("no hit\n");
#endif
            
            return hit;
        }
        
        BBox3 ScaleObjectsTool::bounds() const {
            MapDocumentSPtr document = lock(m_document);
            return document->selectionBounds();
        }
        
        // used for rendering
        
        std::vector<Polygon3f> ScaleObjectsTool::polygonsHighlightedByProportionalDrag() const {
            if (m_isProportional) {
                if (m_dragStartHit.type() == ScaleToolFaceHit) {
                    const auto side = m_dragStartHit.target<BBoxSide>();
                    
                    // return all sides except the opposite one
                    

                    return {};
                }
                if (m_dragStartHit.type() == ScaleToolEdgeHit) {
                    return {};
                }
            }
            return {};
        }
        
        /**
         * For dragging a corner retursn the 3 sides that touch that corner
         */
        static std::vector<BBoxSide> sidesForCornerSelection(const BBoxCorner corner) {
            std::vector<BBoxSide> result;
            for (size_t i = 0; i < 3; ++i) {
                Vec3 sideNormal = Vec3::Null;
                sideNormal[i] = corner.corner[i];
                
                result.push_back(BBoxSide(sideNormal));
            }
            assert(result.size() == 3);
            return result;
        }
        
        /**
         * For dragging an edge, returns the 2 bbox sides that contain that edge
         */
        static std::vector<BBoxSide> sidesForEdgeSelection(const BBoxEdge edge) {
            std::vector<BBoxSide> result;
            
            const BBox3 box{{-1, -1, -1}, {1, 1, 1}};
            
            auto visitor = [&](const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& n){
                const Vec3 verts[4] = {p0, p1, p2, p3};
                
                // look for the edge
                for (size_t i = 0; i < 4; ++i) {
                    if ((verts[i] == edge.point0 && verts[(i+1)%4] == edge.point1)
                        || (verts[i] == edge.point1 && verts[(i+1)%4] == edge.point0)) {
                        result.push_back(BBoxSide(n));
                    }
                }
                
            };
            eachBBoxFace(box, visitor);
            assert(result.size() == 2);
            
            return result;
        }
        
        static std::vector<Polygon3f> polysForSides(const BBox3& box,
                                                    const std::vector<BBoxSide>& sides) {
            std::vector<Polygon3f> result;
            for (const auto side : sides) {
                result.push_back(Polygon3f(polygonForBBoxSide(box, side)));
            }
            return result;
        }
        
        std::vector<Polygon3f> ScaleObjectsTool::polygonsHighlightedByDrag() const {
            if (m_dragStartHit.type() == ScaleToolFaceHit) {
                const auto side = m_dragStartHit.target<BBoxSide>();
                return polysForSides(bounds(), {side});
                
            } else if (m_dragStartHit.type() == ScaleToolEdgeHit) {
                const auto edge = m_dragStartHit.target<BBoxEdge>();
                return polysForSides(bounds(), sidesForEdgeSelection(edge));
                
            } else if (m_dragStartHit.type() == ScaleToolCornerHit) {
                const auto corner = m_dragStartHit.target<BBoxCorner>();
                return polysForSides(bounds(), sidesForCornerSelection(corner));
                
            } else {
                return {};
            }
        }
        
        bool ScaleObjectsTool::hasDragPolygon() const {
            return dragPolygon().vertexCount() > 0;
        }

        Polygon3f ScaleObjectsTool::dragPolygon() const {
            if (m_dragStartHit.type() == ScaleToolFaceHit) {
                const auto side = m_dragStartHit.target<BBoxSide>();
                return Polygon3f(polygonForBBoxSide(bounds(), side));
            }
                                                            
            return Polygon3f();
        }
        
        bool ScaleObjectsTool::hasDragEdge() const {
            return m_dragStartHit.type() == ScaleToolEdgeHit;
        }
        
        Edge3f ScaleObjectsTool::dragEdge() const {
            assert(hasDragEdge());
            auto whichEdge = m_dragStartHit.target<BBoxEdge>();
            return Edge3f(pointsForBBoxEdge(bounds(), whichEdge));
        }
        
        bool ScaleObjectsTool::hasDragCorner() const {
            return m_dragStartHit.type() == ScaleToolCornerHit;
        }
        
        Vec3f ScaleObjectsTool::dragCorner() const {
            assert(hasDragCorner());
            auto whichCorner = m_dragStartHit.target<BBoxCorner>();
            return Vec3f(pointForBBoxCorner(bounds(), whichCorner));
        }

        // for rendering sheared bbox
        BBox3 ScaleObjectsTool::bboxAtDragStart() const {
            return m_bboxAtDragStart;
        }
        Mat4x4 ScaleObjectsTool::bboxShearMatrix() const {
            assert(m_isShearing);
            
            // happens if you cmd+drag on an edge or corner
            if (m_dragStartHit.type() != ScaleToolFaceHit) {
                return Mat4x4::Identity;
            }
            
            const BBoxSide side = m_dragStartHit.target<BBoxSide>();
            
            return shearBBoxMatrix(m_bboxAtDragStart,
                                   side.normal,
                                   m_totalDelta);
        }
        Polygon3f ScaleObjectsTool::shearHandle() const {
            assert(m_isShearing);
            
            // happens if you cmd+drag on an edge or corner
            if (m_dragStartHit.type() != ScaleToolFaceHit) {
                return Polygon3f();
            }
            
            const BBoxSide side = m_dragStartHit.target<BBoxSide>();
            const Polygon3 polyAtDragStart = polygonForBBoxSide(m_bboxAtDragStart, side);
            
            const Polygon3 handle = polyAtDragStart.transformed(bboxShearMatrix());
            return Polygon3f(handle);
        }
        bool ScaleObjectsTool::isShearing() const {
            return m_isShearing;
        }

        Vec3::List ScaleObjectsTool::cornerHandles() const {
            Vec3::List result;
            result.reserve(8);
            auto op = [&](const Vec3& point) {
                result.push_back(point);
            };
            eachBBoxVertex(bounds(), op);
            return result;
        }
        
//        Vec3 ScaleObjectsTool::dragPolygonNormal() const {
//            Plane3 plane;
//            if (!getPlane(m_dragPolygon.begin(), m_dragPolygon.end(), plane))
//                return Vec3(0,0,0);
//
//            return plane.normal;
//        }
        
      void ScaleObjectsTool::updateDragFaces(const Model::PickResult& pickResult) {          
            const Model::Hit& hit = pickResult.query().type(ScaleToolFaceHit | ScaleToolEdgeHit | ScaleToolCornerHit).occluded().first();

            // hack for highlighting on mouseover
            m_dragStartHit = hit;
          
//
//            auto newDragFaces = getDragPolygon(hit);
//            //if (newDragFaces != m_dragPolygon)
//                refreshViews();
//
//            //m_dragPolygon = newDragFaces;
          
      }
        
//        BBoxSide ScaleObjectsTool::getDragPolygon(const Model::Hit& hit) const {
//            if (!hit.isMatch()) return Polygon3();
//
//            const BBoxSide index = hit.target<BBoxSide>();
//            printf("hit out: %d\n", static_cast<int>(index));
//
//            return polygonForBBoxSide(bounds(), index);
//        }
        
//        class ScaleObjectsTool::MatchFaceBoundary {
//        private:
//            const Model::BrushFace* m_reference;
//        public:
//            MatchFaceBoundary(const Model::BrushFace* reference) :
//            m_reference(reference) {
//                ensure(m_reference != nullptr, "reference is null");
//            }
//
//            bool operator()(Model::BrushFace* face) const {
//                return face != m_reference && face->boundary().equals(m_reference->boundary());
//            }
//        };
        
        Model::BrushFaceList ScaleObjectsTool::collectDragFaces(const Model::Hit& hit) const {
            assert(hit.isMatch());
//            assert(hit.type() == ScaleHit2D || hit.type() == ScaleHit3D);

            Model::BrushFaceList result;
//            if (hit.type() == ScaleHit2D) {
//                const Model::BrushFaceList& faces = hit.target<Model::BrushFaceList>();
//                assert(!faces.empty());
//                VectorUtils::append(result, faces);
//                VectorUtils::append(result, collectDragFaces(faces[0]));
//                if (faces.size() > 1)
//                    VectorUtils::append(result, collectDragFaces(faces[1]));
//            } else {
//                Model::BrushFace* face = hit.target<Model::BrushFace*>();
//                result.push_back(face);
//                VectorUtils::append(result, collectDragFaces(face));
//            }

            return result;
        }
        
        Model::BrushFaceList ScaleObjectsTool::collectDragFaces(Model::BrushFace* face) const {
            return {};
//            Model::CollectMatchingBrushFacesVisitor<MatchFaceBoundary> visitor((MatchFaceBoundary(face)));
//
//            MapDocumentSPtr document = lock(m_document);
//            const Model::NodeList& nodes = document->selectedNodes().nodes();
//            Model::Node::accept(std::begin(nodes), std::end(nodes), visitor);
//            return visitor.faces();
        }
        
        bool ScaleObjectsTool::beginResize(const Model::PickResult& pickResult, const bool proportional, const bool vertical, const bool shear) {
            const Model::Hit& hit = pickResult.query().type(ScaleToolFaceHit | ScaleToolEdgeHit | ScaleToolCornerHit).occluded().first();
            if (!hit.isMatch())
                return false;
            
            m_dragStartHit = hit;
            m_bboxAtDragStart = bounds();
            m_dragOrigin = hit.hitPoint();
            m_totalDelta = Vec3::Null;
            
            if (hit.type() == ScaleToolFaceHit)
                printf("start face\n");
            else if (hit.type() == ScaleToolEdgeHit)
                printf("start edge\n");
            else if (hit.type() == ScaleToolCornerHit)
                printf("start corner\n");
            else
                assert(0);
            
//            m_dragSide = hit.target<BBoxSide>();
//            std::cout << "initial hitpoint: " << hit.hitPoint() << " drag side: " << m_dragSide.normal.asString() << "\n";
            
         //   m_splitBrushes = split;
            
            MapDocumentSPtr document = lock(m_document);
            document->beginTransaction("Resize Brushes");
            m_resizing = true;
            
            m_isShearing = shear;
            m_isProportional = proportional;
            
            return true;
        }

        bool ScaleObjectsTool::resize(const Ray3& pickRay, const Renderer::Camera& camera, const bool proportional, const bool vertical) {
//            assert(!m_dragFaces.empty());
//            assert(hasDragPolygon());
//
//            Model::BrushFace* dragFace = m_dragFaces.front();
            
            printf("proportional %d vertical %d shear %d\n",
                   (int)proportional, (int)vertical, (int)m_isShearing);
            
            MapDocumentSPtr document = lock(m_document);
            const View::Grid& grid = document->grid();
           
            if (!m_isShearing) {
                
                Vec3 resizeStart, resizeEnd;
                if (m_dragStartHit.type() == ScaleToolFaceHit) {
                    const auto endSide = m_dragStartHit.target<BBoxSide>();
                    const auto startSide = oppositeSide(endSide);
                    
                    resizeStart = centerForBBoxSide(bounds(), startSide);
                    resizeEnd = centerForBBoxSide(bounds(), endSide);

                    std::cout << "ScaleObjectsTool::resize from face " << resizeStart << " to " << resizeEnd << "\n";
                } else if (m_dragStartHit.type() == ScaleToolEdgeHit) {
                    const auto endEdge = m_dragStartHit.target<BBoxEdge>();
                    const auto startEdge = oppositeEdge(endEdge);
                    
                    const Edge3 endEdgeActual = pointsForBBoxEdge(bounds(), endEdge);
                    const Edge3 startEdgeActual = pointsForBBoxEdge(bounds(), startEdge);
                    
                    resizeStart = startEdgeActual.center();
                    resizeEnd = endEdgeActual.center();

                    std::cout << "ScaleObjectsTool::resize from edge " << resizeStart << " to " << resizeEnd << "\n";
                } else if (m_dragStartHit.type() == ScaleToolCornerHit) {
                    const auto endCorner = m_dragStartHit.target<BBoxCorner>();
                    const auto startCorner = oppositeCorner(endCorner);
                    
                    resizeStart = pointForBBoxCorner(bounds(), startCorner);
                    resizeEnd = pointForBBoxCorner(bounds(), endCorner);

                    std::cout << "ScaleObjectsTool::resize from corner " << resizeStart << " to " << resizeEnd << "\n";
                } else
                    assert(0);

                // FIXME: base on the m_bboxAtDragStart

                // This is a line from:
                //  - face dragging: center of opposite face to center of face being dragged
                //  - edge dragging: midpoint of diagonally opposite edge to midpoint of edge being dragged
                //  - corner dragging: diagonally opposite corner to corner being dragged
                const Line3 resizeLine(resizeStart, (resizeEnd - resizeStart).normalized());

                // find closest point on resizeLine to pickRay
                const Ray3::LineDistance distance = pickRay.distanceToLine(resizeLine.point, resizeLine.direction);
                if (distance.parallel)
                    return true;

                const Vec3 hitPoint = resizeLine.pointAtDistance(distance.lineDistance);
                const Vec3 hitPointSnapped = grid.snap(hitPoint, resizeLine);

                const FloatType dragDist = resizeLine.distance(hitPointSnapped);

                if (dragDist < 0.0) {
                    std::cout << "can't collapse bbox\n";
                    return true;
                }

                // only do sides for now
                BBox3 newBbox;

                if (m_dragStartHit.type() == ScaleToolFaceHit) {
//                    const auto side = m_dragStartHit.target<BBoxSide>();
                    newBbox = moveBBoxGeneral(bounds(), resizeStart, hitPointSnapped, proportional ? 3 : 1);
                } else if (m_dragStartHit.type() == ScaleToolEdgeHit) {
//                    const auto edge = m_dragStartHit.target<BBoxEdge>();
                    newBbox = moveBBoxGeneral(bounds(), resizeStart, hitPointSnapped, proportional ? 3 : 2);
                } else if (m_dragStartHit.type() == ScaleToolCornerHit) {
//                    const auto corner = m_dragStartHit.target<BBoxCorner>();
                    newBbox = moveBBoxGeneral(bounds(), resizeStart, hitPointSnapped, 3);
                } else
                    assert(0);

                if (newBbox.empty()) {
                    std::cout << "skipping because empty\n";
                } else {
                    if (document->scaleObjectsBBox(bounds(), newBbox)) {
                        m_totalDelta += Vec3(1,0,0); // FIXME:
                        //m_dragOrigin += faceDelta;
                    }
                }

                return true;


#if 0
                std::cout << "ScaleObjectsTool::resize with start bbox: "
                << m_bboxAtDragStart.min.asString() << "->"
                << m_bboxAtDragStart.max.asString() << "\n";
                //                        << " side: " << m_dragSide.normal.asString() << "\n";
                

                
                const Vec3 relativeFaceDelta = dragDist * resizeLine.direction; //grid.snap(dragDist * resizeLine.direction, resizeLine);
                //const Vec3 relativeFaceDelta = grid.snap(dragDist) * dragDir;
                //const Vec3 absoluteFaceDelta = grid.moveDelta(dragFace, faceNormal * dragDist);
                
                const Vec3 faceDelta = relativeFaceDelta;//selectDelta(relativeFaceDelta, absoluteFaceDelta, dragDist);
                
                BBox3 newBbox;
                if (m_dragStartHit.type() == ScaleToolFaceHit) {
                    const auto side = m_dragStartHit.target<BBoxSide>();
                    newBbox = moveBBoxFace(m_bboxAtDragStart, side, faceDelta);
                } else if (m_dragStartHit.type() == ScaleToolEdgeHit) {
                    const auto edge = m_dragStartHit.target<BBoxEdge>();
                    newBbox = moveBBoxEdge(m_bboxAtDragStart, edge, faceDelta);
                } else if (m_dragStartHit.type() == ScaleToolCornerHit) {
                    const auto corner = m_dragStartHit.target<BBoxCorner>();
                    newBbox = moveBBoxCorner(m_bboxAtDragStart, corner, faceDelta);
                } else
                    assert(0);

                std::cout << "ScaleObjectsTool new bbox: "
                << newBbox.min.asString() << "->"
                << newBbox.max.asString() << "\n";
                
                std::cout << "make resize with delta: " << faceDelta << "\n";
                if (newBbox.empty()) {
                    std::cout << "skipping because empty\n";
                } else {
                    if (document->scaleObjectsBBox(bounds(), newBbox)) {
                        m_totalDelta += faceDelta;
                        //m_dragOrigin += faceDelta;
                    }
                }
#endif
            } else {
                // shear
                if (m_dragStartHit.type() == ScaleToolFaceHit) {
                    const BBoxSide side = m_dragStartHit.target<BBoxSide>();
                    
                    const auto poly = polygonForBBoxSide(bounds(), side);
                    const Vec3 planeAnchor = poly.vertices().front();
                    
                    // get the point where the pick ray intersects the plane being dragged.
                    Vec3 rayHit = pickRay.pointAtDistance(pickRay.intersectWithPlane(side.normal, planeAnchor));
                    if (rayHit.nan()) {
                        // in 2D views the pick ray will be perpendicular to the face normal.
                        // in that case, use a plane with a normal opposite the pickRay.
                        rayHit = pickRay.pointAtDistance(pickRay.intersectWithPlane(pickRay.direction * -1.0, planeAnchor));
                    }
                    assert(!rayHit.nan());
                    
                    std::cout << "make shear with rayHit: " << rayHit << "\n";

                    //m_dragOrigin = rayHit;
                    
                    Vec3 delta = rayHit - m_dragOrigin;
                    delta = grid.snap(delta);
                    
                    if (camera.perspectiveProjection()) {
                        if (vertical) {
                            delta[0] = 0;
                            delta[1] = 0;
                        } else {
                            delta[2] = 0;
                        }
                    } else if (camera.orthographicProjection()) {
                        const Plane3 cameraPlane(0.0, camera.direction());
                        delta = cameraPlane.projectVector(delta);
                    } else {
                        assert(0);
                    }

                    if (!delta.null()) {
                        std::cout << "make shear with m_dragOrigin: " << m_dragOrigin << "\n";

                        std::cout << "make shear with delta: " << delta << "on side" << side.normal << "\n";
                        if (document->shearObjects(bounds(), side.normal, delta)) {
                            // only used to tell whether to commit the shear
                            m_totalDelta += delta;
                            
                            // update the ref point for the next iteration
                            m_dragOrigin = rayHit;
                        }
                    }
                }
                
            }
            
            
            return true;
        }
        
        void ScaleObjectsTool::commitResize() {
            MapDocumentSPtr document = lock(m_document);
            if (m_totalDelta.null())
                document->cancelTransaction();
            else
                document->commitTransaction();
//            m_dragFaces.clear();
            m_resizing = false;
            m_isShearing = false;
            m_isProportional = false;
        }
        
        void ScaleObjectsTool::cancelResize() {
            MapDocumentSPtr document = lock(m_document);
            document->cancelTransaction();
//            m_dragFaces.clear();
            m_resizing = false;
            m_isShearing = false;
            m_isProportional = false;
        }
        
        void ScaleObjectsTool::bindObservers() {
            MapDocumentSPtr document = lock(m_document);
            document->nodesWereAddedNotifier.addObserver(this, &ScaleObjectsTool::nodesDidChange);
            document->nodesWillChangeNotifier.addObserver(this, &ScaleObjectsTool::nodesDidChange);
            document->nodesWillBeRemovedNotifier.addObserver(this, &ScaleObjectsTool::nodesDidChange);
            document->selectionDidChangeNotifier.addObserver(this, &ScaleObjectsTool::selectionDidChange);
        }
        
        void ScaleObjectsTool::unbindObservers() {
            if (!expired(m_document)) {
                MapDocumentSPtr document = lock(m_document);
                document->nodesWereAddedNotifier.removeObserver(this, &ScaleObjectsTool::nodesDidChange);
                document->nodesWillChangeNotifier.removeObserver(this, &ScaleObjectsTool::nodesDidChange);
                document->nodesWillBeRemovedNotifier.removeObserver(this, &ScaleObjectsTool::nodesDidChange);
                document->selectionDidChangeNotifier.removeObserver(this, &ScaleObjectsTool::selectionDidChange);
            }
        }
        
        void ScaleObjectsTool::nodesDidChange(const Model::NodeList& nodes) {
//            if (!m_resizing)
//                m_dragFaces.clear();
        }
        
        void ScaleObjectsTool::selectionDidChange(const Selection& selection) {
//            if (!m_resizing)
//                m_dragFaces.clear();
        }

        wxWindow* ScaleObjectsTool::doCreatePage(wxWindow* parent) {
            assert(m_toolPage == nullptr);
            m_toolPage = new ScaleObjectsToolPage(parent, m_document);
            return m_toolPage;
        }
    }
}
