/*
 Copyright (C) 2018 Eric Wasylishen

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

#include "IO/DiskIO.h"
#include "IO/File.h"
#include "IO/Reader.h"

#include <memory>

namespace TrenchBroom {
    namespace IO {
        const char* buff();
        std::shared_ptr<File> file();
        void createEmpty(Reader&& r);
        void createNonEmpty(Reader&& r);
        void seekFromBegin(Reader&& r);
        void seekFromEnd(Reader&& r);
        void seekForward(Reader&& r);
        void subReader(Reader&& r);

        const char* buff() {
            static const auto* result = "abcdefghij_";
            return result;
        }

        std::shared_ptr<File> file() {
            static auto result = Disk::openFile(Disk::getCurrentWorkingDir() + Path("fixture/test/IO/Reader/10byte"));
            return result;
        }

        void createEmpty(Reader&& r) {
            EXPECT_EQ(0U, r.size());
            EXPECT_EQ(0U, r.position());
            EXPECT_NO_THROW(r.seekFromBegin(0U));
            EXPECT_NO_THROW(r.seekFromEnd(0U));
            EXPECT_NO_THROW(r.seekForward(0U));
            EXPECT_FALSE(r.canRead(1U));
            EXPECT_TRUE(r.canRead(0U));
            EXPECT_TRUE(r.eof());
            EXPECT_THROW(r.readChar<char>(), ReaderException);
        }

        TEST(BufferReaderTest, createEmpty) {
            createEmpty(Reader::from(buff(), buff()));
        }

        TEST(FileReaderTest, createEmpty) {
            const auto emptyFile = Disk::openFile(Disk::getCurrentWorkingDir() + Path("fixture/test/IO/Reader/empty"));
            createEmpty(emptyFile->reader());
        }

        void createNonEmpty(Reader&& r) {
            EXPECT_EQ(10U, r.size());
            EXPECT_EQ(0U, r.position());
            EXPECT_TRUE(r.canRead(0U));
            EXPECT_TRUE(r.canRead(10U));
            EXPECT_FALSE(r.canRead(11U));
            EXPECT_FALSE(r.eof());

            // read a char
            EXPECT_EQ('a', r.readChar<char>());
            EXPECT_EQ(1U, r.position());
            EXPECT_TRUE(r.canRead(1U));
            EXPECT_TRUE(r.canRead(9U));
            EXPECT_FALSE(r.canRead(10U));

            // read remainder
            EXPECT_EQ(String("bcdefghij"), r.readString(9));
            EXPECT_EQ(10U, r.position());
            EXPECT_FALSE(r.canRead(1U));
            EXPECT_TRUE(r.canRead(0U));
            EXPECT_TRUE(r.eof());
            EXPECT_THROW(r.readChar<char>(), ReaderException);
        }

        TEST(BufferReaderTest, createNonEmpty) {
            createNonEmpty(Reader::from(buff(), buff() + 10));
        }

        TEST(FileReaderTest, createNonEmpty) {
            createNonEmpty(file()->reader());
        }

        void seekFromBegin(Reader&& r) {
            r.seekFromBegin(0U);
            EXPECT_EQ(0U, r.position());

            r.seekFromBegin(1U);
            EXPECT_EQ(1U, r.position());

            r.seekFromBegin(2U);
            EXPECT_EQ(2U, r.position());

            EXPECT_THROW(r.seekFromBegin(11U), ReaderException);
            EXPECT_EQ(2U, r.position());
        }

        TEST(BufferReaderTest, testSeekFromBegin) {
            seekFromBegin(Reader::from(buff(), buff() + 10));

        }

        TEST(FileReaderTest, testSeekFromBegin) {
            seekFromBegin(file()->reader());
        }

        void seekFromEnd(Reader&& r) {
            r.seekFromEnd(0U);
            EXPECT_EQ(10U, r.position());

            r.seekFromEnd(1U);
            EXPECT_EQ(9U, r.position());

            r.seekFromEnd(10U);
            EXPECT_EQ(0U, r.position());

            EXPECT_THROW(r.seekFromEnd(11U), ReaderException);
            EXPECT_EQ(0U, r.position());
        }

        TEST(BufferReaderTest, testSeekFromEnd) {
            seekFromEnd(Reader::from(buff(), buff() + 10));
        }

        TEST(FileReaderTest, testSeekFromEnd) {
            seekFromEnd(file()->reader());
        }

        void seekForward(Reader&& r) {
            r.seekForward(1U);
            EXPECT_EQ(1U, r.position());

            r.seekForward(1U);
            EXPECT_EQ(2U, r.position());

            EXPECT_THROW(r.seekForward(9U), ReaderException);
            EXPECT_EQ(2U, r.position());
        }

        TEST(BufferReaderTest, testSeekForward) {
            seekForward(Reader::from(buff(), buff() + 10));
        }

        TEST(FileReaderTest, testSeekForward) {
            seekForward(file()->reader());
        }

        void subReader(Reader&& r) {
            auto s = r.subReaderFromBegin(5, 3);

            EXPECT_EQ(3U, s.size());
            EXPECT_EQ(0U, s.position());

            ASSERT_EQ('f', s.readChar<char>());
            EXPECT_EQ(1U, s.position());

            ASSERT_EQ('g', s.readChar<char>());
            EXPECT_EQ(2U, s.position());

            ASSERT_EQ('h', s.readChar<char>());
            EXPECT_EQ(3U, s.position());

            EXPECT_THROW(s.seekForward(1U), ReaderException);
            EXPECT_EQ(3U, s.position());
        }

        TEST(BufferReaderTest, testSubReader) {
            subReader(Reader::from(buff(), buff() + 10));
        }

        TEST(FileReaderTest, testSubReader) {
            subReader(file()->reader());
        }
    }
}
