#include <QApplication>
#include <QtWidgets/QMainWindow>
#include <QImage>
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#include <iostream>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    auto *window = new QMainWindow(nullptr);
    window->setMinimumSize(800, 600);

    QGraphicsScene scene;

    auto *view = new QGraphicsView(&scene, window);

    const auto pattern = QFileInfo(QCoreApplication::arguments().at(1));
    const auto root = pattern.dir();
    const auto file = pattern.fileName();

    auto *watcher = new QFileSystemWatcher(window);
    watcher->addPath(root.absolutePath());
    QWidget::connect(watcher, &QFileSystemWatcher::directoryChanged, [](const QString &path) {
        std::cerr << path.toStdString();
    });

    auto *item = new QGraphicsPixmapItem(QPixmap("/tmp/a1-1.png"));
    item->setTransformationMode(Qt::SmoothTransformation);
    scene.addItem(item);

    auto *item2 = new QGraphicsPixmapItem(QPixmap("/tmp/a1-2.png"));
    item2->setTransformationMode(Qt::SmoothTransformation);
    item2->setOffset(item->boundingRect().bottomLeft());
    scene.addItem(item2);

    auto *zoomIn = new QAction(view);
    zoomIn->setShortcut(Qt::Key_Equal);
    QWidget::connect(zoomIn, &QAction::triggered, [=] {
        view->scale(1.1, 1.1);
    });

    auto *zoomOut = new QAction(view);
    zoomOut->setShortcut(Qt::Key_Minus);
    QWidget::connect(zoomOut, &QAction::triggered, [=] {
        view->scale(0.909090909090909, 0.909090909090909);
    });

    auto *quit = new QAction(window);
    quit->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    QWidget::connect(quit, &QAction::triggered, window, &QMainWindow::close);

    view->addAction(zoomIn);
    view->addAction(zoomOut);

    window->addAction(quit);
    window->setCentralWidget(view);
    window->show();
    return QCoreApplication::exec();
}
