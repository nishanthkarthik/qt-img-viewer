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

#include "wuffs-unsupported-snapshot.cc"

namespace {
    const auto cat = QLoggingCategory("img-viewer");

    template<typename Fn>
    class Defer {
    public:
        explicit Defer(Fn fn) : m_fn(std::move(fn)) {}

        Defer(const Defer &) = delete;

        Defer(Defer &&) = delete;

        Defer &operator=(const Defer &) = delete;

        Defer &operator=(Defer &&) = delete;

        ~Defer() noexcept(false) { m_fn(); }

    private:
        Fn m_fn;
    };

    void load_wuffs_image(uint8_t *ptr, size_t len, size_t w, size_t h) {
        wuffs_base__io_buffer buffer{wuffs_base__slice_u8{ptr, len},
                                     wuffs_base__io_buffer_meta{w, h, 0, true}};
    }
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

    static int fileCount = 0;

    const auto makeFilename = [=](qsizetype idx) {
        const auto width = QCoreApplication::arguments().at(2).toInt();
        return root.filePath(QString(filePattern)
                                     .replace(QStringLiteral("{n}"),
                                              QStringLiteral("%1").arg(idx,
                                                                       width,
                                                                       10,
                                                                       QLatin1Char('0'))));
    };

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

            const auto size = file.size();

            if (size <= 16) {
                qInfo(cat) << "Skipping file re-render: Empty file";
                return;
            }

            {
                const auto seekBack = Defer{[&] { file.seek(0); }};
                file.seek(size - 8);

                if (file.read(4) != QByteArrayLiteral("\x49\x45\x4E\x44")) {
                    qInfo(cat) << "Skipping file re-render: Missing IEND footer";
                    return;
                }
            }

            const auto bytesPtr = file.map(0, size);
            const auto unmapFn = Defer{[&] { file.unmap(bytesPtr); }};
            const auto newHash = QCryptographicHash::hash(QByteArrayView(bytesPtr, size),
                                                          QCryptographicHash::Algorithm::Sha1);
            if (newHash == m_hash) {
                qInfo(cat) << "Skipping image update for" << m_idx;
                return;
            }
            qInfo(cat) << "Performing image update for" << m_idx;
            m_pixMap->setPixmap(m_file);
            qInfo(cat) << "Invalidating scene" << m_idx;
            view->invalidateScene(m_pixMap->boundingRect());
            m_hash = newHash;
            qInfo(cat) << "Update finished" << m_idx;
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
