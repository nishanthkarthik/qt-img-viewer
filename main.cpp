#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QLoggingCategory>
#include <QScrollBar>
#include <QtLogging>
#include <QtWidgets/QMainWindow>

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

    struct ImgState {
    public:
        explicit ImgState(qsizetype idx, QGraphicsPixmapItem *item, QString file)
                : m_idx{idx}, m_pixMap{item},
                  m_file{std::move(file)} {
            qInfo(cat) << "Adding file " << idx << " with offset " << item->boundingRect().bottomLeft();
        }

        void refresh(QGraphicsView *view) {
            qInfo(cat) << "Refreshing" << m_idx;

            auto file = QFile(m_file);
            if (!file.open(QIODevice::OpenModeFlag::ReadOnly)) return;
            const auto bytes = file.readAll();

            if (bytes.size() <= 16) {
                qInfo(cat) << "Skipping file re-render: Empty file";
                return;
            }

            if (bytes.sliced(bytes.size() - 8, 4) != QByteArrayLiteral("\x49\x45\x4E\x44")) {
                qInfo(cat) << "Skipping file re-render: Missing IEND footer";
                return;
            }

            const auto newHash = QCryptographicHash::hash(bytes, QCryptographicHash::Algorithm::Sha1);
            if (newHash == m_hash) {
                qInfo(cat) << "Skipping image update for" << m_idx;
                return;
            }
            qInfo(cat) << "Performing image update for" << m_idx;
            m_pixMap->setPixmap(m_file);
            view->invalidateScene(m_pixMap->boundingRect());
            m_hash = newHash;
        }

        [[nodiscard]] QRectF boundingRect() const {
            return m_pixMap->boundingRect();
        }

    private:
        qsizetype m_idx;
        QGraphicsPixmapItem *m_pixMap;
        QString m_file;
        QByteArray m_hash;
    };

    static QVector<ImgState> states;

    auto *watcher = new QFileSystemWatcher(window);
    watcher->addPath(root.absolutePath());
    const auto refreshWatchlist = [=](const QString &path) {
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

        for (qsizetype c = states.size(); c < fileCount; ++c) {
            const auto fileName = makeFilename(c + 1);
            auto *item = new QGraphicsPixmapItem(fileName);
            item->setTransformationMode(Qt::SmoothTransformation);
            auto offset = QPointF(0, 10);
            if (!states.empty()) {
                offset += states.last().boundingRect().bottomLeft();
            }
            item->setOffset(offset);
            states.append(ImgState{c + 1, item, fileName});
            scene->addItem(item);
        }
    };
    QWidget::connect(watcher, &QFileSystemWatcher::directoryChanged, refreshWatchlist);

    QWidget::connect(watcher, &QFileSystemWatcher::fileChanged, [=](const QString &path) {
        for (qsizetype i = 1; i <= fileCount; ++i) {
            if (QFileInfo(path).absoluteFilePath() == QFileInfo(makeFilename(i)).absoluteFilePath()) {
                states[i - 1].refresh(view);
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
    refreshWatchlist(root.path());
    return QCoreApplication::exec();
}
