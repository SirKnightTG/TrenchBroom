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

#ifndef TrenchBroom_DkmParser
#define TrenchBroom_DkmParser

#include "StringType.h"
#include "Assets/EntityModel.h"
#include "IO/EntityModelParser.h"

#include <vector>

#include <vecmath/forward.h>
#include <vecmath/vec.h>

namespace TrenchBroom {
    namespace Assets {
        class EntityModel;
        class Palette;
    }

    namespace IO {
        class Reader;
        class FileSystem;
        class Path;

        namespace DkmLayout {
            static const int Ident = (('D'<<24) + ('M'<<16) + ('K'<<8) + 'D');
            static const int Version1 = 1;
            static const int Version2 = 2;
            static const size_t SkinNameLength = 64;
            static const size_t FrameNameLength = 16;
        }

        // see http://tfc.duke.free.fr/coding/md2-specs-en.html
        class DkmParser : public EntityModelParser {
        private:
            static const vm::vec3f Normals[162];

            using DkmSkinList = StringList;

            struct DkmVertex {
                unsigned int x, y, z;
                unsigned char normalIndex;
            };

            using DkmVertexList = std::vector<DkmVertex>;

            struct DkmFrame {
                vm::vec3f scale;
                vm::vec3f offset;
                String name;
                DkmVertexList vertices;

                explicit DkmFrame(size_t vertexCount);
                vm::vec3f vertex(size_t index) const;
                const vm::vec3f& normal(size_t index) const;
            };

            struct DkmMeshVertex {
                vm::vec2f texCoords;
                size_t vertexIndex;
            };
            using DkmMeshVertexList = std::vector<DkmMeshVertex>;

            struct DkmMesh {
                enum Type {
                    Fan,
                    Strip
                };

                Type type;
                size_t vertexCount;
                DkmMeshVertexList vertices;

                explicit DkmMesh(int i_vertexCount);
            };
            using DkmMeshList = std::vector<DkmMesh>;


            String m_name;
            const char* m_begin;
            const char* m_end;
            const FileSystem& m_fs;
        public:
            DkmParser(const String& name, const char* begin, const char* end, const FileSystem& fs);
        private:
            std::unique_ptr<Assets::EntityModel> doInitializeModel(Logger& logger) override;
            void doLoadFrame(size_t frameIndex, Assets::EntityModel& model, Logger& logger) override;

            DkmSkinList parseSkins(Reader reader, size_t skinCount);
            DkmFrame parseFrame(Reader reader, size_t frameIndex, size_t vertexCount, int version);
            DkmMeshList parseMeshes(Reader reader, size_t commandCount);

            void loadSkins(Assets::EntityModel::Surface& surface, const DkmSkinList& skins);
            const IO::Path findSkin(const String& skin) const;

            void buildFrame(Assets::EntityModel& model, Assets::EntityModel::Surface& surface, size_t frameIndex, const DkmFrame& frame, const DkmMeshList& meshes);
            Assets::EntityModel::VertexList getVertices(const DkmFrame& frame, const DkmMeshVertexList& meshVertices) const;
        };
    }
}

#endif /* defined(TrenchBroom_DkmParser) */
