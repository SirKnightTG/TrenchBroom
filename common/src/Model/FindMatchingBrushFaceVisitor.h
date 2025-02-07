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

#ifndef TrenchBroom_FindMatchingBrushFaceVisitor
#define TrenchBroom_FindMatchingBrushFaceVisitor

#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/ModelTypes.h"
#include "Model/NodeVisitor.h"

namespace TrenchBroom {
    namespace Model {
        template <typename P>
        class FindMatchingBrushFaceVisitor : public NodeVisitor, public NodeQuery<BrushFace*> {
        private:
            P m_p;
        public:
            FindMatchingBrushFaceVisitor(const P& p = P()) : m_p(p) {}
        private:
            void doVisit(World* world)   override {}
            void doVisit(Layer* layer)   override {}
            void doVisit(Group* group)   override {}
            void doVisit(Entity* entity) override {}
            void doVisit(Brush* brush)   override {
                for (BrushFace* face : brush->faces()) {
                    if (m_p(face)) {
                        setResult(face);
                        cancel();
                        return;
                    }
                }
            }
        };
    }
}

#endif /* defined(TrenchBroom_FindMatchingBrushFaceVisitor) */
