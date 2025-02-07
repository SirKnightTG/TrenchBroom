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

#ifndef IndexRangeBuilder_h
#define IndexRangeBuilder_h

#include "Renderer/IndexRangeMap.h"
#include "Renderer/VertexListBuilder.h"

namespace TrenchBroom {
    namespace Renderer {
        /**
         * Builds an index range map and a corresponding vertex array by recording rendering primitives.
         * The recorded data can be used to create an vertex array that can be uploaded to video card memory, and to
         * render the recorded primitives using the index ranges stored in the constructed index range map.
         */
        template <typename VertexSpec>
        class IndexRangeMapBuilder {
        public:
            using Vertex = typename VertexSpec::Vertex;
            using VertexList = typename Vertex::List;
            using IndexData = typename VertexListBuilder<VertexSpec>::IndexData;
        private:
            VertexListBuilder<VertexSpec> m_vertexListBuilder;
            IndexRangeMap m_indexRange;
        public:
            /**
             * Creates a new index range map builder that can grow dynamically to account for the recorded primitives.
             * Be aware that dynamic growth my incur a performance cost as buffers are reallocated when their
             * capacity is exhausted.
             */
            IndexRangeMapBuilder() = default; // default constructors allow dynamic growth

            /**
             * Creates a new index range map builder that initializes its data structures to the given sizes.
             *
             * @param vertexCount the total number of vertices to expect
             * @param indexRangeSize the size of the index range map to expect
             */
            IndexRangeMapBuilder(const size_t vertexCount, const IndexRangeMap::Size& indexRangeSize) :
            m_vertexListBuilder(vertexCount),
            m_indexRange(indexRangeSize) {}

            /**
             * Returns the recorded vertices.
             *
             * @return the recorded vertices
             */
            const VertexList& vertices() const {
                return m_vertexListBuilder.vertices();
            }

            /**
             * Returns the recorded vertices.
             *
             * @return the recorded vertices
             */
            VertexList& vertices() {
                return m_vertexListBuilder.vertices();
            }

            /**
             * Returns the recorded index ranges.
             *
             * @return the recorded index ranges
             */
            const IndexRangeMap& indices() const {
                return m_indexRange;
            }

            /**
             * Returns the recorded index ranges.
             *
             * @return the recorded index ranges
             */
            IndexRangeMap& indices() {
                return m_indexRange;
            }

            /**
             * Adds a point primitive at the given position.
             *
             * @param v the position of the point to add
             */
            void addPoint(const Vertex& v) {
                add(GL_POINTS, m_vertexListBuilder.addPoint(v));
            }

            /**
             * Adds multiple point primitives at the given positions.
             *
             * @param vertices the positions of the points to add
             */
            void addPoints(const VertexList& vertices) {
                add(GL_POINTS, m_vertexListBuilder.addPoints(vertices));
            }

            /**
             * Adds a line with the given end points.
             *
             * @param v1 the position of the first end point
             * @param v2 the position of the second end point
             */
            void addLine(const Vertex& v1, const Vertex& v2) {
                add(GL_LINES, m_vertexListBuilder.addLine(v1, v2));
            }

            /**
             * Adds multiple lines with the given endpoints. Each line to be added consists of two consecutive of the
             * given list, so for each line, two elements of the list are used.
             *
             * @param vertices the end points of the lines to add
             */
            void addLines(const VertexList& vertices) {
                add(GL_LINES, m_vertexListBuilder.addLines(vertices));
            }

            /**
             * Adds a line strip with the given points.
             *
             * @param vertices the end points of the lines to add
             */
            void addLineStrip(const VertexList& vertices) {
                add(GL_LINE_STRIP, m_vertexListBuilder.addLineStrip(vertices));
            }

            /**
             * Adds a line loop with the given points.
             *
             * @param vertices the end points of the lines to add
             */
            void addLineLoop(const VertexList& vertices) {
                add(GL_LINE_LOOP, m_vertexListBuilder.addLineLoop(vertices));
            }

            /**
             * Adds a triangle with the given corners.
             *
             * @param v1 the position of the first corner
             * @param v2 the position of the second corner
             * @param v3 the position of the third corner
             */
            void addTriangle(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
                add(GL_TRIANGLES, m_vertexListBuilder.addTriangle(v1, v2, v3));
            }

            /**
             * Adds multiple triangles using the corner positions in the given list. For each triangle, three positions
             * are used.
             *
             * @param vertices the corner positions
             */
            void addTriangles(const VertexList& vertices) {
                add(GL_TRIANGLES, m_vertexListBuilder.addTriangles(vertices));
            }

            /**
             * Adds a triangle fan using the positions of the vertices in the given list.
             *
             * @param vertices the vertex positions
             */
            void addTriangleFan(const VertexList& vertices) {
                add(GL_TRIANGLE_FAN, m_vertexListBuilder.addTriangleFan(vertices));
            }

            /**
             * Adds a triangle strip using the positions of the vertices in the given list.
             *
             * @param vertices the vertex positions
             */
            void addTriangleStrip(const VertexList& vertices) {
                add(GL_TRIANGLE_STRIP, m_vertexListBuilder.addTriangleStrip(vertices));
            }

            /**
             * Adds a quad with the given corners.
             *
             * @param v1 the position of the first corner
             * @param v2 the position of the second corner
             * @param v3 the position of the third corner
             * @param v4 the position of the fourth corner
             */
            void addQuad(const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex& v4) {
                add(GL_QUADS, m_vertexListBuilder.addQuad(v1, v2, v3, v4));
            }

            /**
             * Adds multiple quads using the corner positions in the given list. For each quad, four positions
             * are used.
             *
             * @param vertices the corner positions
             */
            void addQuads(const VertexList& vertices) {
                add(GL_QUADS, m_vertexListBuilder.addQuads(vertices));
            }

            /**
             * Adds a quad strip using the positions of the vertices in the given list.
             *
             * @param vertices the vertex positions
             */
            void addQuadStrip(const VertexList& vertices) {
                add(GL_QUAD_STRIP, m_vertexListBuilder.addQuadStrip(vertices));
            }

            /**
             * Adds a polygon with the given corners.
             *
             * @param vertices the croner positions
             */
            void addPolygon(const VertexList& vertices) {
                add(GL_POLYGON, m_vertexListBuilder.addPolygon(vertices));
            }
        private:
            void add(const PrimType primType, const IndexData& data) {
                m_indexRange.add(primType, data.index, data.count);
            }
        };
    }
}

#endif /* IndexRangeBuilder_h */
