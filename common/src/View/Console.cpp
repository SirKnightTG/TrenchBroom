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

#include "Console.h"

#include "FileLogger.h"
#include "View/ViewConstants.h"

#include <QDebug>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QScrollBar>

#include <iostream>

namespace TrenchBroom {
    namespace View {
        Console::Console(QWidget* parent) :
        TabBookPage(parent) {
            m_textView = new QTextEdit();
            m_textView->setReadOnly(true);
            m_textView->setWordWrapMode(QTextOption::NoWrap);

            QVBoxLayout* sizer = new QVBoxLayout();
            sizer->setContentsMargins(0, 0, 0, 0);
            sizer->addWidget(m_textView);
            setLayout(sizer);
        }

        void Console::doLog(const LogLevel level, const String& message) {
            doLog(level, QString::fromStdString(message));
        }

        void Console::doLog(const LogLevel level, const QString& message) {
            if (!message.isEmpty()) {
                logToDebugOut(level, message);
                logToConsole(level, message);
                FileLogger::instance().log(level, message);
            }
        }

        void Console::logToDebugOut(const LogLevel level, const QString& message) {
            qDebug("%s", message.toStdString().c_str());
        }

        void Console::logToConsole(const LogLevel level, const QString& message) {
            QTextCharFormat format;
            switch (level) {
                case LogLevel_Debug:
                    format.setForeground(QBrush(Colors::disabledText()));
                    break;
                case LogLevel_Info:
                    break;
                case LogLevel_Warn:
                    format.setForeground(QBrush(Colors::defaultText()));
                    break;
                case LogLevel_Error:
                    format.setForeground(QBrush(QColor(250, 30, 60)));
                    break;
            }
            format.setFont(Fonts::fixedWidthFont());

            QTextCursor cursor(m_textView->document());
            cursor.movePosition(QTextCursor::MoveOperation::End);
            cursor.insertText(message, format);
            cursor.insertText("\n");

            m_textView->moveCursor(QTextCursor::MoveOperation::End);
        }
    }
}
