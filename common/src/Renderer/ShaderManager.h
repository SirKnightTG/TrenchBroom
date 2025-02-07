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

#ifndef TrenchBroom_ShaderManager
#define TrenchBroom_ShaderManager

#include "StringType.h"
#include "Renderer/GL.h"
#include "Renderer/ShaderProgram.h"

#include <map>

namespace TrenchBroom {
    namespace IO {
        class Path;
    }

    namespace Renderer {
        class Shader;
        class ShaderConfig;

        class ShaderManager {
        private:
            using ShaderCache = std::map<String, Shader*>;
            using ShaderCacheEntry = std::pair<String, Shader*>;
            using ShaderProgramCache = std::map<const ShaderConfig*, ShaderProgram*>;
            using ShaderProgramCacheEntry = std::pair<const ShaderConfig*, ShaderProgram*>;

            ShaderCache m_shaders;
            ShaderProgramCache m_programs;
        public:
            ~ShaderManager();

            ShaderProgram& program(const ShaderConfig& config);
        private:
            ShaderProgram* createProgram(const ShaderConfig& config);
            Shader& loadShader(const String& name, const GLenum type);
        };

        class ActiveShader {
        private:
            ShaderProgram& m_program;
        public:
            ActiveShader(ShaderManager& shaderManager, const ShaderConfig& shaderConfig);
            ~ActiveShader();

            template <class T>
            void set(const String& name, const T& value) {
                m_program.set(name, value);
            }
        };
    }
}

#endif /* defined(TrenchBroom_ShaderManager) */
