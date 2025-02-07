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

#include "TestUtils.h"

#include <vecmath/scalar.h>

#include <gmock/gmock.h>

#include "CollectionUtils.h"
#include "Model/Brush.h"
#include "Model/BrushFace.h"

namespace TrenchBroom {
    bool texCoordsEqual(const vm::vec2f& tc1, const vm::vec2f& tc2) {
        for (size_t i = 0; i < 2; ++i) {
            const float dist = vm::abs(tc1[i] - tc2[i]);
            const float distRemainder = dist - vm::floor(dist);

            if (!(vm::is_equal(distRemainder, 0.0f, vm::Cf::almost_zero()) || vm::is_equal(distRemainder, 1.0f,
                vm::Cf::almost_zero())))
                return false;
        }
        return true;
    }

    bool pointExactlyIntegral(const vm::vec3d &point) {
        for (size_t i=0; i<3; i++) {
            const double value = point[i];
            if (static_cast<double>(static_cast<int>(value)) != value) {
                return false;
            }
        }
        return true;
    }

    /**
     * Assumes the UV's have been divided by the texture size.
     */
    bool UVListsEqual(const std::vector<vm::vec2f>& uvs,
                      const std::vector<vm::vec2f>& transformedVertUVs) {
        if (uvs.size() != transformedVertUVs.size()) {
            return false;
        }
        if (uvs.size() < 3U) {
            return false;
        }
        if (!texCoordsEqual(uvs[0], transformedVertUVs[0])) {
            return false;
        }

        for (size_t i=1; i<uvs.size(); ++i) {
            // note, just checking:
            //   texCoordsEqual(uvs[i], transformedVertUVs[i]);
            // would be too lenient.
            const vm::vec2f expected = uvs[i] - uvs[0];
            const vm::vec2f actual = transformedVertUVs[i] - transformedVertUVs[0];
            if (!vm::is_equal(expected, actual, vm::Cf::almost_zero())) {
                return false;
            }
        }
        return true;
    }

    TEST(TestUtilsTest, testTexCoordsEqual) {
        ASSERT_TRUE(texCoordsEqual(vm::vec2f(0.0, 0.0), vm::vec2f(0.0, 0.0)));
        ASSERT_TRUE(texCoordsEqual(vm::vec2f(0.0, 0.0), vm::vec2f(1.0, 0.0)));
        ASSERT_TRUE(texCoordsEqual(vm::vec2f(0.0, 0.0), vm::vec2f(2.00001, 0.0)));
        ASSERT_TRUE(texCoordsEqual(vm::vec2f(0.0, 0.0), vm::vec2f(-10.0, 2.0)));
        ASSERT_TRUE(texCoordsEqual(vm::vec2f(2.0, -3.0), vm::vec2f(-10.0, 2.0)));
        ASSERT_TRUE(texCoordsEqual(vm::vec2f(-2.0, -3.0), vm::vec2f(-10.0, 2.0)));
        ASSERT_TRUE(texCoordsEqual(vm::vec2f(0.0, 0.0), vm::vec2f(-1.0, 1.0)));
        ASSERT_TRUE(texCoordsEqual(vm::vec2f(0.0, 0.0), vm::vec2f(-0.00001, 0.0)));
        ASSERT_TRUE(texCoordsEqual(vm::vec2f(0.25, 0.0), vm::vec2f(-0.75, 0.0)));

        ASSERT_FALSE(texCoordsEqual(vm::vec2f(0.0, 0.0), vm::vec2f(0.1, 0.1)));
        ASSERT_FALSE(texCoordsEqual(vm::vec2f(-0.25, 0.0), vm::vec2f(0.25, 0.0)));
    }

    TEST(TestUtilsTest, UVListsEqual) {
        EXPECT_TRUE(UVListsEqual({{0,0}, {1,0}, {0, 1}},  {{0,0}, {1,0}, {0, 1}}));
        EXPECT_TRUE(UVListsEqual({{0,0}, {1,0}, {0, 1}},  {{10,0}, {11,0}, {10, 1}})); // translation by whole texture increments OK

        EXPECT_FALSE(UVListsEqual({{0,0}, {1,0}, {0, 1}},  {{10.5,0}, {11.5,0}, {10.5, 1}})); // translation by partial texture increments not OK
        EXPECT_FALSE(UVListsEqual({{0,0}, {1,0}, {0, 1}},  {{0,0}, {0,1}, {1, 0}})); // wrong order
        EXPECT_FALSE(UVListsEqual({{0,0}, {1,0}, {0, 1}},  {{0,0}, {2,0}, {0, 2}})); // unwanted scaling
    }

    TEST(TestUtilsTest, pointExactlyIntegral) {
        ASSERT_TRUE(pointExactlyIntegral(vm::vec3d(0.0, 0.0, 0.0)));
        ASSERT_TRUE(pointExactlyIntegral(vm::vec3d(1024.0, 1204.0, 1024.0)));
        ASSERT_TRUE(pointExactlyIntegral(vm::vec3d(-10000.0, -10000.0, -10000.0)));

        const double near1024 = vm::nextgreater(1024.0);
        ASSERT_FALSE(pointExactlyIntegral(vm::vec3d(1024.0, near1024, 1024.0)));
        ASSERT_FALSE(pointExactlyIntegral(vm::vec3d(1024.5, 1024.5, 1024.5)));
    }

    namespace Model {
        void assertTexture(const String& expected, const Brush* brush, const vm::vec3& faceNormal) {
            assert(brush != nullptr);
            BrushFace* face = brush->findFace(faceNormal);
            assert(face != nullptr);

            ASSERT_EQ(expected, face->textureName());
        }

        void assertTexture(const String& expected, const Brush* brush, const vm::vec3d& v1, const vm::vec3d& v2, const vm::vec3d& v3) {
            return assertTexture(expected, brush, VectorUtils::create<vm::vec3d>(v1, v2, v3));
        }

        void assertTexture(const String& expected, const Brush* brush, const vm::vec3d& v1, const vm::vec3d& v2, const vm::vec3d& v3, const vm::vec3d& v4) {
            return assertTexture(expected, brush, VectorUtils::create<vm::vec3d>(v1, v2, v3, v4));
        }

        void assertTexture(const String& expected, const Brush* brush, const std::vector<vm::vec3d>& vertices) {
            return assertTexture(expected, brush, vm::polygon3d(vertices));
        }

        void assertTexture(const String& expected, const Brush* brush, const vm::polygon3d& vertices) {
            assert(brush != nullptr);
            BrushFace* face = brush->findFace(vertices, 0.0001);
            assert(face != nullptr);

            ASSERT_EQ(expected, face->textureName());
        }
    }
}
