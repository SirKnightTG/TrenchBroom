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

#include "IOUtils.h"

#include "Ensure.h"
#include "Exceptions.h"
#include "IO/Path.h"

#include <vecmath/forward.h>
#include <vecmath/vec.h>

#include <cstring>
#include <streambuf>

namespace TrenchBroom {
    namespace IO {
        OpenFile::OpenFile(const Path& path, const bool write) :
        file(nullptr) {
            // cppcheck-suppress noCopyConstructor
            // cppcheck-suppress noOperatorEq
            file = fopen(path.asString().c_str(), write ? "w" : "r");
            if (file == nullptr) {
                throw FileSystemException("Cannot open file: " + path.asString());
            }
        }

        OpenFile::~OpenFile() {
            if (file != nullptr) {
                fclose(file);
            }
        }

        OpenStream::OpenStream(const Path& path, const bool write) :
        stream(path.asString().c_str(), std::ios::in | (write ? std::ios::out : std::ios::in)) {
            if (!stream.is_open()) {
                throw FileSystemException("Cannot open file: " + path.asString());
            }
        }

        OpenStream::~OpenStream() {
            if (stream.is_open()) {
                stream.close();
            }
        }

        std::string OpenStream::readAll() {
            return std::string((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());
        }

        size_t fileSize(std::FILE* file) {
            ensure(file != nullptr, "file is null");
            const auto pos = std::ftell(file);
            if (pos < 0) {
                throw FileSystemException("ftell failed");
            }

            if (std::fseek(file, 0, SEEK_END) != 0) {
                throw FileSystemException("fseek failed");
            }

            const auto size = std::ftell(file);
            if (size < 0) {
                throw FileSystemException("ftell failed");
            }

            if (std::fseek(file, pos, SEEK_SET) != 0) {
                throw FileSystemException("fseek failed");
            }

            return static_cast<size_t>(size);
        }

        String readGameComment(std::istream& stream) {
            return readInfoComment(stream, "Game");
        }

        String readFormatComment(std::istream& stream) {
            return readInfoComment(stream, "Format");
        }

        String readInfoComment(std::istream& stream, const String& name) {
            static const size_t MaxChars = 64;
            const String expectedHeader = "// " + name + ": ";
            char buf[MaxChars];

            stream.getline(buf, MaxChars);
            if (stream.fail())
                return "";

            String line(buf);
            if (line.size() < expectedHeader.size())
                return "";

            if (line.substr(0, expectedHeader.size()) != expectedHeader)
                return "";

            return line.substr(expectedHeader.size());
        }

        void writeGameComment(FILE* stream, const String& gameName, const String& mapFormat) {
            std::fprintf(stream, "// Game: %s\n", gameName.c_str());
            std::fprintf(stream, "// Format: %s\n", mapFormat.c_str());
        }

        vm::vec3f readVec3f(const char*& cursor) {
            vm::vec3f value;
            for (size_t i = 0; i < 3; i++) {
                value[i] = readFloat<float>(cursor);
            }
            return value;
        }

        vm::vec3f readVec3f(const char* const& cursor) {
            vm::vec3f value;
            for (size_t i = 0; i < 3; i++) {
                value[i] = readFloat<float>(cursor);
            }
            return value;
        }

        void readBytes(const char*& cursor, char* buffer, size_t n) {
            std::memcpy(buffer, cursor, n);
            cursor += n;
        }

        void readBytes(const char* const& cursor, char* buffer, size_t n) {
            std::memcpy(buffer, cursor, n);
        }

        void readBytes(const char*& cursor, unsigned char* buffer, size_t n) {
            std::memcpy(buffer, cursor, n);
            cursor += n;
        }

        void readBytes(const char* const& cursor, unsigned char* buffer, size_t n) {
            std::memcpy(buffer, cursor, n);
        }
    }
}
