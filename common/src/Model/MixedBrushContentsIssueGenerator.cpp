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

#include "MixedBrushContentsIssueGenerator.h"

#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/Issue.h"

#include <cassert>

namespace TrenchBroom {
    namespace Model {
        class MixedBrushContentsIssueGenerator::MixedBrushContentsIssue : public Issue {
        public:
            friend class MixedBrushContentsIssueQuickFix;
        public:
            static const IssueType Type;
        public:
            MixedBrushContentsIssue(Brush* brush) :
            Issue(brush) {}

            IssueType doGetType() const override {
                return Type;
            }

            const String doGetDescription() const override {
                return "Brush has mixed content flags";
            }
        };

        const IssueType MixedBrushContentsIssueGenerator::MixedBrushContentsIssue::Type = Issue::freeType();

        MixedBrushContentsIssueGenerator::MixedBrushContentsIssueGenerator() :
        IssueGenerator(MixedBrushContentsIssue::Type, "Mixed brush content flags") {}

        void MixedBrushContentsIssueGenerator::doGenerate(Brush* brush, IssueList& issues) const {
            const BrushFaceList& faces = brush->faces();
            BrushFaceList::const_iterator it = std::begin(faces);
            BrushFaceList::const_iterator end = std::end(faces);
            assert(it != end);

            const int contentFlags = (*it)->surfaceContents();
            ++it;
            while (it != end) {
                if ((*it)->surfaceContents() != contentFlags)
                    issues.push_back(new MixedBrushContentsIssue(brush));
                ++it;
            }
        }
    }
}
