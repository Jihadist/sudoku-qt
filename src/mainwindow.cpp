﻿#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "sudokusolver.h"
#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QTime>

/**
 * @brief 加载颜色风格
 * @return jsonObject的字典
 */
QJsonObject loadStyle()
{
    QString path = ":/styles/";
    QDir directory(path);
    QStringList files = directory.entryList(QStringList() << "*.json", QDir::Files);

    QTime time;
    time = QTime::currentTime();
    QRandomGenerator generator(time.msec() + time.second() * 1000);

    auto n = generator.bounded(files.size());

    QFile file(path + files[n]);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument document = QJsonDocument::fromJson(data);
    QJsonObject jsonObject = document.object();
    return jsonObject;
}

QPushButton* createButton(QWidget* parent, QSize size, QString text)
{
    int nIndex = QFontDatabase::addApplicationFont(":/fonts/ARLRDBD.TTF");
    QStringList strList(QFontDatabase::applicationFontFamilies(nIndex));

    QFont buttonFont = QFont(strList.at(0), 12);
    buttonFont.setBold(true);

    QGraphicsDropShadowEffect* shadow_effect = new QGraphicsDropShadowEffect;
    shadow_effect->setOffset(1, 1);
    shadow_effect->setColor(Qt::gray);
    shadow_effect->setBlurRadius(2);

    QPushButton* button = new QPushButton(text);
    button->setObjectName("createdButton");
    button->setParent(parent);
    button->setFont(buttonFont);
    button->setFixedSize(size);
    button->setGraphicsEffect(shadow_effect);
    return button;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_sr(-1)
    , m_sc(-1)
    , m_switching(false)
    , m_forcing(false)
{
    /*********************************************/

    auto colorStyle = loadStyle();
    int gridSize = 81; // 格子的大小
    int margin = 15; // 四周的边缘宽度
    int spacing = 5; // 九宫格之间的间隔
    int halfSize = gridSize / 2; // 半个格子的大小，也是按钮的高度

    /*********************************************/

    m_grids.resize(9);
    m_counters.resize(10);
    m_numPositions.resize(10);
    m_controlRanges.resize(9);

    for (int r = 0; r < 9; r++) {
        m_grids[r].resize(9);
        m_controlRanges[r].resize(9);
        for (int c = 0; c < 9; c++) {
            // initialize control range, doesn't include self
            for (int i = 0; i < 9; i++) {
                m_controlRanges[r][c].insert(qMakePair(r, i));
                m_controlRanges[r][c].insert(qMakePair(i, c));
            }
            for (int i = r / 3 * 3; i < r / 3 * 3 + 3; i++) {
                for (int j = c / 3 * 3; j < c / 3 * 3 + 3; j++) {
                    m_controlRanges[r][c].insert(qMakePair(i, j));
                }
            }
            m_controlRanges[r][c].erase(m_controlRanges[r][c].find(qMakePair(r, c)));

            GridWidget* grid = new GridWidget(r, c, gridSize, this);
            grid->move(margin + c * gridSize + c / 3 * spacing, margin + r * gridSize + r / 3 * spacing);
            grid->setColorStyle(colorStyle["GridWidget"].toObject());
            m_grids[r][c] = grid;

            // 问题1：取消后立刻移到被取消的单元格，marker会出现，原因：off比hover里的on晚触发
            // 问题2：面板显示时，左击附近格子再立刻移入之前的格子，会出现marker | 通过添加m_switching解决
            // 问题3：点选数字后迅速移开，当前格子还会有marker | 通过添加m_forcing解决

            connect(grid, &GridWidget::hovered, [=]() {
                if (!grid->isEnabled()) {
                    return;
                }

                if ((!m_switching && !m_panel->isVisible()) || m_forcing) {
                    smartAssistOn(r, c);
                    grid->enter();
                }
            });

            connect(grid, &GridWidget::leaved, [=]() {
                if (!grid->isEnabled()) {
                    return;
                }

                if ((!m_switching && !m_panel->isVisible()) || m_forcing) {
                    m_sr = m_sc = -1;
                    smartAssistOff(r, c);
                    grid->leave();
                }
            });

            connect(grid, &GridWidget::rightClicked, [=]() {
                // 菜单未打开就清空
                if (!m_panel->isVisible()) {
                    clearGrid(r, c);
                    return;
                }

                // 如果菜单关闭失败（正在关闭/打开）直接退出
                if (!m_panel->canHide()) {
                    return;
                }

                // 从有唯一值到有多选值也看做是一步操作
                if (m_panel->m_selected && m_grids[m_sr][m_sc]->value()) {
                    receiveResult(0);
                }

                m_grids[m_sr][m_sc]->setMultiValue(m_panel->m_selected);
                m_panel->hide();
                smartAssistOff(m_sr, m_sc);
            });

            connect(grid, &GridWidget::clicked, [=]() {
                m_switching = true;
                if (!m_panel->isVisible() || m_panel->hide()) {
                    m_switching = false;
                    // m_sr和m_sc都小于0表示第一次打开窗体，所以没有上次打开的位置
                    // 这里的逻辑还要再看看游戏里的逻辑是怎么样的

                    if (m_sr >= 0 && m_sc >= 0) {
                        m_grids[m_sr][m_sc]->m_multiValue = m_panel->m_selected;
                        smartAssistOff(m_sr, m_sc);
                    }
                    m_panel->setSelected(grid->value() ? 0 : grid->m_multiValue);
                    m_panel->show(grid->geometry().x(), grid->geometry().y());
                    grid->leave();
                    smartAssistOn(r, c);
                    m_sr = r;
                    m_sc = c;
                }
                m_switching = false;
            });
        }
    }

    /***************************************/
    // 按钮
    // 所有按钮的样式，圆角等属性每个按钮自己设定
    this->setStyleSheet("QPushButton#createdButton{background-color:#FFFFFF;color:#5F5F5F;}"
                        "QPushButton#createdButton:hover{background-color:rgb(236, 236, 236);}"
                        "QPushButton#createdButton:pressed{background-color:rgb(222, 222, 222);}"
                        "QPushButton#createdButton:!enabled{background-color:rgb(200, 200, 200);}");

    // 加载按钮
    QPushButton* loadButton = createButton(this, QSize(gridSize * 3, gridSize), "Load");
    loadButton->setStyleSheet(QString("border-radius:%1px;").arg(halfSize));
    loadButton->move(margin + (gridSize * 3 + spacing) * 0, margin + gridSize * 9 + halfSize);
    connect(loadButton, SIGNAL(clicked()), this, SLOT(loadRandomPuzzle()));

    // 求解按钮
    QPushButton* solveButton = createButton(this, QSize(gridSize * 3, gridSize), "Solve");
    solveButton->setStyleSheet(QString("border-radius:%1px;").arg(halfSize));
    solveButton->move(margin + (gridSize * 3 + spacing) * 1, margin + gridSize * 9 + halfSize);
    connect(solveButton, SIGNAL(clicked()), this, SLOT(solve()));

    // 清空按钮
    QPushButton* clearButton = createButton(this, QSize(gridSize * 3, gridSize), "Clear");
    clearButton->setStyleSheet(QString("border-radius:%1px;").arg(halfSize));
    clearButton->move(margin + (gridSize * 3 + spacing) * 2, margin + gridSize * 9 + halfSize);
    connect(clearButton, SIGNAL(clicked()), this, SLOT(clearAll()));

    // 回退按钮
    m_undoButton = createButton(this, QSize(halfSize, gridSize), "<");
    m_undoButton->move(margin + gridSize * 9 + halfSize, margin + gridSize * 9 + halfSize);
    m_undoButton->setStyleSheet(QString("border-top-left-radius:%1px;border-bottom-left-radius:%1px;").arg(halfSize / 2));
    connect(m_undoButton, SIGNAL(clicked()), this, SLOT(undo()));

    // 重做按钮
    m_redoButton = createButton(this, QSize(halfSize, gridSize), ">");
    m_redoButton->move(margin + gridSize * 10, margin + gridSize * 9 + halfSize);
    m_redoButton->setStyleSheet(QString("border-top-right-radius:%1px;border-bottom-right-radius:%1px;").arg(halfSize / 2));
    connect(m_redoButton, SIGNAL(clicked()), this, SLOT(redo()));

    /***************************************/

    m_panel = new SelectPanel(gridSize, this);
    m_panel->setColorStyle(colorStyle["SelectPanel"].toObject());
    connect(m_panel, &SelectPanel::finish, [&](int selected) {
        m_grids[m_sr][m_sc]->setMultiValue(0);
        m_panel->m_selected = 0;

        smartAssistOff(m_sr, m_sc);
        receiveResult(selected);
        m_forcing = true;
        m_panel->hide();
        m_forcing = false;
    });

    int space = std::min(spacing / 10, 2);
    auto counterStyle = colorStyle["Counter"].toObject();
    for (int num = 1; num <= 9; ++num) {
        Counter* counter = new Counter(num, gridSize, this);
        counter->move(margin + gridSize * 9 + halfSize, margin - gridSize + num * (space + gridSize));
        counter->setColorStyle(counterStyle);
        connect(counter, &Counter::hovered, [=]() { highlight(num, true); });
        connect(counter, &Counter::leaved, [=]() { highlight(num, false); });
        m_counters[num] = counter;
    }

    loadRandomPuzzle();

    /*********************************************/

    // 窗口设置

    int minSize = gridSize * 10 + halfSize + 2 * margin;
    this->setMinimumSize(minSize, minSize);

    QLinearGradient linearGrad(QPointF(0, minSize), QPointF(minSize, 0));
    linearGrad.setColorAt(0, colorStyle["top_right_color"].toString());
    linearGrad.setColorAt(1, colorStyle["bottom_left_color"].toString());
    QBrush brush(linearGrad);

    QPalette p = this->palette();
    p.setBrush(QPalette::Window, brush);
    this->setPalette(p);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_panel;
}

void MainWindow::receiveResult(int selected)
{
    int previous = m_grids[m_sr][m_sc]->value();
    // 选择和之前相同表示清空当前格子
    if (previous == selected) {
        selected = 0;
    }

    changeNumber(m_sr, m_sc, previous, selected);

    m_undoOps.append(Op(m_sr, m_sc, previous, selected));
    m_undoButton->setEnabled(true);

    m_redoOps.clear();
    m_redoButton->setEnabled(false);
}

void MainWindow::highlight(int num, int active)
{
    for (auto pair : m_numPositions[num]) {
        if (active) {
            m_grids[pair.first][pair.second]->showBackground();
        }
        // 取消高亮时如果在选择状态，被覆盖的块不会被取消高亮
        else if (!m_panel->isVisible() || m_controlRanges[m_sr][m_sc].find(pair) == m_controlRanges[m_sr][m_sc].end()) {
            m_grids[pair.first][pair.second]->hideBackground();
        }
    }
}

void MainWindow::changeNumber(int r, int c, int previous, int selected)
{
    m_grids[r][c]->setValue(selected);

    int conflicts = 0;

    // 冲突检测，将和旧值相同的格子冲突数都减去1
    if (previous > 0) {
        m_counters[previous]->modify(1);
        m_numPositions[previous].erase(m_numPositions[previous].find(qMakePair(r, c)));
        for (auto pair : m_controlRanges[r][c]) {
            if (m_grids[pair.first][pair.second]->value() == previous) {
                m_grids[pair.first][pair.second]->changeConflict(-1);
                --conflicts;
            }
        }
    }

    // 如果新的值不为0，将和新值相同的格子冲突数加1，再计算本身的冲突数
    if (selected > 0) {
        m_counters[selected]->modify(-1);
        m_numPositions[selected].insert(qMakePair(r, c));
        for (auto pair : m_controlRanges[r][c]) {
            if (m_grids[pair.first][pair.second]->value() == selected) {
                m_grids[pair.first][pair.second]->changeConflict(1);
                ++conflicts;
            }
        }
    }

    m_grids[r][c]->changeConflict(conflicts);
}

void MainWindow::clearGrid(int r, int c)
{
    int previous = m_grids[r][c]->value();
    // 之前也为空就什么也不做
    if (previous == 0) {
        return;
    }

    changeNumber(r, c, previous, 0);

    m_undoOps.append(Op(r, c, previous, 0));
    m_undoButton->setEnabled(true);

    m_redoOps.clear();
    m_redoButton->setEnabled(false);
}

void MainWindow::clearAll()
{
    if (m_panel->isVisible()) {
        return;
    }

    QVector<int> counts(10, 0);
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            int value = m_grids[r][c]->value();
            m_grids[r][c]->setMultiValue(0);
            if (m_grids[r][c]->isEnabled() && value) {
                ++counts[value];
                m_grids[r][c]->setValue(0);
                m_numPositions[value].erase(m_numPositions[value].find(qMakePair(r, c)));
            }
            m_grids[r][c]->clearConflict();
        }
    }

    for (int num = 1; num <= 9; ++num) {
        m_counters[num]->modify(counts[num]);
    }

    m_undoOps.clear();
    m_undoButton->setEnabled(false);

    m_redoOps.clear();
    m_redoButton->setEnabled(false);
}

void MainWindow::smartAssistOff(int r, int c)
{
    for (auto& pair : m_controlRanges[r][c]) {
        m_grids[pair.first][pair.second]->hideBackground();
    }
    m_grids[r][c]->hideBackground();
}

void MainWindow::smartAssistOn(int r, int c)
{
    for (auto& pair : m_controlRanges[r][c]) {
        m_grids[pair.first][pair.second]->showBackground();
    }
    m_grids[r][c]->showBackground();
}

// 随机生成谜题
void MainWindow::loadRandomPuzzle()
{
    if (m_panel->isVisible()) {
        return;
    }

    QString path = ":/puzzles/";
    QDir directory(path);
    QStringList files = directory.entryList(QStringList() << "*.txt", QDir::Files);

    QTime time;
    time = QTime::currentTime();
    QRandomGenerator generator(time.msec() + time.second() * 1000);

    auto n = generator.bounded(files.size());

    QFile file(path + files[n]);
    file.open(QFile::ReadOnly);
    QString array = file.readAll();
    QStringList rows = array.split('\n');
    file.close();

    QVector<int> counts(10, 0);
    for (auto& set : m_numPositions) {
        set.clear();
    }

    int val;
    for (int r = 0; r < 9; r++) {
        QStringList cols = rows.at(r).split(' ');
        for (int c = 0; c < 9; c++) {
            val = cols.at(c).toInt();
            m_grids[r][c]->setEnabled(val == 0); // 值为0表示待填充，即可操作
            m_grids[r][c]->setValue(val);
            m_numPositions[val].insert(qMakePair(r, c));
            ++counts[val];
        }
    }

    for (int num = 1; num <= 9; ++num) {
        m_counters[num]->setCount(9 - counts[num]);
    }

    m_undoOps.clear();
    m_undoButton->setEnabled(false);
    m_redoButton->setEnabled(false);
}

void MainWindow::solve()
{
    if (m_panel->isVisible()) {
        return;
    }

    QVector<QVector<int>> puzzle(9, QVector<int>(9, 0));
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            puzzle[r][c] = m_grids[r][c]->value();
        }
    }

    SudokuSolver solver(puzzle);
    solver.Solve();
    if (solver.m_num == 0) {
        return;
    }

    for (int num = 1; num <= 9; ++num) {
        m_numPositions[num].clear();
        m_counters[num]->setCount(0);
    }

    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            int value = solver.m_res[r][c];
            m_grids[r][c]->setMultiValue(0);
            m_grids[r][c]->setValue(value);
            m_numPositions[value].insert(qMakePair(r, c));
        }
    }
}

void MainWindow::redo()
{
    if (m_panel->isVisible()) {
        return;
    }

    Op op = m_redoOps.pop();
    m_undoOps.push(op);
    changeNumber(op.row, op.col, op.before, op.after);

    m_undoButton->setEnabled(true);
    m_redoButton->setEnabled(m_redoOps.size() > 0);
}

void MainWindow::undo()
{
    if (m_panel->isVisible()) {
        return;
    }

    Op op = m_undoOps.pop();
    m_redoOps.push(op);
    changeNumber(op.row, op.col, op.after, op.before);

    m_redoButton->setEnabled(true);
    m_undoButton->setEnabled(m_undoOps.size() > 0);
}
