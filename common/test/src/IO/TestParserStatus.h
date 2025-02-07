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

#ifndef TrenchBroom_TestParserStatus
#define TrenchBroom_TestParserStatus

#include "Logger.h"
#include "StringType.h"
#include "IO/ParserStatus.h"

#include <map>

namespace TrenchBroom {
    namespace IO {
        class TestParserStatus : public ParserStatus {
        private:
            static NullLogger _logger;
            using StatusCounts = std::map<Logger::LogLevel, size_t>;
            StatusCounts m_statusCounts;
        public:
            TestParserStatus();
        public:
            size_t countStatus(Logger::LogLevel level) const;
        private:
            void doProgress(double progress) override;
            void doLog(Logger::LogLevel level, const String& str) override;
        };
    }
}

#endif /* defined(TrenchBroom_TestParserStatus) */
