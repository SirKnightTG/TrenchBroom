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

#include "LongAttributeValueIssueGenerator.h"

#include "StringUtils.h"
#include "Model/Brush.h"
#include "Model/RemoveEntityAttributesQuickFix.h"
#include "Model/Entity.h"
#include "Model/Issue.h"
#include "Model/IssueQuickFix.h"
#include "Model/MapFacade.h"
#include "Model/PushSelection.h"

#include <cassert>

namespace TrenchBroom {
    namespace Model {
        class LongAttributeValueIssueGenerator::LongAttributeValueIssue : public AttributeIssue {
        public:
            static const IssueType Type;
        private:
            const AttributeName m_attributeName;
        public:
            LongAttributeValueIssue(AttributableNode* node, const AttributeName& attributeName) :
            AttributeIssue(node),
            m_attributeName(attributeName) {}

            const AttributeName& attributeName() const override {
                return m_attributeName;
            }
        private:
            IssueType doGetType() const override {
                return Type;
            }

            const String doGetDescription() const override {
                return "The value of entity property '" + m_attributeName + "' is too long.";
            }
        };

        const IssueType LongAttributeValueIssueGenerator::LongAttributeValueIssue::Type = Issue::freeType();

        class LongAttributeValueIssueGenerator::TruncateLongAttributeValueIssueQuickFix : public IssueQuickFix {
        private:
            size_t m_maxLength;
        public:
            TruncateLongAttributeValueIssueQuickFix(const size_t maxLength) :
            IssueQuickFix(LongAttributeValueIssue::Type, "Truncate property values"),
            m_maxLength(maxLength) {}
        private:
            void doApply(MapFacade* facade, const Issue* issue) const override {
                const PushSelection push(facade);

                const LongAttributeValueIssue* attrIssue = static_cast<const LongAttributeValueIssue*>(issue);
                const AttributeName& attributeName = attrIssue->attributeName();
                const AttributeValue& attributeValue = attrIssue->attributeValue();

                // If world node is affected, the selection will fail, but if nothing is selected,
                // the removeAttribute call will correctly affect worldspawn either way.

                facade->deselectAll();
                facade->select(issue->node());
                facade->setAttribute(attributeName, attributeValue.substr(0, m_maxLength));
            }
        };

        LongAttributeValueIssueGenerator::LongAttributeValueIssueGenerator(const size_t maxLength) :
        IssueGenerator(LongAttributeValueIssue::Type, "Long entity property value"),
        m_maxLength(maxLength) {
            addQuickFix(new RemoveEntityAttributesQuickFix(LongAttributeValueIssue::Type));
            addQuickFix(new TruncateLongAttributeValueIssueQuickFix(m_maxLength));
        }

        void LongAttributeValueIssueGenerator::doGenerate(AttributableNode* node, IssueList& issues) const {
            for (const EntityAttribute& attribute : node->attributes()) {
                const AttributeName& attributeName = attribute.name();
                const AttributeValue& attributeValue = attribute.value();
                if (attributeValue.size() >= m_maxLength)
                    issues.push_back(new LongAttributeValueIssue(node, attributeName));
            }
        }
    }
}
