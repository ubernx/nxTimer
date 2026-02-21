module;

#include <QGridLayout>
#include <QLabel>
#include <QTimer>
#include <QFont>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QMenu>
#include <QMouseEvent>
#include <QPoint>
#include <deque>
#include <vector>
#include <string>
#include <utility>
#include <cmath>
#include <algorithm>

export module GUIFrame;

import TimerWorker;
import Settings;

static QString formatTime(double seconds, int precision) {
    int totalSeconds = static_cast<int>(seconds);
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;
    double fractional = seconds - totalSeconds;

    // Truncate instead of round to avoid the displayed value being higher than actual
    double truncatedFractional = fractional;
    if (precision == 1) {
        truncatedFractional = std::floor(fractional * 10.0) / 10.0;
    }

    QString fractStr = QString::number(truncatedFractional, 'f', precision).mid(1); // Skip leading "0"
    return QString("%1:%2%3")
        .arg(minutes)
        .arg(secs, 2, 10, QChar('0'))
        .arg(fractStr);
}

export class GridWidget : public QWidget {
private:
    QGridLayout* layout;
    QLabel* totalTimeLabel;
    QLabel* segmentTimeLabel;
    QLabel* totalLabel;      // "Total:" label at the bottom
    QLabel* totalValueLabel; // The actual total time value

    std::vector<QLabel*> splitNameLabels;
    std::vector<QLabel*> splitTimeLabels;

    QTimer* refreshTimer;
    QFont boldFont;

    // Split management
    std::vector<std::pair<std::string, std::string>> immutableSplits;

    // Tracking state
    size_t lastObservedSplitIndex = 0;
    size_t windowStart = 0; // Index into immutableSplits for the top of the visible window
    static constexpr size_t WINDOW_SIZE = 11;
    std::vector<double> completedSplitTimes;
    double lastSplitTime = 0.0; // For segment calculation

    // For dragging the window
    QPoint dragPosition;
    bool isDragging = false;

public:
    GridWidget(QWidget* parent = nullptr) : QWidget(parent) {
        // Set frameless window hint to remove title bar and borders
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground, false);

        layout = new QGridLayout(this);
        layout->setSpacing(3); // smaller spacing so title and category are closer
        layout->setContentsMargins(10, 10, 10, 10);

        boldFont = QFont("Segoe UI", 14, QFont::Bold);

        // Timer font: double the size of the base bold font for the two main timers
        QFont timerFont = boldFont;
        timerFont.setPointSize(boldFont.pointSize() * 2);

        // Top group: keep title and category together so resizing won't separate them
        QWidget* topGroup = new QWidget(this);
        // Prevent the top group from being vertically stretched
        topGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        QVBoxLayout* topV = new QVBoxLayout(topGroup);
        topV->setSpacing(0); // tightened spacing between title and category
        topV->setContentsMargins(0, 0, 0, 0);

        QLabel* gameTitleLabel = new QLabel("S.T.A.L.K.E.R.: Shadow of Chernobyl", topGroup);
        gameTitleLabel->setFont(boldFont);
        gameTitleLabel->setStyleSheet("QLabel { color: white; }");
        gameTitleLabel->setAlignment(Qt::AlignHCenter);
        topV->addWidget(gameTitleLabel);

        QString categoryText = settings.category.empty() ? "-" : QString::fromStdString(settings.category);
        QLabel* categoryLabel = new QLabel(categoryText, topGroup);
        categoryLabel->setFont(boldFont);
        categoryLabel->setStyleSheet("QLabel { color: white; }");
        categoryLabel->setAlignment(Qt::AlignHCenter);
        topV->addWidget(categoryLabel);

        layout->addWidget(topGroup, 0, 0, 1, 2);

        // Spacer between category and total time (slightly smaller now)
        QLabel* spacerCatTotal = new QLabel("", this);
        spacerCatTotal->setFixedHeight(2); // reduced gap
        layout->addWidget(spacerCatTotal, 2, 0, 1, 2);

        // Total time label (row 3 now) - larger and right-centered
        totalTimeLabel = new QLabel("0:00.0", this);
        totalTimeLabel->setFont(timerFont);
        totalTimeLabel->setStyleSheet("QLabel { color: green; }");
        totalTimeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(totalTimeLabel, 3, 0, 1, 2);

        // Segment time label (row 4 now)
        if (settings.segment_time) {
            // Segment timer: larger and right-centered to match total timer
            segmentTimeLabel = new QLabel("0:00.0", this);
            segmentTimeLabel->setFont(timerFont);
            segmentTimeLabel->setStyleSheet("QLabel { color: #87CEEB; }");
            segmentTimeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            layout->addWidget(segmentTimeLabel, 4, 0, 1, 2);
        } else {
            segmentTimeLabel = nullptr;
        }

        // Copy splits into immutable storage
        immutableSplits = settings.splits;

        // Initialize splits table if enabled (startRow shifted to leave spacer after segment)
        int startRow = (settings.segment_time ? 6 : 4);
        size_t visibleCount = 0;
        if (settings.show_splits && !immutableSplits.empty()) {
            visibleCount = std::min(immutableSplits.size(), WINDOW_SIZE);
            for (size_t i = 0; i < visibleCount; ++i) {
                QLabel* nameLabel = new QLabel(QString::fromStdString(immutableSplits[i].first), this);
                nameLabel->setFont(boldFont);
                nameLabel->setStyleSheet("QLabel { color: white; }");
                layout->addWidget(nameLabel, startRow + static_cast<int>(i), 0);
                splitNameLabels.push_back(nameLabel);

                QLabel* timeLabel = new QLabel(QString::fromStdString(immutableSplits[i].second), this);
                timeLabel->setFont(boldFont);
                timeLabel->setStyleSheet("QLabel { color: white; }");
                timeLabel->setAlignment(Qt::AlignRight);
                layout->addWidget(timeLabel, startRow + static_cast<int>(i), 1);
                splitTimeLabels.push_back(timeLabel);
            }
        }

        // Place Total independently from splits: compute spacer row and total row
        int spacerRow;
        if (visibleCount > 0) {
            spacerRow = startRow + static_cast<int>(visibleCount); // spacer after last split
        } else {
            // No splits visible: place spacer after timers (leave one empty row)
            spacerRow = (settings.segment_time ? 5 : 4);
        }

        QLabel* spacerSplitsTotal = new QLabel("", this);
        spacerSplitsTotal->setFixedHeight(14); // controls how far Total is pushed down
        layout->addWidget(spacerSplitsTotal, spacerRow, 0, 1, 2);

        int totalRow = spacerRow + 1;
        totalLabel = new QLabel("Total:", this);
        totalLabel->setFont(boldFont);
        totalLabel->setStyleSheet("QLabel { color: #FFD700; }");
        totalLabel->setAlignment(Qt::AlignLeft);
        layout->addWidget(totalLabel, totalRow, 0);

        totalValueLabel = new QLabel("", this);
        totalValueLabel->setFont(boldFont);
        totalValueLabel->setStyleSheet("QLabel { color: #FFD700; }");
        totalValueLabel->setAlignment(Qt::AlignRight);
        layout->addWidget(totalValueLabel, totalRow, 1);

        setLayout(layout);
        setWindowTitle("nxTimer - S.T.A.L.K.E.R. SoC");

        // Hardlock the window size
        setFixedSize(340, 600);

        setStyleSheet("QWidget { background-color: black; }");

        // Setup 10Hz refresh timer (100ms intervals)
        refreshTimer = new QTimer(this);
        connect(refreshTimer, &QTimer::timeout, this, &GridWidget::updateDisplay);
        refreshTimer->start(100);
    }

protected:
    // Override mouse events for dragging and context menu
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            isDragging = true;
            dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (isDragging && (event->buttons() & Qt::LeftButton)) {
            move(event->globalPosition().toPoint() - dragPosition);
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            isDragging = false;
            event->accept();
        }
    }

    void contextMenuEvent(QContextMenuEvent* event) override {
        QMenu contextMenu(this);

        // Style the context menu with white background and black text
        contextMenu.setStyleSheet(
            "QMenu {"
            "    background-color: white;"
            "    color: black;"
            "    border: 1px solid #cccccc;"
            "}"
            "QMenu::item {"
            "    padding: 5px 20px;"
            "}"
            "QMenu::item:selected {"
            "    background-color: #0078d7;"
            "    color: white;"
            "}"
        );

        QAction* minimizeAction = contextMenu.addAction("Minimize");
        QAction* closeAction = contextMenu.addAction("Close");

        connect(minimizeAction, &QAction::triggered, this, &QWidget::showMinimized);
        connect(closeAction, &QAction::triggered, this, &QWidget::close);

        contextMenu.exec(event->globalPos());
    }

private:
    // Rebuild all visible split labels from the current windowStart
    void rebuildSplitLabels() {
        size_t visibleCount = splitNameLabels.size();
        for (size_t i = 0; i < visibleCount; ++i) {
            size_t splitIdx = windowStart + i; // index into immutableSplits (0-based)
            if (splitIdx >= immutableSplits.size()) break;

            splitNameLabels[i]->setText(QString::fromStdString(immutableSplits[splitIdx].first));

            // completedSplitTimes is indexed by lastObservedSplitIndex (1-based split index)
            // split 1 (immutableSplits[0]) completes at completedSplitTimes[0], etc.
            if (splitIdx < completedSplitTimes.size()) {
                double completedTime = completedSplitTimes[splitIdx];
                double displayTime;
                if (settings.splits_total) {
                    displayTime = completedTime;
                } else {
                    double prevTime = (splitIdx > 0) ? completedSplitTimes[splitIdx - 1] : 0.0;
                    displayTime = completedTime - prevTime;
                }
                splitTimeLabels[i]->setText(formatTime(displayTime, 3));
            } else {
                splitTimeLabels[i]->setText(QString::fromStdString(immutableSplits[splitIdx].second));
            }
        }
    }

    void updateDisplay() {
        double totalTime = timerState.accumulatedTime.load();
        bool isRunning = timerState.timerRunning.load();
        bool isPaused = timerState.gameTimePaused.load();
        bool displayTotal = timerState.displayTotal.load();
        size_t currentSplitIndex = timerState.currentSplitIndex.load();

        // Update total time display
        totalTimeLabel->setText(formatTime(totalTime, 1));
        if (isRunning && !isPaused) {
            totalTimeLabel->setStyleSheet("QLabel { color: #39FF14; }");
        } else {
            totalTimeLabel->setStyleSheet("QLabel { color: green; }");
        }

        // Update segment time display
        if (segmentTimeLabel) {
            double segmentTime = totalTime - lastSplitTime;
            segmentTimeLabel->setText(formatTime(segmentTime, 1));
            if (isRunning && !isPaused) {
                segmentTimeLabel->setStyleSheet("QLabel { color: #00BFFF; }");
            } else {
                segmentTimeLabel->setStyleSheet("QLabel { color: #4169E1; }");
            }
        }

        // Update Total label visibility
        if (totalValueLabel) {
            if (displayTotal) {
                totalValueLabel->setText(formatTime(totalTime, 3));
            } else {
                totalValueLabel->setText("");
            }
        }

        // Reset tracking on timer reset (currentSplitIndex == 0)
        if (currentSplitIndex == 0 && lastObservedSplitIndex > 0) {
            lastObservedSplitIndex = 0;
            lastSplitTime = 0.0;
            windowStart = 0;
            completedSplitTimes.clear();

            if (settings.show_splits) rebuildSplitLabels();
            return;
        }

        if (!settings.show_splits) return;

        // Handle undo: currentSplitIndex decreased
        if (currentSplitIndex < lastObservedSplitIndex) {
            while (lastObservedSplitIndex > currentSplitIndex) {
                lastObservedSplitIndex--;

                // Pop the last completed split time
                if (!completedSplitTimes.empty()) completedSplitTimes.pop_back();
                lastSplitTime = completedSplitTimes.empty() ? 0.0 : completedSplitTimes.back();

                // Scroll window back if needed:
                // windowStart should be max(0, lastObservedSplitIndex - WINDOW_SIZE + 1)
                // but only go back if we were scrolled forward
                size_t desiredWindowStart = (lastObservedSplitIndex >= WINDOW_SIZE)
                    ? lastObservedSplitIndex - WINDOW_SIZE + 1
                    : 0;
                // Only scroll back, never forward during undo
                if (desiredWindowStart < windowStart) {
                    windowStart = desiredWindowStart;
                }
            }
            rebuildSplitLabels();
            return;
        }

        // Handle forward splits
        while (lastObservedSplitIndex < currentSplitIndex && lastObservedSplitIndex < immutableSplits.size()) {
            // Record the completed split time (index into completedSplitTimes == immutableSplits index)
            completedSplitTimes.push_back(totalTime);

            // Update the label for this split
            size_t labelIdx = lastObservedSplitIndex - windowStart;
            if (labelIdx < splitTimeLabels.size()) {
                double displayTime;
                if (settings.splits_total) {
                    displayTime = totalTime;
                } else {
                    displayTime = totalTime - lastSplitTime;
                }
                splitTimeLabels[labelIdx]->setText(formatTime(displayTime, 3));
            }

            lastSplitTime = totalTime;
            lastObservedSplitIndex++;

            // Scroll forward: once we've completed splits beyond WINDOW_SIZE,
            // advance the window so the next upcoming split is always visible at the bottom
            if (lastObservedSplitIndex >= windowStart + WINDOW_SIZE &&
                windowStart + WINDOW_SIZE < immutableSplits.size()) {
                windowStart++;
                rebuildSplitLabels();
            }
        }
    }
};
