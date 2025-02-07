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

#ifndef TrenchBroom_EntityAttributeGrid
#define TrenchBroom_EntityAttributeGrid

#include "Model/ModelTypes.h"
#include "View/ViewTypes.h"

#include <QWidget>

class QTableView;
class QCheckBox;
class QAbstractButton;
class QShortcut;

namespace TrenchBroom {
    namespace View {
        class EntityAttributeModel;
        class Selection;

        /**
         * Panel with the entity attribute table, and the toolbar below it (add/remove icons,
         * "show default properties" checkbox, etc.)
         */
        class EntityAttributeGrid : public QWidget {
            Q_OBJECT
        private:
            MapDocumentWPtr m_document;

            EntityAttributeModel* m_model;
            QTableView* m_table;
            QAbstractButton* m_addAttributeButton;
            QAbstractButton* m_removePropertiesButton;
            QCheckBox* m_showDefaultPropertiesCheckBox;

            QShortcut* m_insertRowShortcut;
            QShortcut* m_removeRowShortcut;
            QShortcut* m_removeRowAlternateShortcut;
            QShortcut* m_openCellEditorShortcut;

            bool m_ignoreSelection;
        public:
            explicit EntityAttributeGrid(MapDocumentWPtr document, QWidget* parent = nullptr);
            ~EntityAttributeGrid() override;
        private:
//            void OnAttributeGridSize(wxSizeEvent& event);
//            void OnAttributeGridSelectCell(wxGridEvent& event);
//            void OnAttributeGridTab(wxGridEvent& event);
        public:
//            void tabNavigate(int row, int col, bool forward);
        private:
//            void moveCursorTo(int row, int col);
//            void fireSelectionEvent(int row, int col);
        private:
            void addAttribute();
            void removeSelectedAttributes();
            void removeAttribute(const String& key);

            bool canRemoveSelectedAttributes() const;
            std::set<int> selectedRowsAndCursorRow() const;
        private:
            void createGui(MapDocumentWPtr document);
            void createShortcuts();
            void updateShortcuts();

            void bindObservers();
            void unbindObservers();

            void documentWasNewed(MapDocument* document);
            void documentWasLoaded(MapDocument* document);
            void nodesDidChange(const Model::NodeList& nodes);
            void selectionWillChange();
            void selectionDidChange(const Selection& selection);
        private:
            void updateControls();
        public:
            Model::AttributeName selectedRowName() const;
        signals:
            void selectedRow();
        };
    }
}

#endif /* defined(TrenchBroom_EntityAttributeGrid) */
