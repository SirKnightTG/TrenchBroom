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

#include "WalTextureReader.h"

#include "Ensure.h"
#include "StringUtils.h"
#include "IO/File.h"
#include "IO/Reader.h"
#include "IO/Path.h"

#include <iostream>

namespace TrenchBroom {
    namespace IO {
        namespace WalLayout {
            const size_t TextureNameLength = 32;
        }

        WalTextureReader::WalTextureReader(const NameStrategy& nameStrategy, const Assets::Palette& palette) :
        TextureReader(nameStrategy),
        m_palette(palette) {}

        Assets::Texture* WalTextureReader::doReadTexture(std::shared_ptr<File> file) const {
            const auto& path = file->path();
            auto reader = file->reader();

            try {
                const char version = reader.readChar<char>();
                reader.seekFromBegin(0);

                if (version == 3) {
                    return readDkWal(reader, path);
                } else {
                    return readQ2Wal(reader, path);
                }
            } catch (const ReaderException&) {
                return new Assets::Texture(textureName(path), 16, 16);
            }
        }

        Assets::Texture* WalTextureReader::readQ2Wal(Reader& reader, const Path& path) const {
            static const size_t MaxMipLevels = 4;
            static Color tempColor, averageColor;
            static Assets::TextureBuffer::List buffers(MaxMipLevels);
            static size_t offsets[MaxMipLevels];

            const String name = reader.readString(WalLayout::TextureNameLength);
            const size_t width = reader.readSize<uint32_t>();
            const size_t height = reader.readSize<uint32_t>();

            if (!checkTextureDimensions(width, height)) {
                return new Assets::Texture(textureName(path), 16, 16);
            }

            if (!m_palette.initialized()) {
                return new Assets::Texture(textureName(name, path), width, height);
            }

            const auto mipLevels = readMipOffsets(MaxMipLevels, offsets, width, height, reader);
            Assets::setMipBufferSize(buffers, mipLevels, width, height, GL_RGBA);
            readMips(m_palette, mipLevels, offsets, width, height, reader, buffers, averageColor, Assets::PaletteTransparency::Opaque);
            return new Assets::Texture(textureName(name, path), width, height, averageColor, buffers, GL_RGBA, Assets::TextureType::Opaque);
        }

        Assets::Texture* WalTextureReader::readDkWal(Reader& reader, const Path& path) const {
            static const size_t MaxMipLevels = 9;
            static Color tempColor, averageColor;
            static Assets::TextureBuffer::List buffers(MaxMipLevels);
            static size_t offsets[MaxMipLevels];

            const char version = reader.readChar<char>();
            ensure(version == 3, "Unknown WAL texture version");

            const auto name = reader.readString(WalLayout::TextureNameLength);
            reader.seekForward(3); // garbage

            const auto width = reader.readSize<uint32_t>();
            const auto height = reader.readSize<uint32_t>();

            if (!checkTextureDimensions(width, height)) {
                return new Assets::Texture(textureName(path), 16, 16);
            }

            const auto mipLevels = readMipOffsets(MaxMipLevels, offsets, width, height, reader);
            Assets::setMipBufferSize(buffers, mipLevels, width, height, GL_RGBA);

            reader.seekForward(32 + 2 * sizeof(uint32_t)); // animation name, flags, contents

            auto paletteReader = reader.subReaderFromCurrent(3 * 256);
            const auto embeddedPalette = Assets::Palette::fromRaw(paletteReader);
            const auto hasTransparency = readMips(embeddedPalette, mipLevels, offsets, width, height, reader, buffers, averageColor, Assets::PaletteTransparency::Index255Transparent);
            return new Assets::Texture(textureName(name, path), width, height, averageColor, buffers, GL_RGBA, hasTransparency ? Assets::TextureType::Masked : Assets::TextureType::Opaque);
        }

        size_t WalTextureReader::readMipOffsets(const size_t maxMipLevels, size_t offsets[], const size_t width, const size_t height, Reader& reader) const {
            size_t mipLevels = 0;
            for (size_t i = 0; i < maxMipLevels; ++i) {
                offsets[i] = reader.readSize<uint32_t>();
                ++mipLevels;
                if (width / (1 << i) == 1 || height / (1 << i) == 1) {
                    break;
                }
            }

            // make sure the reader position is correct afterwards
            reader.seekForward((maxMipLevels - mipLevels) * sizeof(uint32_t));

            return mipLevels;
        }

        bool WalTextureReader::readMips(const Assets::Palette& palette, const size_t mipLevels, const size_t offsets[], const size_t width, const size_t height, Reader& reader, Assets::TextureBuffer::List& buffers, Color& averageColor, const Assets::PaletteTransparency transparency) {
            static Color tempColor;

            auto hasTransparency = false;
            for (size_t i = 0; i < mipLevels; ++i) {
                const auto offset = offsets[i];
                reader.seekFromBegin(offset);
                const auto curWidth = width / (1 << i);
                const auto curHeight = height / (1 << i);
                const auto size = curWidth * curHeight;

                // FIXME: Confirm this is actually happening because of bad data and not a bug.
                // FIXME: Corrupt or missing mips should be deleted, rather than uploaded with garbage.
                if (!reader.canRead(size)) {
                    std::cerr << "WalTextureReader::readMips: buffer overrun\n";
                    return false;
                }

                hasTransparency |= (palette.indexedToRgba(reader, size, buffers[i], transparency, tempColor) && i == 0);
                if (i == 0) {
                    averageColor = tempColor;
                }
            }
            return hasTransparency;
        }
    }
}
