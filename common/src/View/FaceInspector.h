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

#ifndef TrenchBroom_FaceInspector
#define TrenchBroom_FaceInspector

#include "View/TabBook.h"
#include "View/ViewTypes.h"

class QSplitter;
class QWidget;

namespace TrenchBroom {
    namespace Assets {
        class Texture;
    }

    namespace Model {
        class BrushFace;
        class Object;
        class SelectionResult;
    }

    namespace View {
        class FaceAttribsEditor;
        class GLContextManager;
        class TextureBrowser;
        class FileTextureCollectionEditor;

        class FaceInspector : public TabBookPage {
            Q_OBJECT
        private:
            MapDocumentWPtr m_document;
            QSplitter* m_splitter;
            FaceAttribsEditor* m_faceAttribsEditor;
            TextureBrowser* m_textureBrowser;
        public:
            FaceInspector(MapDocumentWPtr document, GLContextManager& contextManager, QWidget* parent = nullptr);
            ~FaceInspector() override;

            bool cancelMouseDrag();
        private:
            void createGui(MapDocumentWPtr document, GLContextManager& contextManager);
            QWidget* createFaceAttribsEditor(QWidget* parent, MapDocumentWPtr document, GLContextManager& contextManager);
            QWidget* createTextureBrowser(QWidget* parent, MapDocumentWPtr document, GLContextManager& contextManager);
            QWidget* createTextureCollectionEditor(QWidget* parent, MapDocumentWPtr document);

            void textureSelected(Assets::Texture* texture);
        };
    }
}

#endif /* defined(TrenchBroom_FaceInspector) */
