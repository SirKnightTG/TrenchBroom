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

#ifndef TrenchBroom_TrenchBroomApp
#define TrenchBroom_TrenchBroomApp

#include "Notifier.h"
#include "IO/Path.h"
#include "View/FrameManager.h"
#include "View/RecentDocuments.h"
#include "View/WelcomeWindow.h"

#include <memory>

#include <QApplication>

class QMenu;
class QSettings;

namespace TrenchBroom {
    class Logger;
    class RecoverableException;

    namespace View {
        class ExecutableEvent;

        class TrenchBroomApp : public QApplication {
            Q_OBJECT
        private:
            std::unique_ptr<FrameManager> m_frameManager;
            std::unique_ptr<RecentDocuments> m_recentDocuments;
            std::unique_ptr<WelcomeWindow> m_welcomeWindow; // must be destroyed before recent documents!
        public:
            Notifier<> recentDocumentsDidChangeNotifier;
        public:
            static TrenchBroomApp& instance();

            TrenchBroomApp(int& argc, char** argv);
        public:
            void parseCommandLineAndShowFrame();
            QSettings& settings();

            FrameManager* frameManager();

            const IO::Path::List& recentDocuments() const;
            void addRecentDocumentMenu(QMenu* menu);
            void removeRecentDocumentMenu(QMenu* menu);
            void updateRecentDocument(const IO::Path& path);

            bool openDocument(const IO::Path& path);
            bool recoverFromException(const RecoverableException& e, const std::function<bool()>& retry);
            void openPreferences();
            void openAbout();
            bool initializeGameFactory();
        public:
            bool newDocument();
            void openDocument();
            void showManual();
            void showPreferences();
            void showAboutDialog();
            void debugShowCrashReportDialog();

            bool notify(QObject* receiver, QEvent* event) override;

#ifdef __APPLE__
            bool event(QEvent* event) override;
#endif
            bool openFilesOrWelcomeFrame(const QStringList& fileNames);
        public:
            void showWelcomeWindow();
            void hideWelcomeWindow();
            void closeWelcomeWindow();
        private:
            static bool useSDI();
        };

        void setCrashReportGUIEnbled(bool guiEnabled);
        void reportCrashAndExit(const String &stacktrace, const String &reason);
        bool isReportingCrash();
    }
}

#endif /* defined(TrenchBroom_TrenchBroomApp) */
