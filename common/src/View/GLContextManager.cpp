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

#include "GLContextManager.h"

#include "Renderer/FontManager.h"
#include "Renderer/GL.h"
#include "Renderer/ShaderManager.h"
#include "Renderer/Vbo.h"

namespace TrenchBroom {
    namespace View {
        String GLContextManager::GLVendor = "unknown";
        String GLContextManager::GLRenderer = "unknown";
        String GLContextManager::GLVersion = "unknown";

        GLContextManager::GLContextManager() :
        m_initialized(false),
        m_vertexVbo(std::make_unique<Renderer::Vbo>(0xFFFFFF)),
        m_indexVbo(std::make_unique<Renderer::Vbo>(0xFFFFF, GL_ELEMENT_ARRAY_BUFFER)),
        m_fontManager(std::make_unique<Renderer::FontManager>()),
        m_shaderManager(std::make_unique<Renderer::ShaderManager>()) {}

        GLContextManager::~GLContextManager() = default;

        bool GLContextManager::initialized() const {
            return m_initialized;
        }

        static void initializeGlew() {
            glewExperimental = GL_TRUE;
            const GLenum glewState = glewInit();
            if (glewState != GLEW_OK) {
                RenderException e;
                e << "Error initializing glew: " << glewGetErrorString(glewState);
                throw e;
            }
        }

        bool GLContextManager::initialize() {
            if (!m_initialized) {
                initializeGlew();

                GLVendor   = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
                GLRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
                GLVersion  = reinterpret_cast<const char*>(glGetString(GL_VERSION));

                m_initialized = true;
                return true;
            }
            return false;
        }

        Renderer::Vbo& GLContextManager::vertexVbo() {
            return *m_vertexVbo;
        }

        Renderer::Vbo& GLContextManager::indexVbo() {
            return *m_indexVbo;
        }

        Renderer::FontManager& GLContextManager::fontManager() {
            return *m_fontManager;
        }

        Renderer::ShaderManager& GLContextManager::shaderManager() {
            return *m_shaderManager;
        }
    }
}
