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

#ifndef TrenchBroom_IssueBrowserView
#define TrenchBroom_IssueBrowserView

#include "View/ViewTypes.h"

#include "Model/ModelTypes.h"

#include <vector>

#include <QWidget>
#include <QAbstractItemModel>

class QWidget;
class QTableView;

namespace TrenchBroom {
    namespace View {
        class IssueBrowserModel;

        class IssueBrowserView : public QWidget {
            Q_OBJECT
        private:
            MapDocumentWPtr m_document;

            Model::IssueType m_hiddenGenerators;
            bool m_showHiddenIssues;

            bool m_valid;

            QTableView* m_tableView;
            IssueBrowserModel* m_tableModel;
        public:
            explicit IssueBrowserView(MapDocumentWPtr document, QWidget* parent = nullptr);

        private:
            void createGui();

        public:
            int hiddenGenerators() const;
            void setHiddenGenerators(int hiddenGenerators);
            void setShowHiddenIssues(bool show);
            void reload();
            void deselectAll();
        private:
            class IssueVisible;
            class IssueCmp;

            void updateIssues();

            Model::IssueList collectIssues(const QList<QModelIndex>& indices) const;
            Model::IssueQuickFixList collectQuickFixes(const QList<QModelIndex>& indices) const;
            Model::IssueType issueTypeMask() const;

            void setIssueVisibility(bool show);

            QList<QModelIndex> getSelection() const;
            void updateSelection();
            void bindEvents();

            void itemRightClicked(const QPoint& pos);
            void itemSelectionChanged();
            void showIssues();
            void hideIssues();
            void applyQuickFix(const Model::IssueQuickFix* quickFix);
        private:
            void invalidate();
        public slots:
            void validate();
        };

        /**
         * Trivial QAbstractTableModel subclass, when the issues list changes,
         * it just refreshes the entire list with beginResetModel()/endResetModel().
         */
        class IssueBrowserModel : public QAbstractTableModel {
            Q_OBJECT
        private:
            Model::IssueList m_issues;
        public:
            explicit IssueBrowserModel(QObject* parent);

            void setIssues(Model::IssueList issues);
            const Model::IssueList& issues();
        public: // QAbstractTableModel overrides
            int rowCount(const QModelIndex& parent) const override;
            int columnCount(const QModelIndex& parent) const override;
            QVariant data(const QModelIndex& index, int role) const override;
            QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        };
    }
}

#endif /* defined(TrenchBroom_IssueBrowserView) */
