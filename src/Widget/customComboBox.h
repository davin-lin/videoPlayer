#ifndef CUSTOMCOMBOBOX_H
#define CUSTOMCOMBOBOX_H
#include <QCombobox>
#include <QAbstractItemView>
#include <QDebug>
#include <QApplication>
#include <MainWindow.h>
// 在 MainWindow.cpp 内部定义 CustomComboBox
class CustomComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit CustomComboBox(QWidget* parent = nullptr) : QComboBox(parent) {}

protected:
    void showPopup() override {

        //关闭弹出动画，调用基类showPopup
        bool blOldAnimation = qApp->isEffectEnabled(Qt::UI_AnimateCombo);
        qApp->setEffectEnabled(Qt::UI_AnimateCombo, false);
        QComboBox::showPopup();
        qApp->setEffectEnabled(Qt::UI_AnimateCombo, blOldAnimation);

        //获取QCombox的视图显示控件
        QWidget* popup = this->findChild<QWidget*>();
        //移动到上方显示
        QPoint globalPos = this->mapToGlobal(QPoint(0, 0));
        popup->move(globalPos.x() - popup->width() + this->width(), globalPos.y() - popup->height() - 5);
        

    }
}; 
#endif // CUSTOMCOMBOBOX_H
