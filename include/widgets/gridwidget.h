﻿/**
 * @file gridwidget.h
 * @brief Contains main component of sudoku
 * @author Joe chen <joechenrh@gmail.com>
 */

#ifndef GRIDWIDGET_H
#define GRIDWIDGET_H

#include "basewidget.h"
#include "gridmarker.h"

#include <QJsonObject>

/**
 * @brief The GridWidget class
 * @details 该控件组成了数组的主要窗体，主要有以下几个功能：
 * 1. 选择和清除当前格的数值
 * 2. 记录和当前格冲突的单元格数量，在存在冲突时，会绘制一个圆环加以提示
 * 3. 鼠标进入时会出现一个标记，同时会高亮当前单元格
 */
class GridWidget : public QWidget
{
    Q_OBJECT

    struct GridWidgetStyle
    {
        QString border_radius[2];
        QString border_color[2];
        QString font_color[3];

        QString background_color_hovered;
        QString background_color_unhovered;
        QString background_shadow_color;
        QString spacing_color;
    };

public:
    explicit GridWidget(int row, int col, int size = 75, QWidget *parent = nullptr);

    void setColorStyle(QJsonObject json);

    /**
     * @brief setEnabled 设置当前控件是否可操作
     * @param flag 是否可操作
     * @details 谜面所在的单元格是不能编辑的，也不会触发任何鼠标事件
     */
    void setEnabled(bool enabled);

    /**
     * @brief 返回当前控件是否可操作
     * @return  是否可操作
     * @see setEnabled
     */
    bool isEnabled() const;

    /**
     * @brief 设置该格的值
     * @param value 该格选中的值，如没有值（例如清空操作），输入0
     */
    void setValue(int value);

    void setMultiValue(int value);

    /**
     * @brief 返回选中的值
     * @return 该格选中的值，如没选中，返回0
     */
    int value() const;

    int multiValue() const;

    void enter();

    void leave();

    /**
     * @brief 显示背景
     */
    void showBackground();

    /**
     * @brief 隐藏背景
     */
    void hideBackground();

    /**
     * @brief 调整冲突数
     * @param num 改变的冲突数
     */
    void changeConflict(int num);

    /**
     * @brief 清空冲突数
     */
    void clearConflict();

    int m_multiValue;

/******************************/
/* 下面8个都是为了传递信号        */

protected:
    /**
     * @brief 鼠标移入触发的事件
     * @param e 鼠标事件
     */
    void enterEvent(QEvent *e);

    /**
     * @brief 鼠标移出触发的事件
     * @param 鼠标事件
     */
    void leaveEvent(QEvent *e);

private slots:
    /**
     * @brief 顶层按钮被左击时的slot
     */
    void buttonClicked();

    /**
     * @brief 顶层按钮被右击时的slot
     */
    void buttonRightClicked();

signals:
    /**
     * @brief 鼠标移入触发的信号
     */
    void hovered();

    /**
     * @brief 鼠标移出触发的信号
     */
    void leaved();

    /**
     * @brief 顶层按钮被左击触发的信号
     */
    void clicked();

    /**
     * @brief 顶层按钮被右击触发的信号
     */
    void rightClicked();

/******************************/

private:
    GridWidgetStyle m_style;

    void setButtonStyle(int entered);

    /**
     * @brief 最顶层的按钮，用于触发单击事件
     */
    BaseWidget *m_button;

    QVector<QLabel*> m_multiGrids;

    /**
     * @brief QLabel，用于显示数值
     */
    QLabel *m_singleGrid;

    /**
     * @brief 鼠标覆盖时的标记
     */
    GridMarker *m_marker;

    /**
     * @brief m_background
     */
    QLabel *m_background;

    /**
     * @brief 底层的前景
     */
    BaseWidget *m_foreground;

    /**
     * @brief m_backgroundStyle
     */
    QString m_backgroundStyle;

    /**
     * @brief 当前单元格的值
     */
    int m_value;

    /**
     * @brief 冲突的单元格数量
     * @details 当前行，列，九宫格内，值和自身值一样的单元格的数量
     */
    int m_numConflict;

    QString m_borderRadius;
    QString m_borderColor;
    QString m_fontColor;
    QString m_buttonStyle;
};

#endif // GRIDWIDGET_H
