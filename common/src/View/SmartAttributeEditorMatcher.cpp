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
 along with TrenchBroom.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SmartAttributeEditorMatcher.h"

#include "StringUtils.h"

namespace TrenchBroom {
    namespace View {
        SmartAttributeEditorMatcher::~SmartAttributeEditorMatcher() {}

        bool SmartAttributeEditorMatcher::matches(const Model::AttributeName& name, const Model::AttributableNodeList& attributables) const {
            return doMatches(name, attributables);
        }

        SmartAttributeEditorKeyMatcher::SmartAttributeEditorKeyMatcher(const String& pattern) :
        SmartAttributeEditorKeyMatcher({ pattern }) {}

        SmartAttributeEditorKeyMatcher::SmartAttributeEditorKeyMatcher(const std::initializer_list<String> patterns) :
        m_patterns(patterns) {}

        bool SmartAttributeEditorKeyMatcher::doMatches(const Model::AttributeName& name, const Model::AttributableNodeList& attributables) const {
            if (attributables.empty())
                return false;

            for (const String& pattern : m_patterns) {
                if (StringUtils::caseSensitiveMatchesPattern(name, pattern))
                    return true;
            }

            return false;
        }

        bool SmartAttributeEditorDefaultMatcher::doMatches(const Model::AttributeName& name, const Model::AttributableNodeList& attributables) const {
            return true;
        }
    }
}
