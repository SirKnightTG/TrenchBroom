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

#include <gtest/gtest.h>

#include "Exceptions.h"
#include "Macros.h"
#include "IO/DiskFileSystem.h"
#include "IO/FileMatcher.h"
#include "IO/Path.h"
#include "IO/PathQt.h"
#include "IO/TestEnvironment.h"

#include <algorithm>

#include <QFileInfo>

namespace TrenchBroom {
    namespace IO {
        class FSTestEnvironment : public TestEnvironment {
        public:
            explicit FSTestEnvironment(const String& dir = "fstest") :
            TestEnvironment(dir) {
                createTestEnvironment();
            }
        private:
            void doCreateTestEnvironment() override {
                createDirectory(Path("dir1"));
                createDirectory(Path("dir2"));
                createDirectory(Path("anotherDir"));
                createDirectory(Path("anotherDir/subDirTest"));

                createFile(Path("test.txt"), "some content");
                createFile(Path("test2.map"), "//test file\n{}");
                createFile(Path("anotherDir/subDirTest/test2.map"), "//sub dir test file\n{}");
                createFile(Path("anotherDir/test3.map"), "//yet another test file\n{}");
            }
        };

        TEST(FileSystemTest, makeAbsolute) {
            FSTestEnvironment env;

            auto fs = std::make_shared<DiskFileSystem>(env.dir() + Path("anotherDir"));
                 fs = std::make_shared<DiskFileSystem>(fs, env.dir() + Path("dir1"));

            // Existing files should be resolved against the first file system in the chain that contains them:
            const auto absPathExisting = fs->makeAbsolute(Path("test3.map"));
            ASSERT_EQ(env.dir() + Path("anotherDir/test3.map"), absPathExisting);

            // Non existing files should be resolved against the first filesystem in the fs chain:
            const auto absPathNotExisting = fs->makeAbsolute(Path("asdf.map"));
            ASSERT_EQ(env.dir() + Path("dir1/asdf.map"), absPathNotExisting);
        }

        TEST(DiskTest, fixPath) {
            FSTestEnvironment env;

            ASSERT_THROW(Disk::fixPath(Path("asdf/blah")), FileSystemException);
            ASSERT_THROW(Disk::fixPath(Path("/../../test")), FileSystemException);

            // on case sensitive file systems, this should also work
            ASSERT_TRUE(QFileInfo::exists(IO::pathAsQString(Disk::fixPath(env.dir() + Path("TEST.txt")))));
            ASSERT_TRUE(QFileInfo::exists(IO::pathAsQString(Disk::fixPath(env.dir() + Path("anotHERDIR/./SUBdirTEST/../SubdirTesT/TesT2.MAP")))));
        }

        TEST(DiskTest, directoryExists) {
            FSTestEnvironment env;

            ASSERT_THROW(Disk::directoryExists(Path("asdf/bleh")), FileSystemException);

            ASSERT_TRUE(Disk::directoryExists(env.dir() + Path("anotherDir")));
            ASSERT_TRUE(Disk::directoryExists(env.dir() + Path("anotherDir/subDirTest")));
        }

        TEST(DiskTest, fileExists) {
            FSTestEnvironment env;

            ASSERT_THROW(Disk::fileExists(Path("asdf/bleh")), FileSystemException);

            ASSERT_TRUE(Disk::fileExists(env.dir() + Path("test.txt")));
            ASSERT_TRUE(Disk::fileExists(env.dir() + Path("anotherDir/subDirTest/test2.map")));
        }

        TEST(DiskTest, getDirectoryContents) {
            FSTestEnvironment env;

            ASSERT_THROW(Disk::getDirectoryContents(Path("asdf/bleh")), FileSystemException);
            ASSERT_THROW(Disk::getDirectoryContents(env.dir() + Path("does/not/exist")), FileSystemException);

            const Path::List contents = Disk::getDirectoryContents(env.dir());
            ASSERT_EQ(5u, contents.size());
            ASSERT_TRUE(std::find(std::begin(contents), std::end(contents), Path("dir1")) != std::end(contents));
            ASSERT_TRUE(std::find(std::begin(contents), std::end(contents), Path("dir2")) != std::end(contents));
            ASSERT_TRUE(std::find(std::begin(contents), std::end(contents), Path("anotherDir")) != std::end(contents));
            ASSERT_TRUE(std::find(std::begin(contents), std::end(contents), Path("test.txt")) != std::end(contents));
            ASSERT_TRUE(std::find(std::begin(contents), std::end(contents), Path("test2.map")) != std::end(contents));
        }

        TEST(DiskTest, openFile) {
            FSTestEnvironment env;

            ASSERT_THROW(Disk::openFile(Path("asdf/bleh")), FileSystemException);
            ASSERT_THROW(Disk::openFile(env.dir() + Path("does/not/exist")), FileNotFoundException);

            ASSERT_THROW(Disk::openFile(env.dir() + Path("does_not_exist.txt")), FileNotFoundException);
            ASSERT_TRUE(Disk::openFile(env.dir() + Path("test.txt")) != nullptr);
            ASSERT_TRUE(Disk::openFile(env.dir() + Path("anotherDir/subDirTest/test2.map")) != nullptr);
        }

        TEST(DiskTest, resolvePath) {
            FSTestEnvironment env;

            Path::List rootPaths;
            rootPaths.push_back(env.dir());
            rootPaths.push_back(env.dir() + Path("anotherDir"));

            Path::List paths;
            paths.push_back(Path("test.txt"));
            paths.push_back(Path("test3.map"));
            paths.push_back(Path("subDirTest/test2.map"));
            paths.push_back(Path("/asfd/blah"));
            paths.push_back(Path("adk3kdk/bhb"));

            ASSERT_EQ(env.dir() + Path("test.txt"), Disk::resolvePath(rootPaths, paths[0]));
            ASSERT_EQ(env.dir() + Path("anotherDir/test3.map"), Disk::resolvePath(rootPaths, paths[1]));
            ASSERT_EQ(env.dir() + Path("anotherDir/subDirTest/test2.map"), Disk::resolvePath(rootPaths, paths[2]));
            ASSERT_EQ(Path(""), Disk::resolvePath(rootPaths, paths[3]));
            ASSERT_EQ(Path(""), Disk::resolvePath(rootPaths, paths[4]));
        }

        TEST(DiskFileSystemTest, createDiskFileSystem) {
            FSTestEnvironment env;

            ASSERT_THROW(DiskFileSystem(env.dir() + Path("asdf"), true), FileSystemException);
            ASSERT_NO_THROW(DiskFileSystem(env.dir() + Path("asdf"), false));
            ASSERT_NO_THROW(DiskFileSystem(env.dir(), true));

            // for case sensitive file systems
            ASSERT_NO_THROW(DiskFileSystem(env.dir() + Path("ANOTHERDIR"), true));

            const DiskFileSystem fs(env.dir() + Path("anotherDir/.."), true);
            ASSERT_EQ(fs.root(), fs.makeAbsolute(Path("")));
        }

        TEST(DiskFileSystemTest, directoryExists) {
            FSTestEnvironment env;
            const DiskFileSystem fs(env.dir());

#if defined _WIN32
            ASSERT_THROW(fs.directoryExists(Path("c:\\")), FileSystemException);
#else
            ASSERT_THROW(fs.directoryExists(Path("/")), FileSystemException);
#endif
            ASSERT_THROW(fs.directoryExists(Path("..")), FileSystemException);

            ASSERT_TRUE(fs.directoryExists(Path(".")));
            ASSERT_TRUE(fs.directoryExists(Path("anotherDir")));
            ASSERT_TRUE(fs.directoryExists(Path("anotherDir/subDirTest")));
            ASSERT_TRUE(fs.directoryExists(Path("anotherDir/./subDirTest/..")));
            ASSERT_TRUE(fs.directoryExists(Path("ANOTHerDir")));
            ASSERT_FALSE(fs.directoryExists(Path("test.txt")));
            ASSERT_FALSE(fs.directoryExists(Path("fasdf")));
        }

        TEST(DiskFileSystemTest, fileExists) {
            FSTestEnvironment env;
            const DiskFileSystem fs(env.dir());

#if defined _WIN32
            ASSERT_THROW(fs.fileExists(Path("C:\\does_not_exist_i_hope.txt")), FileSystemException);
#else
            ASSERT_THROW(fs.fileExists(Path("/does_not_exist_i_hope.txt")), FileSystemException);
#endif
            ASSERT_THROW(fs.fileExists(Path("../test.txt")), FileSystemException);

            ASSERT_TRUE(fs.fileExists(Path("test.txt")));
            ASSERT_TRUE(fs.fileExists(Path("./test.txt")));
            ASSERT_TRUE(fs.fileExists(Path("anotherDir/test3.map")));
            ASSERT_TRUE(fs.fileExists(Path("anotherDir/./subDirTest/../subDirTest/test2.map")));
            ASSERT_TRUE(fs.fileExists(Path("ANOtherDir/test3.MAP")));
            ASSERT_FALSE(fs.fileExists(Path("anotherDir/whatever.txt")));
            ASSERT_FALSE(fs.fileExists(Path("fdfdf.blah")));
        }

        TEST(DiskFileSystemTest, findItems) {
            FSTestEnvironment env;
            const DiskFileSystem fs(env.dir());

#if defined _WIN32
            ASSERT_THROW(fs.findItems(Path("c:\\")), FileSystemException);
#else
            ASSERT_THROW(fs.findItems(Path("/")), FileSystemException);
#endif
            ASSERT_THROW(fs.findItems(Path("..")), FileSystemException);

            Path::List items = fs.findItems(Path("."));
            ASSERT_EQ(5u, items.size());
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./dir1")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./dir2")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./anotherDir")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./test.txt")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./test2.map")) != std::end(items));

            items = fs.findItems(Path(""), FileExtensionMatcher("TXT"));
            ASSERT_EQ(1u, items.size());
            ASSERT_EQ(Path("test.txt"), items.front());

            items = fs.findItems(Path("anotherDir"));
            ASSERT_EQ(2u, items.size());
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("anotherDir/subDirTest")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("anotherDir/test3.map")) != std::end(items));
        }

        TEST(DiskFileSystemTest, findItemsRecursively) {
            FSTestEnvironment env;
            const DiskFileSystem fs(env.dir());

#if defined _WIN32
            ASSERT_THROW(fs.findItemsRecursively(Path("c:\\")), FileSystemException);
#else
            ASSERT_THROW(fs.findItemsRecursively(Path("/")), FileSystemException);
#endif
            ASSERT_THROW(fs.findItemsRecursively(Path("..")), FileSystemException);

            Path::List items = fs.findItemsRecursively(Path("."));
            ASSERT_EQ(8u, items.size());
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./dir1")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./dir2")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./anotherDir")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./anotherDir/test3.map")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./anotherDir/subDirTest")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./anotherDir/subDirTest/test2.map")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./test.txt")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("./test2.map")) != std::end(items));

            items = fs.findItemsRecursively(Path(""), FileExtensionMatcher("MAP"));
            ASSERT_EQ(3u, items.size());
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("anotherDir/test3.map")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("anotherDir/subDirTest/test2.map")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("test2.map")) != std::end(items));

            items = fs.findItemsRecursively(Path("anotherDir"));
            ASSERT_EQ(3u, items.size());
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("anotherDir/test3.map")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("anotherDir/subDirTest")) != std::end(items));
            ASSERT_TRUE(std::find(std::begin(items), std::end(items), Path("anotherDir/subDirTest/test2.map")) != std::end(items));
        }

        // getDirectoryContents gets tested thoroughly by the tests for the find* methods

        TEST(DiskFileSystemTest, openFile) {
            FSTestEnvironment env;
            const DiskFileSystem fs(env.dir());

#if defined _WIN32
            ASSERT_THROW(fs.openFile(Path("c:\\hopefully_nothing.here")), FileSystemException);
#else
            ASSERT_THROW(fs.openFile(Path("/hopefully_nothing.here")), FileSystemException);
#endif
            ASSERT_THROW(fs.openFile(Path("..")), FileSystemException);
            ASSERT_THROW(fs.openFile(Path(".")), FileSystemException);
            ASSERT_THROW(fs.openFile(Path("anotherDir")), FileSystemException);

            ASSERT_TRUE(fs.openFile(Path("test.txt")) != nullptr);
            ASSERT_TRUE(fs.openFile(Path("anotherDir/test3.map")) != nullptr);
            ASSERT_TRUE(fs.openFile(Path("anotherDir/../anotherDir/./test3.map")) != nullptr);
        }

        TEST(WritableDiskFileSystemTest, createWritableDiskFileSystem) {
            FSTestEnvironment env;

            ASSERT_THROW(WritableDiskFileSystem(env.dir() + Path("asdf"), false), FileSystemException);
            ASSERT_NO_THROW(WritableDiskFileSystem(env.dir() + Path("asdf"), true));
            ASSERT_NO_THROW(WritableDiskFileSystem(env.dir(), true));

            // for case sensitive file systems
            ASSERT_NO_THROW(WritableDiskFileSystem(env.dir() + Path("ANOTHERDIR"), false));

            const WritableDiskFileSystem fs(env.dir() + Path("anotherDir/.."), false);
            ASSERT_EQ(env.dir(), fs.makeAbsolute(Path("")));
        }

        TEST(WritableDiskFileSystemTest, createDirectory) {
            FSTestEnvironment env;
            WritableDiskFileSystem fs(env.dir(), false);

#if defined _WIN32
            ASSERT_THROW(fs.createDirectory(Path("c:\\hopefully_nothing_here")), FileSystemException);
#else
            ASSERT_THROW(fs.createDirectory(Path("/hopefully_nothing_here")), FileSystemException);
#endif
            ASSERT_THROW(fs.createDirectory(Path("")), FileSystemException);
            ASSERT_THROW(fs.createDirectory(Path(".")), FileSystemException);
            ASSERT_THROW(fs.createDirectory(Path("..")), FileSystemException);
            ASSERT_THROW(fs.createDirectory(Path("dir1")), FileSystemException);
            ASSERT_THROW(fs.createDirectory(Path("test.txt")), FileSystemException);

            fs.createDirectory(Path("newDir"));
            ASSERT_TRUE(fs.directoryExists(Path("newDir")));

            fs.createDirectory(Path("newDir/someOtherDir"));
            ASSERT_TRUE(fs.directoryExists(Path("newDir/someOtherDir")));

            fs.createDirectory(Path("newDir/someOtherDir/.././yetAnotherDir/."));
            ASSERT_TRUE(fs.directoryExists(Path("newDir/yetAnotherDir")));
        }

        TEST(WritableDiskFileSystemTest, deleteFile) {
            FSTestEnvironment env;
            WritableDiskFileSystem fs(env.dir(), false);

#if defined _WIN32
            ASSERT_THROW(fs.deleteFile(Path("c:\\hopefully_nothing_here.txt")), FileSystemException);
#else
            ASSERT_THROW(fs.deleteFile(Path("/hopefully_nothing_here.txt")), FileSystemException);
#endif
            ASSERT_THROW(fs.deleteFile(Path("")), FileSystemException);
            ASSERT_THROW(fs.deleteFile(Path(".")), FileSystemException);
            ASSERT_THROW(fs.deleteFile(Path("..")), FileSystemException);
            ASSERT_THROW(fs.deleteFile(Path("dir1")), FileSystemException);
            ASSERT_THROW(fs.deleteFile(Path("asdf.txt")), FileSystemException);
            ASSERT_THROW(fs.deleteFile(Path("/dir1/asdf.txt")), FileSystemException);

            fs.deleteFile(Path("test.txt"));
            ASSERT_FALSE(fs.fileExists(Path("test.txt")));

            fs.deleteFile(Path("anotherDir/test3.map"));
            ASSERT_FALSE(fs.fileExists(Path("anotherDir/test3.map")));

            fs.deleteFile(Path("anotherDir/subDirTest/.././subDirTest/./test2.map"));
            ASSERT_FALSE(fs.fileExists(Path("anotherDir/subDirTest/test2.map")));
        }

        TEST(WritableDiskFileSystemTest, moveFile) {
            FSTestEnvironment env;
            WritableDiskFileSystem fs(env.dir(), false);

#if defined _WIN32
            ASSERT_THROW(fs.moveFile(Path("c:\\hopefully_nothing_here.txt"),
                                     Path("dest.txt"), false), FileSystemException);
            ASSERT_THROW(fs.moveFile(Path("test.txt"),
                                     Path("C:\\dest.txt"), false), FileSystemException);
#else
            ASSERT_THROW(fs.moveFile(Path("/hopefully_nothing_here.txt"),
                                     Path("dest.txt"), false), FileSystemException);
            ASSERT_THROW(fs.moveFile(Path("test.txt"),
                                     Path("/dest.txt"), false), FileSystemException);
#endif

            ASSERT_THROW(fs.moveFile(Path("test.txt"),
                                     Path("test2.map"), false), FileSystemException);
            ASSERT_THROW(fs.moveFile(Path("test.txt"),
                                     Path("anotherDir/test3.map"), false), FileSystemException);
            ASSERT_THROW(fs.moveFile(Path("test.txt"),
                                     Path("anotherDir/../anotherDir/./test3.map"), false), FileSystemException);

            fs.moveFile(Path("test.txt"),
                        Path("test2.txt"), true);
            ASSERT_FALSE(fs.fileExists(Path("test.txt")));
            ASSERT_TRUE(fs.fileExists(Path("test2.txt")));

            fs.moveFile(Path("test2.txt"),
                        Path("test2.map"), true);
            ASSERT_FALSE(fs.fileExists(Path("test2.txt")));
            ASSERT_TRUE(fs.fileExists(Path("test2.map")));
            // we're trusting that the file is actually overwritten (should really test the contents here...)

            fs.moveFile(Path("test2.map"),
                        Path("dir1/test2.map"), true);
            ASSERT_FALSE(fs.fileExists(Path("test2.map")));
            ASSERT_TRUE(fs.fileExists(Path("dir1/test2.map")));
        }

        TEST(WritableDiskFileSystemTest, copyFile) {
            FSTestEnvironment env;
            WritableDiskFileSystem fs(env.dir(), false);

#if defined _WIN32
            ASSERT_THROW(fs.copyFile(Path("c:\\hopefully_nothing_here.txt"),
                                     Path("dest.txt"), false), FileSystemException);
            ASSERT_THROW(fs.copyFile(Path("test.txt"),
                                     Path("C:\\dest.txt"), false), FileSystemException);
#else
            ASSERT_THROW(fs.copyFile(Path("/hopefully_nothing_here.txt"),
                                     Path("dest.txt"), false), FileSystemException);
            ASSERT_THROW(fs.copyFile(Path("test.txt"),
                                     Path("/dest.txt"), false), FileSystemException);
#endif

            ASSERT_THROW(fs.copyFile(Path("test.txt"),
                                     Path("test2.map"), false), FileSystemException);
            ASSERT_THROW(fs.copyFile(Path("test.txt"),
                                     Path("anotherDir/test3.map"), false), FileSystemException);
            ASSERT_THROW(fs.copyFile(Path("test.txt"),
                                     Path("anotherDir/../anotherDir/./test3.map"), false), FileSystemException);

            fs.copyFile(Path("test.txt"),
                        Path("test2.txt"), true);
            ASSERT_TRUE(fs.fileExists(Path("test.txt")));
            ASSERT_TRUE(fs.fileExists(Path("test2.txt")));

            fs.copyFile(Path("test2.txt"),
                        Path("test2.map"), true);
            ASSERT_TRUE(fs.fileExists(Path("test2.txt")));
            ASSERT_TRUE(fs.fileExists(Path("test2.map")));
            // we're trusting that the file is actually overwritten (should really test the contents here...)

            fs.copyFile(Path("test2.map"),
                        Path("dir1/test2.map"), true);
            ASSERT_TRUE(fs.fileExists(Path("test2.map")));
            ASSERT_TRUE(fs.fileExists(Path("dir1/test2.map")));
        }
    }
}
