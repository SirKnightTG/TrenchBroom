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

#ifndef TrenchBroom_SmartSpawnflagsEditor
#define TrenchBroom_SmartSpawnflagsEditor

#include "Assets/AssetTypes.h"
#include "Model/ModelTypes.h"
#include "View/SmartAttributeEditor.h"
#include "View/ViewTypes.h"

#include <QPoint>

class QString;
class QWidget;
class QScrollArea;

namespace TrenchBroom {
    namespace View {
        class FlagsEditor;
        class FlagChangedCommand;

        class SmartSpawnflagsEditor : public SmartAttributeEditor {
            Q_OBJECT
        private:
            static const size_t NumFlags = 24;
            static const size_t NumCols = 3;

            class UpdateSpawnflag;

            QScrollArea* m_scrolledWindow;
            QPoint m_lastScrollPos;
            FlagsEditor* m_flagsEditor;
            bool m_ignoreUpdates;
        public:
            explicit SmartSpawnflagsEditor(View::MapDocumentWPtr document, QWidget* parent = nullptr);
        private:
            void createGui();
            void doUpdateVisual(const Model::AttributableNodeList& attributables) override;
            void resetScrollPos();

            void getFlags(const Model::AttributableNodeList& attributables, QStringList& labels, QStringList& tooltips) const;
            void getFlagValues(const Model::AttributableNodeList& attributables, int& setFlags, int& mixedFlags) const;
            int getFlagValue(const Model::AttributableNode* attributable) const;

            void flagChanged(size_t index, int setFlag, int mixedFlag);
        };
    }
}

#endif /* defined(TrenchBroom_SmartSpawnflagsEditor) */
