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

#include "TrenchBroom.h"

#include "CreateSimpleBrushTool.h"
#include "Model/BrushBuilder.h"
#include "View/MapDocument.h"
#include "Model/World.h"

namespace TrenchBroom {
    namespace View {
        CreateSimpleBrushTool::CreateSimpleBrushTool(MapDocumentWPtr document) :
        CreateBrushToolBase(true, document) {}

        void CreateSimpleBrushTool::update(const vm::bbox3& bounds) {
            MapDocumentSPtr document = lock(m_document);
            const Model::BrushBuilder builder(document->world(), document->worldBounds());
            updateBrush(builder.createCuboid(bounds, document->currentTextureName()));
        }

    }
}
