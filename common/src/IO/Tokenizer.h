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

#ifndef TrenchBroom_Tokenizer_h
#define TrenchBroom_Tokenizer_h

#include "Exceptions.h"
#include "Token.h"
#include "SharedPointer.h"

#include <stack>

namespace TrenchBroom {
    namespace IO {
        class TokenizerState {
        private:
            const char* m_begin;
            const char* m_cur;
            const char* m_end;
            String m_escapableChars;
            char m_escapeChar;
            size_t m_line;
            size_t m_column;
            bool m_escaped;
        public:
            TokenizerState(const char* begin, const char* end, const String& escapableChars, char escapeChar);

            TokenizerState* clone(const char* begin, const char* end) const;

            size_t length() const;
            const char* begin() const;
            const char* end() const;

            const char* curPos() const;
            char curChar() const;

            char lookAhead(size_t offset = 1) const;

            size_t line() const;
            size_t column() const;

            bool escaped() const;
            String unescape(const String& str);
            void resetEscaped();

            bool eof() const;
            bool eof(const char* ptr) const;

            size_t offset(const char* ptr) const;

            void advance(size_t offset);
            void advance();
            void reset();

            void errorIfEof() const;

            TokenizerState snapshot() const;
            void restore(const TokenizerState& snapshot);
        };

        template <typename TokenType>
        class Tokenizer {
        public:
            using Token = TokenTemplate<TokenType>;
        private:
            using TokenStack = std::stack<Token>;

            using StatePtr = std::shared_ptr<TokenizerState>;

            class SaveState {
            private:
                StatePtr m_state;
                TokenizerState m_snapshot;
            public:
                explicit SaveState(StatePtr state) :
                m_state(state),
                m_snapshot(m_state->snapshot()) {}

                ~SaveState() {
                    m_state->restore(m_snapshot);
                }
            };

            StatePtr m_state;

            template <typename T> friend class Tokenizer;
        public:
            static const String& Whitespace() {
                static const String whitespace(" \t\n\r");
                return whitespace;
            }
        public:
            Tokenizer(const char* begin, const char* end, const String& escapableChars, const char escapeChar) :
            m_state(std::make_shared<TokenizerState>(begin, end, escapableChars, escapeChar)) {}

            Tokenizer(const String& str, const String& escapableChars, const char escapeChar) :
            m_state(std::make_shared<TokenizerState>(str.c_str(), str.c_str() + str.size(), escapableChars, escapeChar)) {}

            template <typename OtherType>
            explicit Tokenizer(Tokenizer<OtherType>& nestedTokenizer) :
            m_state(nestedTokenizer.m_state) {}

            Tokenizer(const Tokenizer& other) :
            m_state(other.m_state) {}

            virtual ~Tokenizer() = default;

            Token nextToken(const TokenType skipTokens = 0u) {
                auto token = emitToken();
                while (token.hasType(skipTokens)) {
                    token = emitToken();
                }
                return token;
            }

            Token peekToken(const TokenType skipTokens = 0u) {
                SaveState oldState(m_state);
                return nextToken(skipTokens);
            }

            void skipToken(const TokenType skipTokens = ~0u) {
                if (peekToken().hasType(skipTokens)) {
                    nextToken();
                }
            }

            void discardLine() {
                discardUntil("\n");
                discardWhile("\n");
            }

            String readRemainder(const TokenType delimiterType) {
                if (eof()) {
                    return "";
                }

                Token token = peekToken();
                const char* startPos = std::begin(token);
                const char* endPos = startPos;
                do {
                    token = nextToken();
                    endPos = std::end(token);
                } while (peekToken().hasType(delimiterType) == 0 && !eof());

                return String(startPos, static_cast<size_t>(endPos - startPos));
            }

            String readAnyString(const String& delims) {
                while (isWhitespace(curChar())) {
                    advance();
                }
                const char* startPos = curPos();
                const char* endPos = (curChar() == '"' ? readQuotedString() : readUntil(delims));
                return String(startPos, static_cast<size_t>(endPos - startPos));
            }

            String unescapeString(const String& str) const {
                return m_state->unescape(str);
            }

            void reset() {
                m_state->reset();
            }

            double progress() const {
                if (length() == 0) {
                    return 0.0;
                }
                const auto cur = static_cast<double>(offset(curPos()));
                const auto len = static_cast<double>(length());
                return cur / len;
            }

            bool eof() const {
                return m_state->eof();
            }
        public:
            size_t line() const {
                return m_state->line();
            }

            size_t column() const {
                return m_state->column();
            }

            size_t length() const {
                return m_state->length();
            }
        public:
            TokenizerState snapshot() const {
                return m_state->snapshot();
            }

            void replaceState(const char* begin, const char* end) {
                m_state.reset(m_state->clone(begin, end));
            }

            void restore(const TokenizerState& snapshot) {
                m_state->restore(snapshot);
            }
        protected:
            size_t offset(const char* ptr) const {
                return m_state->offset(ptr);
            }

            const char* curPos() const {
                return m_state->curPos();
            }

            char curChar() const {
                if (eof()) {
                    return 0;
                }

                return *curPos();
            }

            char lookAhead(const size_t offset = 1) const {
                return m_state->lookAhead(offset);
            }

            void advance(const size_t offset) {
                m_state->advance(offset);
            }

            void advance() {
                m_state->advance();
            }

            bool isDigit(const char c) const {
                return c >= '0' && c <= '9';
            }

            bool isLetter(const char c) const {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            }

            bool isWhitespace(const char c) const {
                return isAnyOf(c, Whitespace());
            }

            bool isEscaped() const {
                return m_state->escaped();
            }

            const char* readInteger(const String& delims) {
                if (curChar() != '+' && curChar() != '-' && !isDigit(curChar())) {
                    return nullptr;
                }

                const auto previousState = *m_state;
                if (curChar() == '+' || curChar() == '-') {
                    advance();
                }
                while (!eof() && isDigit(curChar())) {
                    advance();
                }
                if (eof() || isAnyOf(curChar(), delims)) {
                    return curPos();
                }

                *m_state = previousState;
                return nullptr;
            }

            const char* readDecimal(const String& delims) {
                if (curChar() != '+' && curChar() != '-' && curChar() != '.' && !isDigit(curChar())) {
                    return nullptr;
                }

                const auto previousState = *m_state;
                if (curChar() != '.') {
                    advance();
                    readDigits();
                }

                if (curChar() == '.') {
                    advance();
                    readDigits();
                }

                if (curChar() == 'e') {
                    advance();
                    if (curChar() == '+' || curChar() == '-' || isDigit(curChar())) {
                        advance();
                        readDigits();
                    }
                }

                if (eof() || isAnyOf(curChar(), delims)) {
                    return curPos();
                }

                *m_state = previousState;
                return nullptr;
            }

        private:
            void readDigits() {
                while (!eof() && isDigit(curChar())) {
                    advance();
                }
            }
        protected:
            const char* readUntil(const String& delims) {
                if (!eof()) {
                    do {
                        advance();
                    } while (!eof() && !isAnyOf(curChar(), delims));
                }
                return curPos();
            }

            const char* readWhile(const String& allow) {
                while (!eof() && isAnyOf(curChar(), allow)) {
                    advance();
                }
                return curPos();
            }

            const char* readQuotedString(const char delim = '"', const String& hackDelims = "") {
                while (!eof() && (curChar() != delim || isEscaped())) {
                    // This is a hack to handle paths with trailing backslashes that get misinterpreted as escaped double quotation marks.
                    if (!hackDelims.empty() && curChar() == '"' && isEscaped() && hackDelims.find(lookAhead()) != String::npos) {
                        m_state->resetEscaped();
                        break;
                    }
                    advance();
                }
                errorIfEof();
                const char* end = curPos();
                advance();
                return end;
            }

            const char* discardWhile(const String& allow) {
                while (!eof() && isAnyOf(curChar(), allow)) {
                    advance();
                }
                return curPos();
            }

            const char* discardUntil(const String& delims) {
                while (!eof() && !isAnyOf(curChar(), delims)) {
                    advance();
                }
                return curPos();
            }

            bool matchesPattern(const String& pattern) const {
                if (pattern.empty() || isEscaped() || curChar() != pattern[0]) {
                    return false;
                }
                for (size_t i = 1; i < pattern.size(); ++i) {
                    if (lookAhead(i) != pattern[i]) {
                        return false;
                    }
                }
                return true;
            }

            const char* discardUntilPattern(const String& pattern) {
                if (pattern.empty()) {
                    return curPos();
                }

                while (!eof() && !matchesPattern(pattern)) {
                    advance();
                }

                if (eof()) {
                    return m_state->end();
                }

                return curPos();
            }

            const char* discard(const String& str) {
                for (size_t i = 0; i < str.size(); ++i) {
                    const char c = lookAhead(i);
                    if (c == 0 || c != str[i]) {
                        return nullptr;
                    }

                }

                advance(str.size());
                return curPos();
            }

            void errorIfEof() const {
                m_state->errorIfEof();
            }
        protected:
            bool isAnyOf(const char c, const String& allow) const {
                for (const auto& a : allow) {
                    if (c == a) {
                        return true;
                    }
                }
                return false;
            }

            virtual Token emitToken() = 0;
        };
    }
}
#endif
