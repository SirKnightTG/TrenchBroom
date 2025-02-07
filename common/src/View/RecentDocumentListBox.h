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

#ifndef TrenchBroom_RecentDocumentListBox
#define TrenchBroom_RecentDocumentListBox

#include "View/ImageListBox.h"
#include "IO/Path.h"

#include <QPixmap>

namespace TrenchBroom {
    namespace IO {
        class Path;
    }

    namespace View {
        class RecentDocumentListBox : public ImageListBox {
            Q_OBJECT
        private:
            QPixmap m_documentIcon;
        public:
            explicit RecentDocumentListBox(QWidget* parent = nullptr);
            ~RecentDocumentListBox() override;
        private:
            void recentDocumentsDidChange();

            size_t itemCount() const override;
            QPixmap image(size_t index) const override;
            QString title(size_t index) const override;
            QString subtitle(size_t index) const override;

            void doubleClicked(size_t index) override;
        signals:
            void loadRecentDocument(const IO::Path& path);
        };
    }
}


#endif /* defined(TrenchBroom_RecentDocumentListBox) */
