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

#include "MoveObjectsTool.h"

#include "TrenchBroom.h"
#include "Model/Brush.h"
#include "View/Grid.h"
#include "View/InputState.h"
#include "View/MapDocument.h"
#include "View/MoveObjectsToolPage.h"

#include <vecmath/bbox.h>

#include <cassert>

namespace TrenchBroom {
    namespace View {
        MoveObjectsTool::MoveObjectsTool(MapDocumentWPtr document) :
        Tool(true),
        m_document(document),
        m_duplicateObjects(false) {}

        const Grid& MoveObjectsTool::grid() const {
            return lock(m_document)->grid();
        }

        bool MoveObjectsTool::startMove(const InputState& inputState) {
            auto document = lock(m_document);
            document->beginTransaction(duplicateObjects(inputState) ? "Duplicate Objects" : "Move Objects");
            m_duplicateObjects = duplicateObjects(inputState);
            return true;
        }

        MoveObjectsTool::MoveResult MoveObjectsTool::move(const InputState& inputState, const vm::vec3& delta) {
            auto document = lock(m_document);
            const auto& worldBounds = document->worldBounds();
            const auto bounds = document->selectionBounds();
            if (!worldBounds.contains(bounds.translate(delta))) {
                return MR_Deny;
            }

            if (m_duplicateObjects) {
                m_duplicateObjects = false;
                if (!document->duplicateObjects()) {
                    return MR_Cancel;
                }
            }

            if (!document->translateObjects(delta)) {
                return MR_Deny;
            } else {
                return MR_Continue;
            }
        }

        void MoveObjectsTool::endMove(const InputState& inputState) {
            auto document = lock(m_document);
            document->commitTransaction();
        }

        void MoveObjectsTool::cancelMove() {
            auto document = lock(m_document);
            document->cancelTransaction();
        }

        bool MoveObjectsTool::duplicateObjects(const InputState& inputState) const {
            return inputState.modifierKeysDown(ModifierKeys::MKCtrlCmd);
        }

        QWidget* MoveObjectsTool::doCreatePage(QWidget* parent) {
            return new MoveObjectsToolPage(m_document, parent);
        }
    }
}
