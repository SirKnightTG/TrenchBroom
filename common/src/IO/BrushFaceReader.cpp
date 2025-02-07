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

#include "BrushFaceReader.h"

#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/Entity.h"
#include "Model/Layer.h"
#include "Model/ModelFactory.h"
#include "Model/World.h"

namespace TrenchBroom {
    namespace IO {
        BrushFaceReader::BrushFaceReader(const String& str, Model::ModelFactory& factory) :
        MapReader(str),
        m_factory(factory) {}

        const Model::BrushFaceList& BrushFaceReader::read(const vm::bbox3& worldBounds, ParserStatus& status) {
            try {
                readBrushFaces(m_factory.format(), worldBounds, status);
                return m_brushFaces;
            } catch (const ParserException&) {
                VectorUtils::clearAndDelete(m_brushFaces);
                throw;
            }
        }

        Model::ModelFactory& BrushFaceReader::initialize(const Model::MapFormat format, const vm::bbox3& worldBounds) {
            assert(format == m_factory.format());
            return m_factory;
        }

        Model::Node* BrushFaceReader::onWorldspawn(const Model::EntityAttribute::List& attributes, const ExtraAttributes& extraAttributes, ParserStatus& status) { return nullptr; }
        void BrushFaceReader::onWorldspawnFilePosition(const size_t lineNumber, const size_t lineCount, ParserStatus& status) {}
        void BrushFaceReader::onLayer(Model::Layer* layer, ParserStatus& status) {}
        void BrushFaceReader::onNode(Model::Node* parent, Model::Node* node, ParserStatus& status) {}
        void BrushFaceReader::onUnresolvedNode(const ParentInfo& parentInfo, Model::Node* node, ParserStatus& status) {}
        void BrushFaceReader::onBrush(Model::Node* parent, Model::Brush* brush, ParserStatus& status) {}

        void BrushFaceReader::onBrushFace(Model::BrushFace* face, ParserStatus& status) {
            m_brushFaces.push_back(face);
        }
    }
}
