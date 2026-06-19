#include <QtTest>
#include <QSignalSpy>

#include "QMonacoEditor.h"

/**
 * @brief Integration tests for QMonacoEditor.
 *
 * Each test drives a real QWebEngineView loading the embedded Monaco frontend.
 * Because the editor loads asynchronously and getters block on a nested event
 * loop, tests wait on editorReady() before asserting, and use QTRY_* macros to
 * spin the loop while the async JS round-trip settles.
 */
class TestQMonacoEditor : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void gettersReturnDefaultsBeforeReady();
    void setTextRoundTrips();
    void textChangedSignalEmitted();
    void readOnlyToggles();
    void cursorPositionRoundTrips();
    void setLanguageAndThemeDoNotCrash();

private:
    QMonacoEditor *m_editor = nullptr;
};

void TestQMonacoEditor::initTestCase() {
    m_editor = new QMonacoEditor();
    m_editor->resize(800, 600);
    m_editor->show();

    // The editor loads Monaco asynchronously; block until it signals ready
    // (or fail the whole suite if the frontend never loads).
    QSignalSpy readySpy(m_editor, &QMonacoEditor::editorReady);
    QVERIFY2(readySpy.wait(30000), "editorReady() did not fire within 30s");
}

void TestQMonacoEditor::cleanupTestCase() {
    delete m_editor;
    m_editor = nullptr;
}

void TestQMonacoEditor::gettersReturnDefaultsBeforeReady() {
    // A fresh editor that hasn't fired editorReady() yet: getters short-circuit
    // to default-constructed values and setters are no-ops (no crash, no hang).
    QMonacoEditor fresh;
    QCOMPARE(fresh.text(), QString());
    QCOMPARE(fresh.isReadOnly(), false);
    QCOMPARE(fresh.cursorLine(), 0);
    QCOMPARE(fresh.cursorColumn(), 0);
    fresh.setText(QStringLiteral("ignored"));
    QCOMPARE(fresh.text(), QString());
}

void TestQMonacoEditor::setTextRoundTrips() {
    const QString sample = QStringLiteral("int main() { return 0; }");
    m_editor->setText(sample);
    // setValue applies on the JS side asynchronously; spin until text() reflects it.
    QTRY_COMPARE(m_editor->text(), sample);
}

void TestQMonacoEditor::textChangedSignalEmitted() {
    QSignalSpy spy(m_editor, &QMonacoEditor::textChanged);
    const QString sample = QStringLiteral("line1\nline2\nline3");
    m_editor->setText(sample);

    QVERIFY(spy.wait(5000));
    // The last emission must carry the full new content.
    const QString emitted = spy.last().at(0).toString();
    QCOMPARE(emitted, sample);
    QCOMPARE(m_editor->text(), sample);
}

void TestQMonacoEditor::readOnlyToggles() {
    m_editor->setReadOnly(true);
    QTRY_VERIFY(m_editor->isReadOnly());
    m_editor->setReadOnly(false);
    QTRY_VERIFY(!m_editor->isReadOnly());
}

void TestQMonacoEditor::cursorPositionRoundTrips() {
    // Need enough lines/columns for the target position to be valid.
    m_editor->setText(QStringLiteral("aaaa\nbbbb\ncccc"));
    QTRY_COMPARE(m_editor->cursorLine() > 0, true);

    QSignalSpy spy(m_editor, &QMonacoEditor::cursorPositionChanged);
    m_editor->setCursorPosition(2, 3);

    QTRY_COMPARE(m_editor->cursorLine(), 2);
    QCOMPARE(m_editor->cursorColumn(), 3);
    QVERIFY(spy.count() >= 1);
}

void TestQMonacoEditor::setLanguageAndThemeDoNotCrash() {
    // No public getter for language/theme, so assert these apply without
    // disturbing observable state or crashing the round-trip.
    const QString sample = QStringLiteral("print('hi')");
    m_editor->setText(sample);
    QTRY_COMPARE(m_editor->text(), sample);

    m_editor->setLanguage(QStringLiteral("python"));
    m_editor->setTheme(QStringLiteral("vs"));

    // Content survives a language/theme switch.
    QTRY_COMPARE(m_editor->text(), sample);
}

QTEST_MAIN(TestQMonacoEditor)
#include "test_qmonacoeditor.moc"
