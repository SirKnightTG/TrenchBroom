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

#ifndef TRENCHBROOM_RECOVERABLEEXCEPTIONS_H
#define TRENCHBROOM_RECOVERABLEEXCEPTIONS_H

#include "Exceptions.h"

#include "IO/Path.h"

#include <string_view>

namespace TrenchBroom {
    class RecoverableException : public Exception {
    protected:
        RecoverableException(const std::string& str);
    public:
        virtual std::string_view query() const = 0;
        virtual void recover() const = 0;
    };

    class FileDeletingException : public RecoverableException {
    private:
        IO::Path m_path;
    public:
        FileDeletingException(const std::string& str, const IO::Path& path) noexcept;

        std::string_view query() const override;
        void recover() const override;
    };
}

#endif //TRENCHBROOM_RECOVERABLEEXCEPTIONS_H
