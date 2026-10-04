#pragma once

#include "editor.h"

#include <QWidget>

class QButtonGroup;
class QGridLayout;
class QToolButton;

namespace app {

QString toolName(lp::ToolId id);
QString toolStatusTip(lp::ToolId id);

// The two-column tool box with the options box underneath, as in the classic Paint.
class ToolBox : public QWidget {
    Q_OBJECT
public:
    explicit ToolBox(Editor* editor, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* e) override;

private:
    void rebuildOptions();
    void refreshIcons();
    QToolButton* addOption(const QIcon& icon, const QString& name, bool checked, int row, int col,
                           const std::function<void()>& onClick);

    Editor* editor_;
    QList<QToolButton*> toolButtons_;
    QButtonGroup* toolGroup_;
    QWidget* optionsBox_;
    QGridLayout* optionsLayout_;
    QButtonGroup* optionsGroup_ = nullptr;
};

} // namespace app
