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

#ifndef TrenchBroom_RenderBatch
#define TrenchBroom_RenderBatch

#include <list>

namespace TrenchBroom {
    namespace Renderer {
        class Renderable;
        class DirectRenderable;
        class IndexedRenderable;
        class RenderContext;
        class Vbo;

        class RenderBatch {
        private:
            Vbo& m_vertexVbo;
            Vbo& m_indexVbo;

            class IndexedRenderableWrapper;

            using RenderableList = std::list<Renderable*>;
            using DirectRenderableList = std::list<DirectRenderable*>;
            using IndexedRenderableList = std::list<IndexedRenderable*>;

            DirectRenderableList m_directRenderables;
            IndexedRenderableList m_indexedRenderables;

            RenderableList m_batch;
            RenderableList m_oneshots;
        public:
            RenderBatch(Vbo& vertexVbo, Vbo& indexVbo);
            ~RenderBatch();

            void add(Renderable* renderable);
            void add(DirectRenderable* renderable);
            void add(IndexedRenderable* renderable);

            /**
             * Same as `add()`, but takes ownership of the given renderable and deletes it in
             * `~RenderBatch`.
             */
            void addOneShot(Renderable* renderable);
            void addOneShot(DirectRenderable* renderable);
            void addOneShot(IndexedRenderable* renderable);

            void render(RenderContext& renderContext);
        private:
            void doAdd(Renderable* renderable);

            void prepareRenderables();

            void renderRenderables(RenderContext& renderContext);
        };
    }
}

#endif /* defined(TrenchBroom_RenderBatch) */
