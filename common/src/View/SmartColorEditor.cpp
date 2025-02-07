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

#include "SmartColorEditor.h"

#include "Assets/ColorRange.h"
#include "CollectionUtils.h"
#include "Model/AttributableNode.h"
#include "Model/Entity.h"
#include "Model/EntityColor.h"
#include "Model/NodeVisitor.h"
#include "Model/World.h"
#include "View/BorderLine.h"
#include "View/ColorTable.h"
#include "View/MapDocument.h"
#include "View/ViewConstants.h"
#include "View/wxUtils.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QScrollArea>

#include <iomanip>

namespace TrenchBroom {
    namespace View {
        SmartColorEditor::SmartColorEditor(View::MapDocumentWPtr document, QWidget* parent) :
        SmartAttributeEditor(document, parent),
        m_floatRadio(nullptr),
        m_byteRadio(nullptr),
        m_colorPicker(nullptr),
        m_colorHistory(nullptr) {
            createGui();
        }

        void SmartColorEditor::createGui() {
            assert(m_floatRadio == nullptr);
            assert(m_byteRadio == nullptr);
            assert(m_colorPicker == nullptr);
            assert(m_colorHistory == nullptr);

            auto* rangeTxt = new QLabel(tr("Color range"));
            makeEmphasized(rangeTxt);

            m_floatRadio = new QRadioButton(tr("Float [0,1]"));
            m_byteRadio = new QRadioButton(tr("Byte [0,255]"));
            m_colorPicker = new ColorButton();
            m_colorHistory = new ColorTable(ColorHistoryCellSize);

            auto* colorHistoryScroller = new QScrollArea();
            colorHistoryScroller->setWidget(m_colorHistory);
            colorHistoryScroller->setWidgetResizable(true);
            colorHistoryScroller->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

            auto* leftLayout = new QVBoxLayout();
            leftLayout->setContentsMargins(0, 0, 0, 0);
            leftLayout->setSpacing(LayoutConstants::NarrowVMargin);
            leftLayout->addWidget(rangeTxt);
            leftLayout->addWidget(m_floatRadio);
            leftLayout->addWidget(m_byteRadio);
            leftLayout->addWidget(m_colorPicker);
            leftLayout->addStretch(1);

            auto* outerLayout = new QHBoxLayout();
            outerLayout->setContentsMargins(LayoutConstants::WideHMargin, 0, 0, 0);
            outerLayout->setSpacing(0);
            outerLayout->addLayout(leftLayout);
            outerLayout->addSpacing(LayoutConstants::WideHMargin);
            outerLayout->addWidget(new BorderLine(BorderLine::Direction_Vertical));
            outerLayout->addWidget(colorHistoryScroller, 1);
            setLayout(outerLayout);

            connect(m_floatRadio, &QAbstractButton::clicked, this, &SmartColorEditor::floatRangeRadioButtonClicked);
            connect(m_byteRadio, &QAbstractButton::clicked, this, &SmartColorEditor::byteRangeRadioButtonClicked);
            connect(m_colorPicker, &ColorButton::colorChanged, this, &SmartColorEditor::colorPickerChanged);
            connect(m_colorHistory, &ColorTable::colorTableSelected, this, &SmartColorEditor::colorTableSelected);
        }

        void SmartColorEditor::doUpdateVisual(const Model::AttributableNodeList& attributables) {
            ensure(m_floatRadio != nullptr, "floatRadio is null");
            ensure(m_byteRadio != nullptr, "byteRadio is null");
            ensure(m_colorPicker != nullptr, "colorPicker is null");
            ensure(m_colorHistory != nullptr, "colorHistory is null");

            updateColorRange(attributables);
            updateColorHistory();
        }

        void SmartColorEditor::updateColorRange(const Model::AttributableNodeList& attributables) {
            const auto range = detectColorRange(name(), attributables);
            if (range == Assets::ColorRange::Float) {
                m_floatRadio->setChecked(true);
                m_byteRadio->setChecked(false);
            } else if (range == Assets::ColorRange::Byte) {
                m_floatRadio->setChecked(false);
                m_byteRadio->setChecked(true);
            } else {
                m_floatRadio->setChecked(false);
                m_byteRadio->setChecked(false);
            }
        }

        struct ColorCmp {
            bool operator()(const QColor& lhs, const QColor& rhs) const {
                const auto lr = lhs.red() / 255.0f;
                const auto lg = lhs.green() / 255.0f;
                const auto lb = lhs.blue() / 255.0f;
                const auto rr = rhs.red() / 255.0f;
                const auto rg = rhs.green() / 255.0f;
                const auto rb = rhs.blue() / 255.0f;

                float lh, ls, lbr, rh, rs, rbr;
                Color::rgbToHSB(lr, lg, lb, lh, ls, lbr);
                Color::rgbToHSB(rr, rg, rb, rh, rs, rbr);

                if (lh < rh) {
                    return true;
                } else if (lh > rh) {
                    return false;
                } else if (ls < rs) {
                    return true;
                } else if (ls > rs) {
                    return false;
                } else if (lbr < rbr) {
                    return true;
                } else {
                    return false;
                }
            }
        };

        class SmartColorEditor::CollectColorsVisitor : public Model::ConstNodeVisitor {
        private:
            const Model::AttributeName& m_name;
            std::vector<QColor> m_colors;
        public:
            explicit CollectColorsVisitor(const Model::AttributeName& name) :
            m_name(name) {}

            const std::vector<QColor>& colors() const { return m_colors; }
        private:
            void doVisit(const Model::World* world) override   { visitAttributableNode(world); }
            void doVisit(const Model::Layer* layer) override   {}
            void doVisit(const Model::Group* group) override   {}
            void doVisit(const Model::Entity* entity) override { visitAttributableNode(entity); stopRecursion(); }
            void doVisit(const Model::Brush* brush) override   {}

            void visitAttributableNode(const Model::AttributableNode* attributable) {
                static const auto NullValue("");
                const auto& value = attributable->attribute(m_name, NullValue);
                if (value != NullValue)
                    addColor(Model::parseEntityColor(value));
            }

            void addColor(const Color& color) {
                VectorUtils::setInsert(m_colors, toQColor(color), ColorCmp());
            }
        };

        void SmartColorEditor::updateColorHistory() {
            CollectColorsVisitor collectAllColors(name());
            document()->world()->acceptAndRecurse(collectAllColors);
            m_colorHistory->setColors(collectAllColors.colors());

            CollectColorsVisitor collectSelectedColors(name());
            const auto nodes = document()->allSelectedAttributableNodes();
            Model::Node::accept(std::begin(nodes), std::end(nodes), collectSelectedColors);

            const auto& selectedColors = collectSelectedColors.colors();
            m_colorHistory->setSelection(selectedColors);

            const QColor color = !selectedColors.empty() ? selectedColors.back() : QColor(Qt::black);
            m_colorPicker->setColor(color);
        }

        void SmartColorEditor::setColor(const QColor& color) const {
            const auto colorRange = m_floatRadio->isChecked() ? Assets::ColorRange::Float : Assets::ColorRange::Byte;
            const auto value = Model::entityColorAsString(fromQColor(color), colorRange);
            document()->setAttribute(name(), value);
        }
        void SmartColorEditor::floatRangeRadioButtonClicked() {
            document()->convertEntityColorRange(name(), Assets::ColorRange::Float);
        }

        void SmartColorEditor::byteRangeRadioButtonClicked() {
            document()->convertEntityColorRange(name(), Assets::ColorRange::Byte);
        }

        void SmartColorEditor::colorPickerChanged(const QColor& color) {
            setColor(color);
        }

        void SmartColorEditor::colorTableSelected(QColor color) {
            setColor(color);
        }
    }
}
