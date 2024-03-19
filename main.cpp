#include <QApplication>
#include <QtWidgets/QMainWindow>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QScrollBar>
#include <QDir>
#include <QtLogging>
#include <QLoggingCategory>

namespace {
    const auto cat = QLoggingCategory("img-viewer");
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    auto *window = new QMainWindow(nullptr);
    window->setMinimumSize(400, 300);

    auto *scene = new QGraphicsScene(window);
    scene->setBackgroundBrush(Qt::darkGray);

    auto *view = new QGraphicsView(scene, window);
    view->setDragMode(QGraphicsView::DragMode::ScrollHandDrag);

    const auto pattern = QFileInfo(QCoreApplication::arguments().at(1));
    const auto root = pattern.dir();
    const auto filePattern = pattern.fileName();

    const auto makeFilename = [=](qsizetype idx) {
        return root.filePath(QString(filePattern).replace(QStringLiteral("{n}"), QString::number(idx)));
    };

    static int fileCount = 0;
    static QVector<QGraphicsPixmapItem *> pixMaps;

    auto *watcher = new QFileSystemWatcher(window);
    watcher->addPath(root.absolutePath());
    QWidget::connect(watcher, &QFileSystemWatcher::directoryChanged, [=](const QString &path) {
        int fileIdx = 1;
        QStringList validFiles;
        while ([&] {
            auto info = QFile(makeFilename(fileIdx));
            if (info.exists()) { validFiles << info.fileName(); }
            return info.exists();
        }()) {
            ++fileIdx;
        }
        fileCount = fileIdx - 1;

        for (const auto &file: watcher->files()) {
            if (!QFile(file).exists()) {
                watcher->removePath(file);
            }
        }

        watcher->addPaths(validFiles);

        for (qsizetype c = pixMaps.size(); c < fileCount; ++c) {
            auto *item = new QGraphicsPixmapItem(makeFilename(c + 1));
            item->setTransformationMode(Qt::SmoothTransformation);
            auto offset = QPointF(0, 10);
            if (!pixMaps.empty()) {
                offset += pixMaps.last()->boundingRect().bottomLeft();
            }
            item->setOffset(offset);
            qInfo(cat) << "Adding file " << c << " with offset " << item->boundingRect().bottomLeft();
            pixMaps.append(item);
            scene->addItem(item);
        }
    });

    QWidget::connect(watcher, &QFileSystemWatcher::fileChanged, [=](const QString &path) {
        for (qsizetype i = 1; i <= fileCount; ++i) {
            if (QFileInfo(path).absoluteFilePath() == QFileInfo(makeFilename(i)).absoluteFilePath()) {
                qInfo(cat) << "Reloading image " << i;
                pixMaps[i - 1]->setPixmap(path);
                view->invalidateScene(pixMaps[i - 1]->boundingRect());
            }
        }
    });

    auto *zoomIn = new QAction(view);
    zoomIn->setShortcut(Qt::Key_Equal);
    QWidget::connect(zoomIn, &QAction::triggered, [=] {
        view->scale(1.1, 1.1);
    });

    auto *zoomOut = new QAction(view);
    zoomOut->setShortcut(Qt::Key_Minus);
    QWidget::connect(zoomOut, &QAction::triggered, [=] {
        view->scale(1.0 / 1.1, 1.0 / 1.1);
    });

    auto *quit = new QAction(window);
    quit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    QWidget::connect(quit, &QAction::triggered, window, &QMainWindow::close);

    view->addAction(zoomIn);
    view->addAction(zoomOut);

    window->addAction(quit);
    window->setCentralWidget(view);
    window->show();
    return QCoreApplication::exec();
}
