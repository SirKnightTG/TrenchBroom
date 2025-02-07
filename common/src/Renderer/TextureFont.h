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

#ifndef TrenchBroom_Font
#define TrenchBroom_Font

#include "AttrString.h"
#include "FreeType.h"
#include "Macros.h"
#include "Renderer/FontGlyph.h"
#include "Renderer/FontGlyphBuilder.h"

#include <vecmath/forward.h>
#include <vecmath/vec.h>

#include <memory>
#include <vector>

namespace TrenchBroom {
    namespace Renderer {
        class FontTexture;

        class TextureFont {
        public:
        private:
            std::unique_ptr<FontTexture> m_texture;
            FontGlyph::List m_glyphs;
            size_t m_lineHeight;

            unsigned char m_firstChar;
            unsigned char m_charCount;
        public:
            TextureFont(std::unique_ptr<FontTexture> texture, const FontGlyph::List& glyphs, size_t lineHeight, unsigned char firstChar, unsigned char charCount);
            ~TextureFont();

            deleteCopyAndMove(TextureFont)

            std::vector<vm::vec2f> quads(const AttrString& string, bool clockwise, const vm::vec2f& offset = vm::vec2f::zero()) const;
            vm::vec2f measure(const AttrString& string) const;

            std::vector<vm::vec2f> quads(const String& string, bool clockwise, const vm::vec2f& offset = vm::vec2f::zero()) const;
            vm::vec2f measure(const String& string) const;

            void activate();
            void deactivate();
        };
    }
}


#endif /* defined(TrenchBroom_Font) */
