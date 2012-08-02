/*
 Copyright (C) 2010-2012 Kristian Duske
 
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
 along with TrenchBroom.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MoveFaceTool.h"

#include "Controller/Grid.h"
#include "Model/Map/Brush.h"
#include "Model/Map/BrushGeometry.h"
#include "Model/Map/Face.h"
#include "Model/Map/Map.h"
#include "Model/Preferences.h"
#include "Model/Selection.h"
#include "Renderer/Figures/HandleFigure.h"
#include "Renderer/Figures/PointGuideFigure.h"

namespace TrenchBroom {
    namespace Controller {
        int MoveFaceTool::hitType() {
            return Model::TB_HT_FACE_HANDLE;
        }
        
        std::string MoveFaceTool::undoName() {
            return "Move Face";
        }
        
        Vec3f MoveFaceTool::movePosition(const Model::Brush& brush, size_t index) {
            return brush.geometry->edges[index]->center();
        }
        
        Vec3f MoveFaceTool::moveDelta(const Vec3f& position, const Vec3f& delta) {
            return editor().grid().moveDelta(delta);
        }
        
        Model::MoveResult MoveFaceTool::performMove(Model::Brush& brush, size_t index, const Vec3f& delta) {
            return editor().map().moveFace(brush, index, delta);
        }

        const Vec4f& MoveFaceTool::handleColor() {
            Model::Preferences& prefs = *Model::Preferences::sharedPreferences;
            return prefs.faceHandleColor();
        }
        
        const Vec4f& MoveFaceTool::hiddenHandleColor() {
            Model::Preferences& prefs = *Model::Preferences::sharedPreferences;
            return prefs.hiddenFaceHandleColor();
        }
        
        const Vec4f& MoveFaceTool::selectedHandleColor() {
            Model::Preferences& prefs = *Model::Preferences::sharedPreferences;
            return prefs.selectedFaceHandleColor();
        }
        
        const Vec4f& MoveFaceTool::hiddenSelectedHandleColor() {
            Model::Preferences& prefs = *Model::Preferences::sharedPreferences;
            return prefs.hiddenSelectedFaceHandleColor();
        }
        
        const Vec3fList MoveFaceTool::handlePositions() {
            Vec3fList positions;
            
            Model::Map& map = editor().map();
            Model::Selection& selection = map.selection();
            const Model::BrushList& brushes = selection.brushes();
            
            for (unsigned int i = 0; i < brushes.size(); i++) {
                Model::Brush* brush = brushes[i];
                for (unsigned int j = 0; j < brush->faces.size(); j++)
                    positions.push_back(brush->faces[j]->center());
            }
            
            return positions;
        }
        
        const Vec3fList MoveFaceTool::selectedHandlePositions() {
            Vec3fList positions;
            positions.push_back(draggedHandlePosition());
            return positions;
        }
        
        const Vec3f MoveFaceTool::draggedHandlePosition() {
            size_t index = VertexTool::index();
            Model::Face* face = brush()->faces[index];
            return face->center();
        }
    }
}
