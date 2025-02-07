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

#include "View/Actions.h"

#include "KeyStrings.h"
#include "IO/Path.h"

#include <QApplication>
#include <QFileInfo>
#include <QTextStream>

#include <array>
#include <iostream>
#include <tuple>

namespace TrenchBroom {
    namespace View {
        void printKeys(QTextStream& out);
        QString toString(const IO::Path& path, const QString& suffix);
        QString toString(const QKeySequence& keySequence);

        void printMenuShortcuts(QTextStream& out);
        void printActionShortcuts(QTextStream& out);

        void printKeys(QTextStream& out) {
            const auto keyStrings = KeyStrings();

            out << "const keys = {\n";
            for (const auto& [key, str] : keyStrings) {
                out << "    " << key << ": '";
                if (str == "'") {
                    out << "\\'";
                } else if (str == "\\") {
                    out << "\\\\";
                } else {
                    out << str;
                }
                out << "',\n";
            }
            out << "};\n";
        }

        QString toString(const IO::Path& path, const QString& suffix) {
            QString result;
            result += "[";
            for (const auto& component : path.components()) {
                result += "'" + QString::fromStdString(component) + "', ";
            }
            result += "'" + suffix + "'";
            result += "]";
            return result;
        }

        QString toString(const QKeySequence& keySequence) {
            static constexpr std::array<std::tuple<int, Qt::Key>, 4> Modifiers = {
                std::make_tuple(static_cast<int>(Qt::SHIFT), Qt::Key_Shift),
                std::make_tuple(static_cast<int>(Qt::ALT), Qt::Key_Alt),
                std::make_tuple(static_cast<int>(Qt::CTRL), Qt::Key_Control),
                std::make_tuple(static_cast<int>(Qt::META), Qt::Key_Meta),
            };

            QString result;
            result += "{ ";

            if (keySequence.count() > 0) {
                const auto rawKey = keySequence[0];
                const auto key = rawKey & ~(static_cast<int>(Qt::KeyboardModifierMask));

                result += "key: " + QString::number(key) + ", ";
                result += "modifiers: [";
                for (const auto& [modifier, keyCode] : Modifiers) {
                    if ((rawKey & modifier) != 0) {
                        result += QString::number(keyCode) + ", ";
                    }
                }
                result += "]";
            } else {
                result += "key: 0, modifiers: []";
            }
            result += " }";
            return result;
        }

        class PrintMenuVisitor : public TrenchBroom::View::MenuVisitor {
        private:
            QTextStream& m_out;
            IO::Path m_path;
        public:
            PrintMenuVisitor(QTextStream& out) : m_out(out) {}

            void visit(const Menu& menu) override {
                m_path = m_path + IO::Path(menu.name());
                menu.visitEntries(*this);
                m_path = m_path.deleteLastComponent();
            }

            void visit(const MenuSeparatorItem& item) override {}

            void visit(const MenuActionItem& item) override {
                m_out << "    '" << QString::fromStdString(item.action().preferencePath().asString('/')) << "': ";
                m_out << "{ path: " << toString(m_path, item.label()) << ", shortcut: " << toString(item.action().keySequence()) << " },\n";
            }
        };

        void printMenuShortcuts(QTextStream& out) {
            out << "const menu = {\n";

            const auto& actionManager = ActionManager::instance();
            PrintMenuVisitor visitor(out);
            actionManager.visitMainMenu(visitor);

            out << "};\n";
        }

        void printActionShortcuts(QTextStream& out) {
            out << "const actions = {\n";

            const auto& actionManager = ActionManager::instance();
            actionManager.visitMapViewActions([&out](const auto& action) {
                out << "    '" << QString::fromStdString(action.preferencePath().asString('/')) << "': ";
                out << toString(action.keySequence()) << ",\n";
            });

            out << "};\n";
        }
    }
}

extern void qt_set_sequence_auto_mnemonic(bool b);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "Usage: dump-shortcuts <path-to-output-file>\n";
        return 1;
    }

    QSettings::setDefaultFormat(QSettings::IniFormat);

    // We can't use auto mnemonics in TrenchBroom. e.g. by default with Qt, Alt+D opens the "Debug" menu,
    // Alt+S activates the "Show default properties" checkbox in the entity inspector.
    // Flying with Alt held down and pressing WASD is a fundamental behaviour in TB, so we can't have
    // shortcuts randomly activating.
    qt_set_sequence_auto_mnemonic(false);

    const auto path = QString(argv[1]);
    auto file = QFile(path);
    const auto fileInfo = QFileInfo(file.fileName());
    const auto absPath = fileInfo.absoluteFilePath().toStdString();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { // QIODevice::WriteOnly implies truncate, which we want
        std::cout << "Could not open output file for writing: " << absPath << "\n";
        return 1;
    }

    QTextStream out(&file);

    // QKeySequence requires that an application instance is created!
    QApplication app(argc, argv);
    app.setApplicationName("TrenchBroom");
    // Needs to be "" otherwise Qt adds this to the paths returned by QStandardPaths
    // which would cause preferences to move from where they were with wx
    app.setOrganizationName("");
    app.setOrganizationDomain("com.kristianduske");

    TrenchBroom::View::printKeys(out);
    TrenchBroom::View::printMenuShortcuts(out);
    TrenchBroom::View::printActionShortcuts(out);
}
