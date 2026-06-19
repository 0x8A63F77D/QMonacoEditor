#include <QtTest>
#include <QSignalSpy>

#include "MonacoBridge.h"

/**
 * @brief Unit tests for MonacoBridge's JS-to-C++ signal forwarding.
 *
 * The bridge's contract is that each on* slot (invoked from JS over the web
 * channel) re-emits the matching plain signal verbatim. These tests call the
 * slots directly and assert the signals fire with the arguments passed through
 * unchanged. No WebEngine involved, so this runs fast and headless.
 */
class TestMonacoBridge : public QObject {
    Q_OBJECT

private slots:
    void onEditorReadyEmitsSignal();
    void onTextChangedForwardsText();
    void onCursorPositionChangedForwardsLineColumn();
};

void TestMonacoBridge::onEditorReadyEmitsSignal() {
    MonacoBridge bridge;
    QSignalSpy spy(&bridge, &MonacoBridge::editorReady);
    bridge.onEditorReady();
    QCOMPARE(spy.count(), 1);
}

void TestMonacoBridge::onTextChangedForwardsText() {
    MonacoBridge bridge;
    QSignalSpy spy(&bridge, &MonacoBridge::textChanged);
    const QString sample = QStringLiteral("hello\nworld");
    bridge.onTextChanged(sample);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), sample);
}

void TestMonacoBridge::onCursorPositionChangedForwardsLineColumn() {
    MonacoBridge bridge;
    QSignalSpy spy(&bridge, &MonacoBridge::cursorPositionChanged);
    bridge.onCursorPositionChanged(7, 42);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toInt(), 7);
    QCOMPARE(spy.first().at(1).toInt(), 42);
}

QTEST_MAIN(TestMonacoBridge)
#include "test_monacobridge.moc"
