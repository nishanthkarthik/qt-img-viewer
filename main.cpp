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

    wuffs_aux::DecodeImageResult load_wuffs_image(uint8_t *ptr, size_t len) {
        wuffs_aux::DecodeImageCallbacks callbacks;
        wuffs_aux::sync_io::MemoryInput input(ptr, len);
        wuffs_aux::DecodeImageResult result = wuffs_aux::DecodeImage(callbacks, input);
        return result;
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

    static auto root = pattern.dir();
    static auto filePattern = pattern.fileName();
    static int fileCount = 0;
    static int width = 1;

    static const auto makeFilename = [](size_t idx) {
        return root.filePath(QString(filePattern)
                                     .replace(QStringLiteral("{n}"),
                                              QStringLiteral("%1").arg(idx,
                                                                       width,
                                                                       10,
                                                                       QChar('0'))));
    };

    struct ImgState {
    public:
        explicit ImgState(size_t idx, QGraphicsPixmapItem *item)
                : m_idx{idx}, m_pixMap{item}, m_store("Empty") {
            qInfo(cat) << "Adding file " << idx << " with offset " << item->boundingRect().bottomLeft();
        }

        void fetch(uchar *bytesPtr, size_t size) {
            qInfo(cat) << "Performing image update for" << m_idx;
            m_store = load_wuffs_image(bytesPtr, size);
            auto plane = m_store.pixbuf.plane(0);
            m_pixMap->setPixmap(
                    QPixmap::fromImage(QImage(plane.ptr, plane.width / 4, plane.height, QImage::Format_RGBA8888)));
            qInfo(cat) << "Loaded image";
        }

        void fetch() {
            auto file = QFile(fileName());
            file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
            auto bytesPtr = file.map(0, file.size());
            const auto unmapFn = Defer{[&] { file.unmap(bytesPtr); }};
            fetch(bytesPtr, file.size());
        }

        void refresh(QGraphicsView *view) {
            qInfo(cat) << "Refreshing" << m_idx;

            auto file = QFile(fileName());
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

            fetch(bytesPtr, size);

            qInfo(cat) << "Invalidating scene" << m_idx;
            view->invalidateScene(m_pixMap->boundingRect(), QGraphicsScene::ItemLayer);

            m_hash = newHash;
            qInfo(cat) << "Update finished" << m_idx;
        }

        [[nodiscard]] QRectF boundingRect() const {
            return m_pixMap->boundingRect();
        }

        [[nodiscard]] QString fileName() const {
            return makeFilename(m_idx);
        }

    private:
        size_t m_idx;
        QGraphicsPixmapItem *m_pixMap;
        QByteArray m_hash;
        wuffs_aux::DecodeImageResult m_store;
    };

    static std::vector<ImgState> states;

    auto *watcher = new QFileSystemWatcher(window);
    watcher->addPath(root.absolutePath());
    const auto refreshWatchlist = [=](const QString &path) {
        while (!QFileInfo::exists(makeFilename(1))) {
            ++width;
        }

        qInfo(cat) << "Detected padding with width" << width;

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

        for (size_t c = states.size(); c < fileCount; ++c) {
            auto *item = new QGraphicsPixmapItem();
            item->setTransformationMode(Qt::SmoothTransformation);
            auto offset = QPointF(0, 10);
            if (!states.empty()) {
                offset += states.at(states.size() - 1).boundingRect().bottomLeft();
            }
            item->setOffset(offset);
            auto state = ImgState{c + 1, item};
            state.fetch();
            states.emplace_back(std::move(state));
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
