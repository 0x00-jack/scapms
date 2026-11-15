#include "gui/MainWindow.h"
#include "gui/Ui.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SCAPMS");
    app.setApplicationVersion("1.0");
    app.setStyle("Fusion");
    app.setStyleSheet(clinic::gui::styleSheet());

    clinic::gui::MainWindow window(app.arguments().value(1));
    window.show();
    return app.exec();
}
