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

#ifndef TrenchBroom_AddRemoveNodesCommand
#define TrenchBroom_AddRemoveNodesCommand

#include "Model/ModelTypes.h"
#include "View/DocumentCommand.h"

namespace TrenchBroom {
    namespace View {
        class AddRemoveNodesCommand : public DocumentCommand {
        public:
            static const CommandType Type;
            using Ptr = std::shared_ptr<AddRemoveNodesCommand>;
        private:
            typedef enum {
                Action_Add,
                Action_Remove
            } Action;

            Action m_action;
            Model::ParentChildrenMap m_nodesToAdd;
            Model::ParentChildrenMap m_nodesToRemove;
        public:
            static Ptr add(Model::Node* parent, const Model::NodeList& children);
            static Ptr add(const Model::ParentChildrenMap& nodes);
            static Ptr remove(const Model::ParentChildrenMap& nodes);
            ~AddRemoveNodesCommand() override;
        private:
            AddRemoveNodesCommand(Action action, const Model::ParentChildrenMap& nodes);
            static String makeName(Action action);

            bool doPerformDo(MapDocumentCommandFacade* document) override;
            bool doPerformUndo(MapDocumentCommandFacade* document) override;

            bool doIsRepeatable(MapDocumentCommandFacade* document) const override;

            bool doCollateWith(UndoableCommand::Ptr command) override;
        };
    }
}

#endif /* defined(TrenchBroom_AddRemoveNodesCommand) */
