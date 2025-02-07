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

#include "AABBTree.h"
#include "IO/DiskIO.h"
#include "IO/File.h"
#include "IO/Path.h"
#include "IO/TestParserStatus.h"
#include "IO/WorldReader.h"
#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/Entity.h"
#include "Model/NodeVisitor.h"
#include "Model/World.h"

#include <vecmath/bbox.h>

namespace TrenchBroom {
    namespace Model {
        using AABB = AABBTree<double, 3, Node*>;
        using BOX = AABB::Box;

        class TreeBuilder : public NodeVisitor {
        private:
            AABB& m_tree;
            vm::bbox3 m_bounds;
        public:
            TreeBuilder(AABB& tree) : m_tree(tree) {}
        private:
            void doVisit(World* world) override {}
            void doVisit(Layer* layer) override {}
            void doVisit(Group* group) override {}
            void doVisit(Entity* entity) override {
                doInsert(entity);
            }
            void doVisit(Brush* brush) override {
                doInsert(brush);
            }

            void doInsert(Node* node) {
                if (!m_tree.empty()) {
                    const auto oldBounds = m_tree.bounds();

                    m_tree.insert(node->physicalBounds(), node);
                    m_bounds = merge(m_bounds, node->physicalBounds());

                    if (!m_tree.bounds().contains(oldBounds)) {
                        cancel();
                        ASSERT_TRUE(m_tree.bounds().contains(oldBounds)) << "Node at line " << node->lineNumber() << " decreased tree bounds: " << oldBounds << " -> " << m_tree.bounds();
                    }
                } else {
                    m_tree.insert(node->physicalBounds(), node);
                    m_bounds = node->physicalBounds();
                }

                if (!m_tree.contains(node)) {
                    cancel();
                    m_tree.print(std::cout);
                    ASSERT_TRUE(m_tree.contains(node)) << "Node " << node << " with bounds " << node->physicalBounds() << " at line " << node->lineNumber() << " not found in tree after insertion";
                }

                if (m_bounds != m_tree.bounds()) {
                    cancel();
                    ASSERT_TRUE(m_bounds == m_tree.bounds()) << "Node at line " << node->lineNumber() << " mangled tree bounds";
                }
            }
        };

        TEST(AABBTreeStressTest, parseMapTest) {
            const auto mapPath = IO::Disk::getCurrentWorkingDir() + IO::Path("fixture/test/IO/Map/rtz_q1.map");
            const auto file = IO::Disk::openFile(mapPath);
            auto fileReader = file->reader().buffer();

            IO::TestParserStatus status;
            IO::WorldReader worldReader(std::begin(fileReader), std::end(fileReader));

            const auto worldBounds = vm::bbox3(8192.0);
            auto world = worldReader.read(Model::MapFormat::Standard, worldBounds, status);

            AABB tree;
            TreeBuilder builder(tree);
            world->acceptAndRecurse(builder);
        }
    }
}

