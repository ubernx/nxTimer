#include <QApplication>
#include <thread>

import Settings;
import GameMemory;
import TimerWorker;
import GUIFrame;

int main(int argc, char** argv) {

    setupSettings(loadSettings()); // valid setup is guaranteed by this call, even if the user provides invalid settings
    setupVersionOffsets(); // might fail but timerworker module has its own extra check for this

    std::thread workerThread([] { TimerWorker(); }); // Start TimerWorker on background thread
    workerThread.detach(); // runs for program lifetime

    QApplication app(argc, argv);
    GridWidget widget;
    widget.show();

    return app.exec();

}