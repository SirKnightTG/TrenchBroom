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

#ifndef TrenchBroom_ShearTexturesCommand
#define TrenchBroom_ShearTexturesCommand

#include "View/DocumentCommand.h"

#include <vecmath/forward.h>
#include <vecmath/vec.h>

namespace TrenchBroom {
    namespace View {
        class ShearTexturesCommand : public DocumentCommand {
        public:
            static const CommandType Type;
            using Ptr = std::shared_ptr<ShearTexturesCommand>;
        private:
            vm::vec2f m_factors;
        public:
            static Ptr shear(const vm::vec2f& factors);
        private:
            ShearTexturesCommand(const vm::vec2f& factors);

            bool doPerformDo(MapDocumentCommandFacade* document) override;
            bool doPerformUndo(MapDocumentCommandFacade* document) override;

            bool shearTextures(MapDocumentCommandFacade* document, const vm::vec2f& factors);

            bool doIsRepeatable(MapDocumentCommandFacade* document) const override;
            UndoableCommand::Ptr doRepeat(MapDocumentCommandFacade* document) const override;

            bool doCollateWith(UndoableCommand::Ptr command) override;
        private:
            ShearTexturesCommand(const ShearTexturesCommand& other);
            ShearTexturesCommand& operator=(const ShearTexturesCommand& other);
        };
    }
}

#endif /* defined(TrenchBroom_ShearTexturesCommand) */
