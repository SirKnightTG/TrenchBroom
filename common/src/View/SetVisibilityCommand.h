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

#ifndef TrenchBroom_SetVisibilityCommand
#define TrenchBroom_SetVisibilityCommand

#include "Model/ModelTypes.h"
#include "View/UndoableCommand.h"

#include <map>

namespace TrenchBroom {
    namespace View {
        class SetVisibilityCommand : public UndoableCommand {
        public:
            static const CommandType Type;
            using Ptr = std::shared_ptr<SetVisibilityCommand>;
        private:
            typedef enum {
                Action_Reset,
                Action_Hide,
                Action_Show,
                Action_Ensure
            } Action;

            Model::NodeList m_nodes;
            CommandType m_action;
            Model::VisibilityMap m_oldState;
        public:
            static Ptr show(const Model::NodeList& nodes);
            static Ptr hide(const Model::NodeList& nodes);
            static Ptr ensureVisible(const Model::NodeList& nodes);
            static Ptr reset(const Model::NodeList& nodes);
        private:
            SetVisibilityCommand(const Model::NodeList& nodes, Action action);
            static String makeName(Action action);
        private:
            bool doPerformDo(MapDocumentCommandFacade* document) override;
            bool doPerformUndo(MapDocumentCommandFacade* document) override;

            bool doCollateWith(UndoableCommand::Ptr command) override;
            bool doIsRepeatable(MapDocumentCommandFacade* document) const override;
        };
    }
}

#endif /* defined(TrenchBroom_SetVisibilityCommand) */
