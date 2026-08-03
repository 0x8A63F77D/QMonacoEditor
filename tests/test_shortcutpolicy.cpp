#include <QtTest>
#include <QSignalSpy>
#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QKeySequence>
#include <QLineEdit>
#include <QMainWindow>
#include <QTimer>
#include <QWebEngineView>
#include <QWebEnginePage>

#include <memory>

#include "QMonacoEditor.h"

/**
 * @brief Arbitration tests for QMonacoEditor::setShortcutPolicy().
 *
 * A QMainWindow holds host QActions on Ctrl+F, Ctrl+Z and Ctrl+S and embeds one
 * editor. Keys are injected with QTest::keyClick() on the window handle, which
 * goes through QWindowSystemInterface -- the same entry point the Windows
 * platform plugin uses for real key presses.
 *
 * Every assertion settles on observable content: the host side on QSignalSpy
 * counts for QAction::triggered, the editor side on QMonacoEditor's synchronous
 * getters and on the find widget's DOM state. Both editor-side reads travel to
 * the render process over the same FIFO channel the key events use, so a read
 * that has returned proves any earlier key has already been processed there --
 * no sleeps or retries are needed.
 */
class TestShortcutPolicy : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void defaultPolicyIsQtDefault();
    void qtDefaultLetsHostTakeFindAndEditorTakeUndo();
    void hostShortcutsWinTakesEvenEditingChords();
    void editorFirstTakesFindAwayFromTheHost();
    void editorFirstStillFallsBackToHostForUnboundChords();
    void policyDoesNotGovernWidgetsReparentedOutOfTheView();

private:
    /// The widget that actually receives ShortcutOverride: the view's focus proxy.
    QWidget *renderWidget() const;
    /// Gives the render widget Qt focus and Monaco DOM focus, then verifies both.
    void focusEditor();
    /// Blocking JS evaluation in the page, mirroring QMonacoEditor's own getters.
    QVariant evalJs(const QString &expr) const;
    bool findWidgetVisible() const;
    QWindow *keyTarget() const { return m_window->windowHandle(); }

    QMainWindow *m_window = nullptr;
    QMonacoEditor *m_editor = nullptr;
    QAction *m_findAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_saveAction = nullptr;
};

void TestShortcutPolicy::initTestCase() {
    m_window = new QMainWindow();
    m_editor = new QMonacoEditor();
    m_window->setCentralWidget(m_editor);

    // Host actions on the window. #7 measured that ShortcutContext makes no
    // difference to the arbitration, so one context is enough here.
    const auto addAction = [this](const char *name, const QKeySequence &keys) {
        auto *action = new QAction(QString::fromLatin1(name), m_window);
        action->setShortcut(keys);
        action->setShortcutContext(Qt::WindowShortcut);
        m_window->addAction(action);
        return action;
    };
    m_findAction = addAction("HostFind", QKeySequence(QStringLiteral("Ctrl+F")));
    m_undoAction = addAction("HostUndo", QKeySequence(QStringLiteral("Ctrl+Z")));
    m_saveAction = addAction("HostSave", QKeySequence(QStringLiteral("Ctrl+S")));

    // Constructed before anything pumps the event loop: qWaitForWindowActive() below
    // does, and QSignalSpy::wait() only sees emissions that happen after it exists, so
    // an editorReady() delivered during activation would otherwise be missed.
    QSignalSpy readySpy(m_editor, &QMonacoEditor::editorReady);

    m_window->resize(800, 600);
    m_window->show();
    QVERIFY2(QTest::qWaitForWindowActive(m_window), "window never became active");

    QVERIFY2(readySpy.count() > 0 || readySpy.wait(30000),
             "editorReady() did not fire within 30s");
}

void TestShortcutPolicy::cleanupTestCase() {
    delete m_window;
    m_window = nullptr;
    m_editor = nullptr;
}

void TestShortcutPolicy::init() {
    m_editor->setShortcutPolicy(QMonacoEditor::ShortcutPolicy::QtDefault);
    m_editor->setText(QStringLiteral("alpha"));
    QTRY_COMPARE(m_editor->text(), QStringLiteral("alpha"));

    // A find widget left open by an earlier test would mask the next one.
    evalJs(QStringLiteral("window.__monacoEditor?.trigger('qtest','closeFindWidget')"));
    QTRY_VERIFY(!findWidgetVisible());

    focusEditor();
}

QWidget *TestShortcutPolicy::renderWidget() const {
    auto *view = m_editor->findChild<QWebEngineView *>();
    return view ? view->focusProxy() : nullptr;
}

void TestShortcutPolicy::focusEditor() {
    // The focus proxy is created with the render widget, i.e. after construction.
    QTRY_VERIFY(renderWidget() != nullptr);
    renderWidget()->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), renderWidget());

    // setCursorPosition() also calls editor.focus() on the JS side, which puts DOM
    // focus on Monaco's hidden textarea -- the state WebEngine's Qt::ImEnabled gate
    // inspects when it decides whether to claim a chord.
    m_editor->setCursorPosition(1, 1);
    QTRY_COMPARE(m_editor->cursorColumn(), 1);
    QTRY_VERIFY(evalJs(QStringLiteral(
        "document.activeElement?.classList?.contains('inputarea')")).toBool());
}

QVariant TestShortcutPolicy::evalJs(const QString &expr) const {
    auto *view = m_editor->findChild<QWebEngineView *>();
    if (!view) {
        return {};
    }
    // Heap-owned state, shared with the callback: on the timeout path below this
    // function returns while the callback is still registered, and a late arrival must
    // not write through references into a dead stack frame. Same reasoning as
    // QMonacoEditor::evalJsSync().
    struct EvalState {
        QEventLoop loop;
        QVariant result;
    };
    auto state = std::make_shared<EvalState>();
    view->page()->runJavaScript(expr, [state](const QVariant &value) {
        state->result = value;
        state->loop.quit();
    });
    QTimer::singleShot(5000, &state->loop, &QEventLoop::quit);
    state->loop.exec();
    return state->result;
}

bool TestShortcutPolicy::findWidgetVisible() const {
    return evalJs(QStringLiteral(
        "!!document.querySelector('.monaco-editor .find-widget.visible')")).toBool();
}

void TestShortcutPolicy::defaultPolicyIsQtDefault() {
    // A freshly constructed editor must not change arbitration for existing code.
    QMonacoEditor fresh;
    QCOMPARE(fresh.shortcutPolicy(), QMonacoEditor::ShortcutPolicy::QtDefault);

    fresh.setShortcutPolicy(QMonacoEditor::ShortcutPolicy::EditorFirst);
    QCOMPARE(fresh.shortcutPolicy(), QMonacoEditor::ShortcutPolicy::EditorFirst);
}

void TestShortcutPolicy::qtDefaultLetsHostTakeFindAndEditorTakeUndo() {
    QSignalSpy findSpy(m_findAction, &QAction::triggered);
    QSignalSpy undoSpy(m_undoAction, &QAction::triggered);
    QSignalSpy saveSpy(m_saveAction, &QAction::triggered);

    // Ctrl+F is not one of Qt's text-editing keys, so the web view leaves the
    // ShortcutOverride unaccepted and the host QAction wins.
    QTest::keyClick(keyTarget(), Qt::Key_F, Qt::ControlModifier);
    QTRY_COMPARE(findSpy.count(), 1);
    QVERIFY(!findWidgetVisible());

    // Ctrl+S likewise: Monaco has no binding for it and Qt does not treat it as an
    // editing key, so it reaches the host.
    QTest::keyClick(keyTarget(), Qt::Key_S, Qt::ControlModifier);
    QTRY_COMPARE(saveSpy.count(), 1);

    // Ctrl+Z is in Qt's edit-command list, so the web view accepts the override and
    // the host QAction never fires; Monaco undoes the typed character instead.
    QTest::keyClick(keyTarget(), 'x');
    QTRY_COMPARE(m_editor->text(), QStringLiteral("xalpha"));
    QTest::keyClick(keyTarget(), Qt::Key_Z, Qt::ControlModifier);
    QTRY_COMPARE(m_editor->text(), QStringLiteral("alpha"));
    QCOMPARE(undoSpy.count(), 0);
}

void TestShortcutPolicy::hostShortcutsWinTakesEvenEditingChords() {
    m_editor->setShortcutPolicy(QMonacoEditor::ShortcutPolicy::HostShortcutsWin);

    QSignalSpy findSpy(m_findAction, &QAction::triggered);
    QSignalSpy undoSpy(m_undoAction, &QAction::triggered);
    QSignalSpy saveSpy(m_saveAction, &QAction::triggered);

    // Type one character first: the undo assertion below needs something to undo, and
    // this also shows plain keys are untouched -- only chords bound to a QAction are.
    QTest::keyClick(keyTarget(), 'x');
    QTRY_COMPARE(m_editor->text(), QStringLiteral("xalpha"));

    // The chords the host already won under QtDefault keep working.
    QTest::keyClick(keyTarget(), Qt::Key_F, Qt::ControlModifier);
    QTRY_COMPARE(findSpy.count(), 1);
    QTest::keyClick(keyTarget(), Qt::Key_S, Qt::ControlModifier);
    QTRY_COMPARE(saveSpy.count(), 1);

    // The difference from QtDefault: Ctrl+Z is an editing chord the web view would
    // normally claim, and now the host takes it instead.
    QTest::keyClick(keyTarget(), Qt::Key_Z, Qt::ControlModifier);
    QTRY_COMPARE(undoSpy.count(), 1);
    // Nothing was undone. text() is a blocking round-trip to the render process over the
    // same channel key events use, so had the chord been delivered it would already show.
    QCOMPARE(m_editor->text(), QStringLiteral("xalpha"));
    QVERIFY(!findWidgetVisible());
}

void TestShortcutPolicy::editorFirstTakesFindAwayFromTheHost() {
    m_editor->setShortcutPolicy(QMonacoEditor::ShortcutPolicy::EditorFirst);

    QSignalSpy findSpy(m_findAction, &QAction::triggered);
    QSignalSpy undoSpy(m_undoAction, &QAction::triggered);

    // The difference from QtDefault: Ctrl+F reaches the page, Monaco opens its find
    // widget, and the host QAction does not fire.
    QTest::keyClick(keyTarget(), Qt::Key_F, Qt::ControlModifier);
    QTRY_VERIFY(findWidgetVisible());
    QCOMPARE(findSpy.count(), 0);

    // Focus moved into the find widget's input; put it back on the editor text.
    evalJs(QStringLiteral("window.__monacoEditor?.trigger('qtest','closeFindWidget')"));
    QTRY_VERIFY(!findWidgetVisible());

    // Editing chords still behave as they did: the editor keeps them.
    QTest::keyClick(keyTarget(), 'x');
    QTRY_COMPARE(m_editor->text(), QStringLiteral("xalpha"));
    QTest::keyClick(keyTarget(), Qt::Key_Z, Qt::ControlModifier);
    QTRY_COMPARE(m_editor->text(), QStringLiteral("alpha"));
    QCOMPARE(undoSpy.count(), 0);
}

void TestShortcutPolicy::editorFirstStillFallsBackToHostForUnboundChords() {
    m_editor->setShortcutPolicy(QMonacoEditor::ShortcutPolicy::EditorFirst);

    QSignalSpy saveSpy(m_saveAction, &QAction::triggered);

    // Ctrl+S is offered to the editor first, but Monaco binds nothing to it, so the
    // render process reports it unhandled and WebEngine forwards it back to the Qt
    // parent chain -- where the host QAction picks it up. This is what makes
    // "editor first, host as fallback" work without the host enumerating Monaco's
    // bindings, so it is asserted rather than assumed.
    QTest::keyClick(keyTarget(), Qt::Key_S, Qt::ControlModifier);
    QTRY_COMPARE(saveSpy.count(), 1);
    QCOMPARE(m_editor->text(), QStringLiteral("alpha"));
}

void TestShortcutPolicy::policyDoesNotGovernWidgetsReparentedOutOfTheView() {
    auto *view = m_editor->findChild<QWebEngineView *>();
    QVERIFY(view != nullptr);

    // A widget born below the web view gets the shortcut filter installed on it. An
    // installed event filter does not come off by itself when the widget is later
    // reparented elsewhere, so the policy must not keep governing it: it is no longer
    // part of the editor, and host shortcuts there have to behave normally.
    auto *stray = new QLineEdit(view);
    stray->setParent(m_window);
    stray->show();
    stray->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), stray);

    m_editor->setShortcutPolicy(QMonacoEditor::ShortcutPolicy::EditorFirst);
    QSignalSpy findSpy(m_findAction, &QAction::triggered);
    QTest::keyClick(keyTarget(), Qt::Key_F, Qt::ControlModifier);
    QTRY_COMPARE(findSpy.count(), 1);

    delete stray;
}

QTEST_MAIN(TestShortcutPolicy)
#include "test_shortcutpolicy.moc"
