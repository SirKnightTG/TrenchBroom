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

#include <gtest/gtest.h>

#include "IO/TestParserStatus.h"
#include "IO/WorldReader.h"
#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/Entity.h"
#include "Model/Layer.h"
#include "Model/World.h"

#include <vecmath/vec.h>

namespace TrenchBroom {
    namespace IO {
        inline Model::BrushFace*
        findFaceByPoints(const Model::BrushFaceList& faces, const vm::vec3& point0, const vm::vec3& point1,
                         const vm::vec3& point2) {
            for (Model::BrushFace* face : faces) {
                if (face->points()[0] == point0 &&
                    face->points()[1] == point1 &&
                    face->points()[2] == point2)
                    return face;
            }
            return nullptr;
        }

        TEST(WorldReaderTest, parseFailure_1424) {
            const String data(R"(
{
"classname" "worldspawn"
"message" "yay"
{
( 0 0 0 ) ( 0 0 0 ) ( 0 0 0 ) __TB_empty -56 -72 -0 1 1
( 1320 512 152 ) ( 1280 512 192 ) ( 1320 504 152 ) grill_wall03b_h -0 -72 -0 1 1
( 1344 512 160 ) ( 1280 512 224 ) ( 1320 512 152 ) grill_wall03b_h -56 -72 -0 1 1
( 1320 512 152 ) ( 1320 504 152 ) ( 1344 512 160 ) grill_wall03b_h -56 -0 -0 1 1
( 0 0 0 ) ( 0 0 0 ) ( 0 0 0 ) __TB_empty -0 -72 -0 1 1
( 1320 504 152 ) ( 1280 505.37931034482756 197.51724137931035 ) ( 1344 512 160 ) grill_wall03b_h -56 -72 -0 1 1
}
})");

            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);
            ASSERT_TRUE(world != nullptr);
        }

        TEST(WorldReaderTest, parseEmptyMap) {
            const String data("");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_TRUE(world != nullptr);
            ASSERT_EQ(1u, world->childCount());
            ASSERT_FALSE(world->children().front()->hasChildren());
        }

        TEST(WorldReaderTest, parseMapWithEmptyEntity) {
            const String data("{}");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_TRUE(world != nullptr);
            ASSERT_EQ(1u, world->childCount());
            ASSERT_EQ(1u, world->children().front()->childCount());
        }

        TEST(WorldReaderTest, parseMapWithWorldspawn) {
            const String data(R"(
{
"classname" "worldspawn"
"message" "yay"
}
)");

            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_TRUE(world != nullptr);
            ASSERT_EQ(1u, world->childCount());
            ASSERT_FALSE(world->children().front()->hasChildren());

            ASSERT_TRUE(world->hasAttribute(Model::AttributeNames::Classname));
            ASSERT_STREQ("yay", world->attribute("message").c_str());
        }

        TEST(WorldReaderTest, parseMapWithWorldspawnAndOneMoreEntity) {
            const String data(R"(
{
"classname" "worldspawn"
"message" "yay"
}
{
"classname" "info_player_deathmatch"
"origin" "1 22 -3"
"angle" " -1 "
}
)");

            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_TRUE(world != nullptr);
            ASSERT_TRUE(world->hasAttribute(Model::AttributeNames::Classname));
            ASSERT_STREQ("yay", world->attribute("message").c_str());

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());

            Model::Entity* entity = static_cast<Model::Entity*>(defaultLayer->children().front());
            ASSERT_TRUE(entity->hasAttribute("classname"));
            ASSERT_STREQ("info_player_deathmatch", entity->attribute("classname").c_str());
            ASSERT_TRUE(entity->hasAttribute("origin"));
            ASSERT_STREQ("1 22 -3", entity->attribute("origin").c_str());
            ASSERT_TRUE(entity->hasAttribute("angle"));
            ASSERT_STREQ(" -1 ", entity->attribute("angle").c_str());
        }

        TEST(WorldReaderTest, parseMapWithWorldspawnAndOneBrush) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -0 -0 -16 ) ( -0 -0  -0 ) ( 64 -0 -16 ) tex1 1 2 3 4 5
( -0 -0 -16 ) ( -0 64 -16 ) ( -0 -0  -0 ) tex2 0 0 0 1 1
( -0 -0 -16 ) ( 64 -0 -16 ) ( -0 64 -16 ) tex3 0 0 0 1 1
( 64 64  -0 ) ( -0 64  -0 ) ( 64 64 -16 ) tex4 0 0 0 1 1
( 64 64  -0 ) ( 64 64 -16 ) ( 64 -0  -0 ) tex5 0 0 0 1 1
( 64 64  -0 ) ( 64 -0  -0 ) ( -0 64  -0 ) tex6 0 0 0 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());

            Model::Brush* brush = static_cast<Model::Brush*>(defaultLayer->children().front());
            const Model::BrushFaceList& faces = brush->faces();
            ASSERT_EQ(6u, faces.size());

            const Model::BrushFace* face1 = findFaceByPoints(faces, vm::vec3(0.0, 0.0, -16.0), vm::vec3(0.0, 0.0, 0.0),
                                                             vm::vec3(64.0, 0.0, -16.0));
            ASSERT_TRUE(face1 != nullptr);
            ASSERT_STREQ("tex1", face1->textureName().c_str());
            ASSERT_FLOAT_EQ(1.0, face1->xOffset());
            ASSERT_FLOAT_EQ(2.0, face1->yOffset());
            ASSERT_FLOAT_EQ(3.0, face1->rotation());
            ASSERT_FLOAT_EQ(4.0, face1->xScale());
            ASSERT_FLOAT_EQ(5.0, face1->yScale());

            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(0.0, 0.0, -16.0), vm::vec3(0.0, 64.0, -16.0),
                                         vm::vec3(0.0, 0.0, 0.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(0.0, 0.0, -16.0), vm::vec3(64.0, 0.0, -16.0),
                                         vm::vec3(0.0, 64.0, -16.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(64.0, 64.0, 0.0), vm::vec3(0.0, 64.0, 0.0),
                                         vm::vec3(64.0, 64.0, -16.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(64.0, 64.0, 0.0), vm::vec3(64.0, 64.0, -16.0),
                                         vm::vec3(64.0, 0.0, 0.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(64.0, 64.0, 0.0), vm::vec3(64.0, 0.0, 0.0),
                                         vm::vec3(0.0, 64.0, 0.0)) != nullptr);
        }

        TEST(WorldReaderTest, parseMapAndCheckFaceFlags) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -0 -0 -16 ) ( -0 -0  -0 ) ( 64 -0 -16 ) none 22 -3 56.2 1.03433 -0.55
( -0 -0 -16 ) ( -0 64 -16 ) ( -0 -0  -0 ) none 0 0 0 1 1
( -0 -0 -16 ) ( 64 -0 -16 ) ( -0 64 -16 ) none 0 0 0 1 1
( 64 64  -0 ) ( -0 64  -0 ) ( 64 64 -16 ) none 0 0 0 1 1
( 64 64  -0 ) ( 64 64 -16 ) ( 64 -0  -0 ) none 0 0 0 1 1
( 64 64  -0 ) ( 64 -0  -0 ) ( -0 64  -0 ) none 0 0 0 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());

            Model::Brush* brush = static_cast<Model::Brush*>(defaultLayer->children().front());
            const Model::BrushFaceList& faces = brush->faces();
            ASSERT_EQ(6u, faces.size());

            Model::BrushFace* face = findFaceByPoints(faces, vm::vec3(0.0, 0.0, -16.0), vm::vec3(0.0, 0.0, 0.0),
                                                      vm::vec3(64.0, 0.0, -16.0));
            ASSERT_TRUE(face != nullptr);
            ASSERT_FLOAT_EQ(22.0f, face->xOffset());
            ASSERT_FLOAT_EQ(22.0f, face->xOffset());
            ASSERT_FLOAT_EQ(56.2f, face->rotation());
            ASSERT_FLOAT_EQ(1.03433f, face->xScale());
            ASSERT_FLOAT_EQ(-0.55f, face->yScale());
        }

        TEST(WorldReaderTest, parseBrushWithCurlyBraceInTextureName) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -0 -0 -16 ) ( -0 -0  -0 ) ( 64 -0 -16 ) {none 0 0 0 1 1
( -0 -0 -16 ) ( -0 64 -16 ) ( -0 -0  -0 ) none 0 0 0 1 1
( -0 -0 -16 ) ( 64 -0 -16 ) ( -0 64 -16 ) none 0 0 0 1 1
( 64 64  -0 ) ( -0 64  -0 ) ( 64 64 -16 ) none 0 0 0 1 1
( 64 64  -0 ) ( 64 64 -16 ) ( 64 -0  -0 ) none 0 0 0 1 1
( 64 64  -0 ) ( 64 -0  -0 ) ( -0 64  -0 ) none 0 0 0 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());

            Model::Brush* brush = static_cast<Model::Brush*>(defaultLayer->children().front());
            const Model::BrushFaceList faces = brush->faces();
            ASSERT_EQ(6u, faces.size());

            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(0.0, 0.0, -16.0), vm::vec3(0.0, 0.0, 0.0),
                                         vm::vec3(64.0, 0.0, -16.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(0.0, 0.0, -16.0), vm::vec3(0.0, 64.0, -16.0),
                                         vm::vec3(0.0, 0.0, 0.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(0.0, 0.0, -16.0), vm::vec3(64.0, 0.0, -16.0),
                                         vm::vec3(0.0, 64.0, -16.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(64.0, 64.0, 0.0), vm::vec3(0.0, 64.0, 0.0),
                                         vm::vec3(64.0, 64.0, -16.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(64.0, 64.0, 0.0), vm::vec3(64.0, 64.0, -16.0),
                                         vm::vec3(64.0, 0.0, 0.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(64.0, 64.0, 0.0), vm::vec3(64.0, 0.0, 0.0),
                                         vm::vec3(0.0, 64.0, 0.0)) != nullptr);
        }

        TEST(WorldReaderTest, parseProblematicBrush1) {
            const String data(R"(
{
"classname" "worldspawn"
{
( 308 108 176 ) ( 308 132 176 ) ( 252 132 176 ) mt_sr_v13 -59 13 -90 1 1
( 252 132 208 ) ( 308 132 208 ) ( 308 108 208 ) mt_sr_v13 -59 13 -90 1 1
( 288 152 176 ) ( 288 152 208 ) ( 288 120 208 ) mt_sr_v13 -59 -110 -180 1 1
( 288 122 176 ) ( 288 122 208 ) ( 308 102 208 ) mt_sr_v13 -37 -111 -180 1 1
( 308 100 176 ) ( 308 100 208 ) ( 324 116 208 ) mt_sr_v13 -100 -111 0 1 -1
( 287 152 208 ) ( 287 152 176 ) ( 323 116 176 ) mt_sr_v13 -65 -111 -180 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());

            Model::Brush* brush = static_cast<Model::Brush*>(defaultLayer->children().front());
            const Model::BrushFaceList faces = brush->faces();
            ASSERT_EQ(6u, faces.size());
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(308.0, 108.0, 176.0), vm::vec3(308.0, 132.0, 176.0),
                                         vm::vec3(252.0, 132.0, 176.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(252.0, 132.0, 208.0), vm::vec3(308.0, 132.0, 208.0),
                                         vm::vec3(308.0, 108.0, 208.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(288.0, 152.0, 176.0), vm::vec3(288.0, 152.0, 208.0),
                                         vm::vec3(288.0, 120.0, 208.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(288.0, 122.0, 176.0), vm::vec3(288.0, 122.0, 208.0),
                                         vm::vec3(308.0, 102.0, 208.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(308.0, 100.0, 176.0), vm::vec3(308.0, 100.0, 208.0),
                                         vm::vec3(324.0, 116.0, 208.0)) != nullptr);
            ASSERT_TRUE(findFaceByPoints(faces, vm::vec3(287.0, 152.0, 208.0), vm::vec3(287.0, 152.0, 176.0),
                                         vm::vec3(323.0, 116.0, 176.0)) != nullptr);
        }

        TEST(WorldReaderTest, parseProblematicBrush2) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -572 1078 128 ) ( -594 1088 128 ) ( -597 1072 96 ) mt_sr_v16 -64 0 -180 1 -1
( -572 1078 160 ) ( -572 1078 128 ) ( -590 1051 128 ) b_rc_v4 32 0 90 1 1
( -601 1056 160 ) ( -601 1056 128 ) ( -594 1088 128 ) b_rc_v4 32 0 90 1 1
( -590 1051 160 ) ( -590 1051 128 ) ( -601 1056 128 ) b_rc_v4 32 -16 90 1 1
( -512 1051 128 ) ( -624 1051 128 ) ( -568 1088 128 ) b_rc_v4 0 -16 90 1 1
( -559 1090 96 ) ( -598 1090 96 ) ( -598 1055 96 ) mt_sr_v13 -16 0 0 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());
        }

        TEST(WorldReaderTest, parseProblematicBrush3) {
            const String data(R"(
{
"classname" "worldspawn"
{
( 256 1152 -96 ) ( 160 1152 -96 ) ( 160 1120 -96 ) b_rc_v4 31 -31 90 1 1
( -64 1120 64 ) ( -64 1184 64 ) ( -32 1184 32 ) b_rc_v4 31 -31 90 1 1
( -112 1120 32 ) ( 224 1120 32 ) ( 224 1120 -96 ) b_rc_v4 0 0 90 1 1
( -112 1184 -96 ) ( 264 1184 -96 ) ( 264 1184 32 ) b_rc_v4 -127 -32 90 1 1
( -64 1184 64 ) ( -64 1120 64 ) ( -64 1120 -96 ) b_rc_v4 -127 32 90 1 1
( -32 1136 32 ) ( -32 1152 -96 ) ( -32 1120 -96 ) b_rc_v4 0 32 90 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());
        }

        TEST(WorldReaderTest, parseValveBrush) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -800 288 1024 ) ( -736 288 1024 ) ( -736 224 1024 ) METAL4_5 [ 1 0 0 64 ] [ 0 -1 0 0 ] 0 1 1
( -800 288 1024 ) ( -800 224 1024 ) ( -800 224 576 ) METAL4_5 [ 0 1 0 0 ] [ 0 0 -1 0 ] 0 1 1
( -736 224 1024 ) ( -736 288 1024 ) ( -736 288 576 ) METAL4_5 [ 0 1 0 0 ] [ 0 0 -1 0 ] 0 1 1
( -736 288 1024 ) ( -800 288 1024 ) ( -800 288 576 ) METAL4_5 [ 1 0 0 64 ] [ 0 0 -1 0 ] 0 1 1
( -800 224 1024 ) ( -736 224 1024 ) ( -736 224 576 ) METAL4_5 [ 1 0 0 64 ] [ 0 0 -1 0 ] 0 1 1
( -800 224 576 ) ( -736 224 576 ) ( -736 288 576 ) METAL4_5 [ 1 0 0 64 ] [ 0 -1 0 0 ] 0 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Valve, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());
        }

        TEST(WorldReaderTest, parseQuake2Brush) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -712 1280 -448 ) ( -904 1280 -448 ) ( -904 992 -448 ) rtz/c_mf_v3c 56 -32 0 1 1 0 0 0
( -904 992 -416 ) ( -904 1280 -416 ) ( -712 1280 -416 ) rtz/b_rc_v16w 32 32 0 1 1
( -832 968 -416 ) ( -832 1256 -416 ) ( -832 1256 -448 ) rtz/c_mf_v3c 16 96 0 1 1
( -920 1088 -448 ) ( -920 1088 -416 ) ( -680 1088 -416 ) rtz/c_mf_v3c 56 96 0 1 1 0 0 0
( -968 1152 -448 ) ( -920 1152 -448 ) ( -944 1152 -416 ) rtz/c_mf_v3c 56 96 0 1 1 0 0 0
( -896 1056 -416 ) ( -896 1056 -448 ) ( -896 1344 -448 ) rtz/c_mf_v3c 16 96 0 1 1 0 0 0
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Quake2, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());
        }

        TEST(WorldReaderTest, parseDaikatanaBrush) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -712 1280 -448 ) ( -904 1280 -448 ) ( -904 992 -448 ) rtz/c_mf_v3cw 56 -32 0 1 1 0 0 0 5 6 7
( -904 992 -416 ) ( -904 1280 -416 ) ( -712 1280 -416 ) rtz/b_rc_v16w 32 32 0 1 1 1 2 3 8 9 10
( -832 968 -416 ) ( -832 1256 -416 ) ( -832 1256 -448 ) rtz/c_mf_v3cww 16 96 0 1 1
( -920 1088 -448 ) ( -920 1088 -416 ) ( -680 1088 -416 ) rtz/c_mf_v3c 56 96 0 1 1 0 0 0
( -968 1152 -448 ) ( -920 1152 -448 ) ( -944 1152 -416 ) rtz/c_mf_v3c 56 96 0 1 1 0 0 0
( -896 1056 -416 ) ( -896 1056 -448 ) ( -896 1344 -448 ) rtz/c_mf_v3c 16 96 0 1 1 0 0 0
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Daikatana, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());

            const auto* brush = static_cast<Model::Brush*>(defaultLayer->children().front());
            ASSERT_TRUE(vm::is_equal(Color(5, 6, 7), brush->findFace("rtz/c_mf_v3cw")->color(), 0.1f));
            ASSERT_EQ(1, brush->findFace("rtz/b_rc_v16w")->surfaceContents());
            ASSERT_EQ(2, brush->findFace("rtz/b_rc_v16w")->surfaceFlags());
            ASSERT_FLOAT_EQ(3.0, brush->findFace("rtz/b_rc_v16w")->surfaceValue());
            ASSERT_TRUE(vm::is_equal(Color(8, 9, 10), brush->findFace("rtz/b_rc_v16w")->color(), 0.1f));
            ASSERT_FALSE(brush->findFace("rtz/c_mf_v3cww")->hasColor());
        }

        TEST(WorldReaderTest, parseDaikatanaMapHeader) {
            const String data(R"(
////////////////////////////////////////////////////////////
// ldef 000 "Base Brush Layer"
////////////////////////////////////////////////////////////
{
"angle" "0"
"mapname" "Plague Poundings"
"cloud2speed" "2"
"lightningfreq" "1"
"classname" "worldspawn"
"sky" "e3m1"
"palette" "e3m1"
"episode" "3"
"ambient" "5"
"cloudname" "mtntile"
"musictrack" "E3C"
// brush 0  layer 000
{
( 1024 1520 0 ) ( 864 1520 160 ) ( 864 1728 160 ) e3m1/thatch2sno 49 0 90 1 1 134217728 16384 0
( 960 1488 48 ) ( 1008 1488 0 ) ( 1008 1872 0 ) e3m1/roof03 -83 45 -180 1 1 134217728 1024 0
( 1008 2152 -48 ) ( 1024 2152 -48 ) ( 944 2152 80 ) e3m1/rooftrim 32 13 135 1 -0.500000 134217728 0 0
( 944 1536 72 ) ( 944 1792 64 ) ( 944 1792 80 ) e3m1/rooftrim 32 -31 133 0.999905 -0.499926 134217728 0 0
( 1024 2144 -48 ) ( 1008 2144 -48 ) ( 1032 2120 -24 ) e3m1/rooftrim -18 -26 -135 0.999873 -0.499936 134217728 0 0
( 968 2120 -48 ) ( 944 2120 -48 ) ( 956 2120 80 ) e3m1/rooftrim -18 -26 -135 0.999873 -0.499936 134217728 0 0
}
}
)");

            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Daikatana, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());
        }

        TEST(WorldReaderTest, parseQuakeBrushWithNumericalTextureName) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -712 1280 -448 ) ( -904 1280 -448 ) ( -904 992 -448 ) c_mf_v3c 56 -32 0 1 1
( -904 992 -416 ) ( -904 1280 -416 ) ( -712 1280 -416 ) b_rc_v16w 32 32 0 1 1
( -832 968 -416 ) ( -832 1256 -416 ) ( -832 1256 -448 ) 666 16 96 0 1 1
( -920 1088 -448 ) ( -920 1088 -416 ) ( -680 1088 -416 ) c_mf_v3c 56 96 0 1 1
( -968 1152 -448 ) ( -920 1152 -448 ) ( -944 1152 -416 ) c_mf_v3c 56 96 0 1 1
( -896 1056 -416 ) ( -896 1056 -448 ) ( -896 1344 -448 ) c_mf_v3c 16 96 0 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());
            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(1u, defaultLayer->childCount());
        }

        TEST(WorldReaderTest, parseBrushesWithLayer) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -0 -0 -16 ) ( -0 -0  -0 ) ( 64 -0 -16 ) none 0 0 0 1 1
( -0 -0 -16 ) ( -0 64 -16 ) ( -0 -0  -0 ) none 0 0 0 1 1
( -0 -0 -16 ) ( 64 -0 -16 ) ( -0 64 -16 ) none 0 0 0 1 1
( 64 64  -0 ) ( -0 64  -0 ) ( 64 64 -16 ) none 0 0 0 1 1
( 64 64  -0 ) ( 64 64 -16 ) ( 64 -0  -0 ) none 0 0 0 1 1
( 64 64  -0 ) ( 64 -0  -0 ) ( -0 64  -0 ) none 0 0 0 1 1
}
{
( -712 1280 -448 ) ( -904 1280 -448 ) ( -904 992 -448 ) rtz/c_mf_v3c 56 -32 0 1 1
( -904 992 -416 ) ( -904 1280 -416 ) ( -712 1280 -416 ) rtz/b_rc_v16w 32 32 0 1 1
( -832 968 -416 ) ( -832 1256 -416 ) ( -832 1256 -448 ) rtz/c_mf_v3c 16 96 0 1 1
( -920 1088 -448 ) ( -920 1088 -416 ) ( -680 1088 -416 ) rtz/c_mf_v3c 56 96 0 1 1
( -968 1152 -448 ) ( -920 1152 -448 ) ( -944 1152 -416 ) rtz/c_mf_v3c 56 96 0 1 1
( -896 1056 -416 ) ( -896 1056 -448 ) ( -896 1344 -448 ) rtz/c_mf_v3c 16 96 0 1 1
}
}
{
"classname" "func_group"
"_tb_type" "_tb_layer"
"_tb_name" "My Layer"
"_tb_id" "1"
{
( -800 288 1024 ) ( -736 288 1024 ) ( -736 224 1024 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 288 1024 ) ( -800 224 1024 ) ( -800 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 224 1024 ) ( -736 288 1024 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 288 1024 ) ( -800 288 1024 ) ( -800 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 1024 ) ( -736 224 1024 ) ( -736 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 576 ) ( -736 224 576 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Quake2, worldBounds, status);

            ASSERT_EQ(2u, world->childCount());
            ASSERT_EQ(2u, world->children().front()->childCount());
            ASSERT_EQ(1u, world->children().back()->childCount());
        }

        TEST(WorldReaderTest, parseEntitiesAndBrushesWithLayer) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -0 -0 -16 ) ( -0 -0  -0 ) ( 64 -0 -16 ) none 0 0 0 1 1
( -0 -0 -16 ) ( -0 64 -16 ) ( -0 -0  -0 ) none 0 0 0 1 1
( -0 -0 -16 ) ( 64 -0 -16 ) ( -0 64 -16 ) none 0 0 0 1 1
( 64 64  -0 ) ( -0 64  -0 ) ( 64 64 -16 ) none 0 0 0 1 1
( 64 64  -0 ) ( 64 64 -16 ) ( 64 -0  -0 ) none 0 0 0 1 1
( 64 64  -0 ) ( 64 -0  -0 ) ( -0 64  -0 ) none 0 0 0 1 1
}
{
( -712 1280 -448 ) ( -904 1280 -448 ) ( -904 992 -448 ) rtz/c_mf_v3c 56 -32 0 1 1
( -904 992 -416 ) ( -904 1280 -416 ) ( -712 1280 -416 ) rtz/b_rc_v16w 32 32 0 1 1
( -832 968 -416 ) ( -832 1256 -416 ) ( -832 1256 -448 ) rtz/c_mf_v3c 16 96 0 1 1
( -920 1088 -448 ) ( -920 1088 -416 ) ( -680 1088 -416 ) rtz/c_mf_v3c 56 96 0 1 1
( -968 1152 -448 ) ( -920 1152 -448 ) ( -944 1152 -416 ) rtz/c_mf_v3c 56 96 0 1 1
( -896 1056 -416 ) ( -896 1056 -448 ) ( -896 1344 -448 ) rtz/c_mf_v3c 16 96 0 1 1
}
}
{
"classname" "func_group"
"_tb_type" "_tb_layer"
"_tb_name" "My Layer"
"_tb_id" "1"
{
( -800 288 1024 ) ( -736 288 1024 ) ( -736 224 1024 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 288 1024 ) ( -800 224 1024 ) ( -800 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 224 1024 ) ( -736 288 1024 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 288 1024 ) ( -800 288 1024 ) ( -800 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 1024 ) ( -736 224 1024 ) ( -736 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 576 ) ( -736 224 576 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
}
}
{
"classname" "func_door"
"_tb_layer" "1"
{
( -800 288 1024 ) ( -736 288 1024 ) ( -736 224 1024 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 288 1024 ) ( -800 224 1024 ) ( -800 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 224 1024 ) ( -736 288 1024 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 288 1024 ) ( -800 288 1024 ) ( -800 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 1024 ) ( -736 224 1024 ) ( -736 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 576 ) ( -736 224 576 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Quake2, worldBounds, status);

            ASSERT_EQ(2u, world->childCount());
            ASSERT_EQ(2u, world->children().front()->childCount()); // default layer
            ASSERT_EQ(2u, world->children().back()->childCount()); // My Layer
            ASSERT_EQ(1u, world->children().back()->children().back()->childCount());
        }

        TEST(WorldReaderTest, parseEntitiesAndBrushesWithGroup) {
            const String data(R"(
{
"classname" "worldspawn"
{
( -0 -0 -16 ) ( -0 -0  -0 ) ( 64 -0 -16 ) none 0 0 0 1 1
( -0 -0 -16 ) ( -0 64 -16 ) ( -0 -0  -0 ) none 0 0 0 1 1
( -0 -0 -16 ) ( 64 -0 -16 ) ( -0 64 -16 ) none 0 0 0 1 1
( 64 64  -0 ) ( -0 64  -0 ) ( 64 64 -16 ) none 0 0 0 1 1
( 64 64  -0 ) ( 64 64 -16 ) ( 64 -0  -0 ) none 0 0 0 1 1
( 64 64  -0 ) ( 64 -0  -0 ) ( -0 64  -0 ) none 0 0 0 1 1
}
{
( -712 1280 -448 ) ( -904 1280 -448 ) ( -904 992 -448 ) rtz/c_mf_v3c 56 -32 0 1 1
( -904 992 -416 ) ( -904 1280 -416 ) ( -712 1280 -416 ) rtz/b_rc_v16w 32 32 0 1 1
( -832 968 -416 ) ( -832 1256 -416 ) ( -832 1256 -448 ) rtz/c_mf_v3c 16 96 0 1 1
( -920 1088 -448 ) ( -920 1088 -416 ) ( -680 1088 -416 ) rtz/c_mf_v3c 56 96 0 1 1
( -968 1152 -448 ) ( -920 1152 -448 ) ( -944 1152 -416 ) rtz/c_mf_v3c 56 96 0 1 1
( -896 1056 -416 ) ( -896 1056 -448 ) ( -896 1344 -448 ) rtz/c_mf_v3c 16 96 0 1 1
}
}
{
"classname" "func_group"
"_tb_type" "_tb_group"
"_tb_name" "My Group"
"_tb_id" "1"
{
( -800 288 1024 ) ( -736 288 1024 ) ( -736 224 1024 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 288 1024 ) ( -800 224 1024 ) ( -800 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 224 1024 ) ( -736 288 1024 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 288 1024 ) ( -800 288 1024 ) ( -800 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 1024 ) ( -736 224 1024 ) ( -736 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 576 ) ( -736 224 576 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
}
}
{
"classname" "func_door"
"_tb_group" "1"
{
( -800 288 1024 ) ( -736 288 1024 ) ( -736 224 1024 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 288 1024 ) ( -800 224 1024 ) ( -800 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 224 1024 ) ( -736 288 1024 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 288 1024 ) ( -800 288 1024 ) ( -800 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 1024 ) ( -736 224 1024 ) ( -736 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 576 ) ( -736 224 576 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
}
}
{
"classname" "func_group"
"_tb_type" "_tb_group"
"_tb_name" "My Subroup"
"_tb_id" "2"
"_tb_group" "1"
{
( -800 288 1024 ) ( -736 288 1024 ) ( -736 224 1024 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 288 1024 ) ( -800 224 1024 ) ( -800 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 224 1024 ) ( -736 288 1024 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -736 288 1024 ) ( -800 288 1024 ) ( -800 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 1024 ) ( -736 224 1024 ) ( -736 224 576 ) rtz/c_mf_v3c 56 -32 0 1 1
( -800 224 576 ) ( -736 224 576 ) ( -736 288 576 ) rtz/c_mf_v3c 56 -32 0 1 1
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Quake2, worldBounds, status);

            ASSERT_EQ(1u, world->childCount());

            Model::Node* defaultLayer = world->children().front();
            ASSERT_EQ(3u, defaultLayer->childCount());

            Model::Node* myGroup = defaultLayer->children().back();
            ASSERT_EQ(3u, myGroup->childCount());

            Model::Node* mySubGroup = myGroup->children().back();
            ASSERT_EQ(1u, mySubGroup->childCount());
        }

        TEST(WorldReaderTest, parseBrushPrimitive) {
            const String data(R"(
            {
                "classname" "worldspawn"
                {
                    brushDef
                    {
                        ( -64 64 64 ) ( 64 -64 64 ) ( -64 -64 64 ) ( ( 0.015625 0 -0 ) ( -0 0.015625 0 ) ) common/caulk 0 0 0
                        ( -64 64 64 ) ( 64 64 -64 ) ( 64 64 64 ) ( ( 0.015625 0 0 ) ( 0 0.015625 0 ) ) common/caulk 0 0 0
                        ( 64 64 64 ) ( 64 -64 -64 ) ( 64 -64 64 ) ( ( 0.015625 0 -0 ) ( -0 0.015625 0 ) ) common/caulk 0 0 0
                        ( 64 64 -64 ) ( -64 -64 -64 ) ( 64 -64 -64 ) ( ( 0.015625 0 -0 ) ( -0 0.015625 0 ) ) common/caulk 0 0 0
                        ( 64 -64 -64 ) ( -64 -64 64 ) ( 64 -64 64 ) ( ( 0.015625 0 -0 ) ( -0 0.015625 0 ) ) common/caulk 0 0 0
                        ( -64 -64 64 ) ( -64 64 -64 ) ( -64 64 64 ) ( ( 0.015625 0 -0 ) ( -0 0.015625 0 ) ) common/caulk 0 0 0
                    }
                }
            })");

            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Quake3, worldBounds, status);

            // TODO 2427: Assert one brush!
            ASSERT_EQ(0u, world->defaultLayer()->childCount());
        }

        TEST(WorldReaderTest, parseBrushPrimitiveAndLegacyBrush) {
            const String data(R"(
{
"classname" "worldspawn"
{
brushDef
{
( -64 64 64 ) ( 64 -64 64 ) ( -64 -64 64 ) ( ( 0.015625 0 -0 ) ( -0 0.015625 0 ) ) common/caulk 0 0 0
( -64 64 64 ) ( 64 64 -64 ) ( 64 64 64 ) ( ( 0.015625 0 0 ) ( 0 0.015625 0 ) ) common/caulk 0 0 0
( 64 64 64 ) ( 64 -64 -64 ) ( 64 -64 64 ) ( ( 0.015625 0 -0 ) ( -0 0.015625 0 ) ) common/caulk 0 0 0
( 64 64 -64 ) ( -64 -64 -64 ) ( 64 -64 -64 ) ( ( 0.015625 0 -0 ) ( -0 0.015625 0 ) ) common/caulk 0 0 0
( 64 -64 -64 ) ( -64 -64 64 ) ( 64 -64 64 ) ( ( 0.015625 0 -0 ) ( -0 0.015625 0 ) ) common/caulk 0 0 0
( -64 -64 64 ) ( -64 64 -64 ) ( -64 64 64 ) ( ( 0.015625 0 -0 ) ( -0 0.015625 0 ) ) common/caulk 0 0 0
}
}
{
( 64 64 64 ) ( 64 -64 64 ) ( -64 64 64 ) common/caulk 0 0 0 1 1 134217728 0 0
( 64 64 64 ) ( -64 64 64 ) ( 64 64 -64 ) common/caulk 0 0 0 1 1 134217728 0 0
( 64 64 64 ) ( 64 64 -64 ) ( 64 -64 64 ) common/caulk 0 0 0 1 1 134217728 0 0
( -64 -64 -64 ) ( 64 -64 -64 ) ( -64 64 -64 ) common/caulk 0 0 0 1 1 134217728 0 0
( -64 -64 -64 ) ( -64 -64 64 ) ( 64 -64 -64 ) common/caulk 0 0 0 1 1 134217728 0 0
( -64 -64 -64 ) ( -64 64 -64 ) ( -64 -64 64 ) common/caulk 0 0 0 1 1 134217728 0 0
}
})");

            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Quake3, worldBounds, status);

            // TODO 2427: Assert two brushes!
            ASSERT_EQ(1u, world->defaultLayer()->childCount());
        }

        TEST(WorldReaderTest, parseQuake3Patch) {
            const String data(R"(
{
"classname" "worldspawn"
{
patchDef2
{
common/caulk
( 3 3 0 0 0 )
(
( ( -64 -64 4 0 0 ) ( -64 0 4 0 -0.25 ) ( -64 64 4 0 -0.5 ) )
( ( 0 -64 4 0.25 0 ) ( 0 0 4 0.25 -0.25 ) ( 0 64 4 0.25 -0.5 ) )
( ( 64 -64 4 0.5 0 ) ( 64 0 4 0.5 -0.25 ) ( 64 64 4 0.5 -0.5 ) )
)
}
}
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Quake3, worldBounds, status);

            // TODO 2428: Assert one patch!
            ASSERT_EQ(0u, world->defaultLayer()->childCount());
        }

        TEST(WorldReaderTest, parseMultipleClassnames) {
            // See https://github.com/kduske/TrenchBroom/issues/1485

            const String data(R"(
{
"classname" "worldspawn"
"classname" "worldspawn"
})");

            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            ASSERT_NO_THROW(reader.read(Model::MapFormat::Quake2, worldBounds, status));
        }

        TEST(WorldReaderTest, parseEscapedDoubleQuotationMarks) {
            const String data(R"(
{
"classname" "worldspawn"
"message" "yay \"Mr. Robot!\""
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_TRUE(world != nullptr);
            ASSERT_EQ(1u, world->childCount());
            ASSERT_FALSE(world->children().front()->hasChildren());

            ASSERT_TRUE(world->hasAttribute(Model::AttributeNames::Classname));
            ASSERT_STREQ("yay \\\"Mr. Robot!\\\"", world->attribute("message").c_str());
        }

        TEST(WorldReaderTest, parseAttributeWithUnescapedPathAndTrailingBackslash) {
            const String data(R"(
{
"classname" "worldspawn"
"path" "c:\a\b\c\"
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_TRUE(world != nullptr);
            ASSERT_EQ(1u, world->childCount());
            ASSERT_FALSE(world->children().front()->hasChildren());

            ASSERT_TRUE(world->hasAttribute(Model::AttributeNames::Classname));
            ASSERT_STREQ("c:\\a\\b\\c\\", world->attribute("path").c_str());
        }

        TEST(WorldReaderTest, parseAttributeWithEscapedPathAndTrailingBackslash) {
            const String data(R"(
{
"classname" "worldspawn"
"path" "c:\\a\\b\\c\\"
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_TRUE(world != nullptr);
            ASSERT_EQ(1u, world->childCount());
            ASSERT_FALSE(world->children().front()->hasChildren());

            ASSERT_TRUE(world->hasAttribute(Model::AttributeNames::Classname));
            ASSERT_STREQ("c:\\\\a\\\\b\\\\c\\\\", world->attribute("path").c_str());
        }

        TEST(WorldReaderTest, parseAttributeTrailingEscapedBackslash) {
            const String data(R"(
{
"classname" "worldspawn"
"message" "test\\"
})"
            );
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_TRUE(world != nullptr);
            ASSERT_EQ(1u, world->childCount());
            ASSERT_FALSE(world->children().front()->hasChildren());

            ASSERT_TRUE(world->hasAttribute(Model::AttributeNames::Classname));
            ASSERT_STREQ("test\\\\", world->attribute("message").c_str());
        }

        // https://github.com/kduske/TrenchBroom/issues/1739
        TEST(WorldReaderTest, parseAttributeNewlineEscapeSequence) {
            const String data(R"(
{
"classname" "worldspawn"
"message" "vm::line1\nvm::line2"
})");
            const vm::bbox3 worldBounds(8192.0);

            IO::TestParserStatus status;
            WorldReader reader(data);

            auto world = reader.read(Model::MapFormat::Standard, worldBounds, status);

            ASSERT_TRUE(world != nullptr);
            ASSERT_EQ(1u, world->childCount());
            ASSERT_FALSE(world->children().front()->hasChildren());

            ASSERT_TRUE(world->hasAttribute(Model::AttributeNames::Classname));
            ASSERT_STREQ("vm::line1\\nvm::line2", world->attribute("message").c_str());
        }

        /*
        TEST(WorldReaderTest, parseIssueIgnoreFlags) {
            const String data("{"
                              "\"classname\" \"worldspawn\""
                              "{\n"
                              "/// hideIssues 2\n"
                              "( -0 -0 -16 ) ( -0 -0  -0 ) ( 64 -0 -16 ) none 0 0 0 1 1\n"
                              "( -0 -0 -16 ) ( -0 64 -16 ) ( -0 -0  -0 ) none 0 0 0 1 1\n"
                              "( -0 -0 -16 ) ( 64 -0 -16 ) ( -0 64 -16 ) none 0 0 0 1 1\n"
                              "( 64 64  -0 ) ( -0 64  -0 ) ( 64 64 -16 ) none 0 0 0 1 1\n"
                              "( 64 64  -0 ) ( 64 64 -16 ) ( 64 -0  -0 ) none 0 0 0 1 1\n"
                              "( 64 64  -0 ) ( 64 -0  -0 ) ( -0 64  -0 ) none 0 0 0 1 1\n"
                              "}\n"
                              "}"
                              "{"
                              "/// hideIssues 3\n"
                              "\"classname\" \"info_player_deathmatch\""
                              "\"origin\" \"1 22 -3\""
                              "\"angle\" \" -1 \""
                              "}");
            vm::bbox3 worldBounds(-8192, 8192);

            using namespace testing;
            Model::MockGameSPtr game = Model::MockGame::newGame();
            EXPECT_CALL(*game, doBrushContentTypes()).WillOnce(ReturnRef(Model::BrushContentType::EmptyList));

            StandardMapParser parser(data, game.get());
            Model::Map* map = parser.parseMap(worldBounds);

            const Model::EntityList& entities = map->entities();
            ASSERT_EQ(2u, entities.size());

            const Model::Entity* firstEntity = entities[0];
            ASSERT_EQ(0u, firstEntity->hiddenIssues());

            const Model::BrushList& brushes = firstEntity->brushes();
            ASSERT_EQ(1u, brushes.size());

            const Model::Brush* brush = brushes[0];
            ASSERT_EQ(2u, brush->hiddenIssues());

            const Model::Entity* secondEntity = entities[1];
            ASSERT_EQ(3u, secondEntity->hiddenIssues());
        }
         */
    }
}
