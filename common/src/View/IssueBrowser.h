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

#ifndef TrenchBroom_IssueBrowser
#define TrenchBroom_IssueBrowser

#include "Model/ModelTypes.h"
#include "View/ViewTypes.h"
#include "View/TabBook.h"

class QCheckBox;
class QStackedLayout;
class QWidget;

namespace TrenchBroom {
    namespace Model {
        class Issue;
    }

    namespace View {
        class FlagChangedCommand;
        class FlagsPopupEditor;
        class IssueBrowserView;

        class IssueBrowser : public TabBookPage {
            Q_OBJECT
        private:
            static const int SelectObjectsCommandId = 1;
            static const int ShowIssuesCommandId = 2;
            static const int HideIssuesCommandId = 3;
            static const int FixObjectsBaseId = 4;

            MapDocumentWPtr m_document;
            IssueBrowserView* m_view;
            QCheckBox* m_showHiddenIssuesCheckBox;
            FlagsPopupEditor* m_filterEditor;
        public:
            explicit IssueBrowser(MapDocumentWPtr document, QWidget* parent = nullptr);
            ~IssueBrowser() override;

            QWidget* createTabBarPage(QWidget* parent) override;
        private:
            void bindObservers();
            void unbindObservers();
            void documentWasNewedOrLoaded(MapDocument* document);
            void documentWasSaved(MapDocument* document);
            void nodesWereAdded(const Model::NodeList& nodes);
            void nodesWereRemoved(const Model::NodeList& nodes);
            void nodesDidChange(const Model::NodeList& nodes);
            void brushFacesDidChange(const Model::BrushFaceList& faces);
            void issueIgnoreChanged(Model::Issue* issue);

            void updateFilterFlags();

            void showHiddenIssuesChanged();
            void filterChanged(size_t index, int setFlag, int mixedFlag);
        };
    }
}

#endif /* defined(TrenchBroom_IssueBrowser) */
