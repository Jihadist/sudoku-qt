﻿#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <vector>
#include <QDir>
#include <QDebug>
#include <QGraphicsDropShadowEffect>

#include "sudokusolver.h"

#define startX 10
#define startY 10
#define gridSize 75
#define spacing 10

/**
 * @brief 操作数
 */
struct Op
{
    int row;    // 操作的行数
    int col;    // 操作的列数
    int before; // 更改前的值
    int after;  // 更改后的值
    Op(int _row = 0, int _col = 0, int _before = 0, int _after = 0)
        : row(_row), col(_col), before(_before), after(_after) {}
};

QPushButton* createButton(QString text)
{
    QFont buttonFont("华文新魏", 15);
    buttonFont.setPointSize(12);
    buttonFont.setBold(true);

    QGraphicsDropShadowEffect *shadow_effect = new QGraphicsDropShadowEffect;
    shadow_effect->setOffset(1, 1);
    shadow_effect->setColor(Qt::gray);
    shadow_effect->setBlurRadius(2);

    QPushButton *button = new QPushButton(text);
    button->setFont(buttonFont);
    button->setGraphicsEffect(shadow_effect);
    button->setStyleSheet(QString("QWidget{border-radius:%1px;background-color:#FFFFFF;color:#5F5F5F;}"
                          "QPushButton:hover{background-color:rgb(236,236,236);}"
                          "QPushButton:pressed{background-color:rgb(222,222,222);}").arg(gridSize / 2));
    return button;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    // 背景色
    QColor color = QColor("#DDE2E5");
    QPalette p = this->palette();
    p.setColor(QPalette::Window,color);
    this->setPalette(p);

    m_grids.resize(9);
    m_controlRanges.resize(9);
    for(int r = 0; r < 9; r++)
    {
        m_grids[r].resize(9);
        m_controlRanges[r].resize(9);
        for (int c = 0; c < 9; c++)
        {
            // initialize control range, doesn't include self
            for (int i = 0; i < 9; i++)
            {
                m_controlRanges[r][c].insert(qMakePair(r, i));
                m_controlRanges[r][c].insert(qMakePair(i, c));
            }
            for (int i = r / 3 * 3; i < r / 3 * 3 + 3; i++)
            {
                for (int j = c / 3 * 3; j < c / 3 * 3 + 3; j++)
                {
                   m_controlRanges[r][c].insert(qMakePair(i, j));
                }
            }
            m_controlRanges[r][c].erase(m_controlRanges[r][c].find(qMakePair(r, c)));

            GridWidget *grid = new GridWidget(r, c);
            grid->setParent(this);
            grid->move(startX + c * gridSize + c / 3 * spacing, startY + r * gridSize + r / 3 * spacing);
            m_grids[r][c] = grid;

            connect(grid, &GridWidget::hovered, [=](){smartAssistOn(r, c); });
            connect(grid, &GridWidget::leave, [=](){smartAssistOff(r, c); });
            connect(grid, &GridWidget::clicked, [=](){changeGrid(r, c); });
            connect(grid, &GridWidget::rightClicked, [=](){clearGrid(r, c);});
        }
    }

    QPushButton *loadButton = createButton("load");
    loadButton->setParent(this);
    loadButton->move(startX + (gridSize * 3 + spacing) * 0, gridSize * 9.5 + startY);
    loadButton->setFixedSize(gridSize * 3, gridSize);
    connect(loadButton, &QPushButton::clicked, [&](){loadRandomPuzzle();});

    QPushButton *solveButton = createButton("solve");
    solveButton->setParent(this);
    solveButton->move(startX + (gridSize * 3 + spacing) * 1, gridSize * 9.5 + startY);
    solveButton->setFixedSize(gridSize * 3, gridSize);
    connect(solveButton, &QPushButton::clicked, [&](){solve();});

    QPushButton *clearButton = createButton("clear");
    clearButton->setParent(this);
    clearButton->move(startX + (gridSize * 3 + spacing) * 2, gridSize * 9.5 + startY);
    clearButton->setFixedSize(gridSize * 3, gridSize);
    connect(clearButton, &QPushButton::clicked,  [&](){clearAll();});

    m_undoButton = new QPushButton("<");
    m_undoButton->setParent(this);
    m_undoButton->setEnabled(false);
    m_undoButton->move(gridSize * 9.5 + startX, gridSize * 9.5 + startY);
    m_undoButton->setFixedSize(gridSize * 0.4, gridSize);
    connect(m_undoButton, &QPushButton::clicked,  [&](){undo();});

    m_redoButton = new QPushButton(">");
    m_redoButton->setParent(this);
    m_redoButton->setEnabled(false);
    m_redoButton->move(gridSize * 10.1 + startX, gridSize * 9.5 + startY);
    m_redoButton->setFixedSize(gridSize * 0.4, gridSize);
    connect(m_redoButton, &QPushButton::clicked,  [&](){redo();});

    m_panel = new SelectPanel(75);
    m_panel->setParent(this);
    m_panel->close();

    int space = std::min(spacing / 10, 2);
    for (int i = 0; i < 9; i++)
    {
       Counter *counter = new Counter(i + 1, gridSize, this);
       counter->move(startX + gridSize * 9.5, startY + space + i * (space + gridSize));
       m_counters.push_back(counter);
       connect(counter, &Counter::hovered, [=]() {highlight(i + 1, true);});
       connect(counter, &Counter::leave, [=]() {highlight(i + 1, false);});
    }

    int minHeight = loadButton->geometry().y() + loadButton->geometry().height() + startY;
    int minWidth = m_counters[0]->geometry().x() + m_counters[0]->geometry().width() + startX;
    this->setMinimumSize(minWidth, minHeight);
    Qt::WindowFlags flags = 0;
    flags |= Qt::WindowMinimizeButtonHint;
    this->setWindowFlags(flags); // 设置禁止最大化

    loadRandomPuzzle();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_panel;
}



// 统计每个数的位置，就不用遍历了 | 废除
void MainWindow::highlight(int num, int active)
{
    for (int r = 0; r < 9; r++)
    {
        for (int c = 0; c < 9; c++)
        {
            if (m_grids[r][c]->value() == num)
            {
                if (active)
                {
                    m_grids[r][c]->hideButton();
                }
                else
                {
                    m_grids[r][c]->revealButton();
                }

            }
        }
    }
}

// 为了实现undo/redo剥离出修改数值的代码
void MainWindow::changeNumber(int r, int c, int previous, int selected)
{
    m_grids[r][c]->setValue(selected);
    m_grids[r][c]->clearConflict();

    // Conflict detection
    if (previous > 0)
    {
        m_counters[previous - 1]->plus();
        for (auto pair : m_controlRanges[r][c])
        {
            if (m_grids[pair.first][pair.second]->value() == previous)
            {
                m_grids[pair.first][pair.second]->removeConflict(1);
            }
        }
    }

    if (selected > 0)
    {
        int num = 0;
        m_counters[selected - 1]->minus();
        for (auto pair : m_controlRanges[r][c])
        {
            if (m_grids[pair.first][pair.second]->value() == selected)
            {
                m_grids[pair.first][pair.second]->addConflict(1);
                ++num;
            }
        }
        m_grids[r][c]->addConflict(num);
    }
}

// 冲突检测 | 完成
void MainWindow::clearGrid(int r, int c)
{
    int previous = m_grids[r][c]->value();
    if (previous == 0)
    {
        return;
    }

    /*
    for (auto pair : m_controlRanges[r][c])
    {
        if (m_grids[pair.first][pair.second]->value() == previous)
        {
            m_grids[pair.first][pair.second]->removeConflict(1);
        }
    }

    m_counters[m_grids[r][c]->value() - 1]->plus();  // 计数加1
    m_grids[r][c]->setValue(0);
    m_grids[r][c]->clearConflict();  // 清空冲突数
    */

    changeNumber(r, c, previous, 0);

    m_ops.append(Op(r, c, previous, 0));
    m_ops2.clear();
    m_undoButton->setEnabled(true);
    m_redoButton->setEnabled(false);
}

// 冲突检测 | 显示qdialog时有bug
void MainWindow::changeGrid(int r, int c)
{
    if (m_panel->isVisible())
    {
        m_panel->close();
        return;
    }

    m_panel->move(gridSize * c + startX + c / 3 * spacing, r * gridSize + startY + r / 3 * spacing);
    m_panel->exec();

    // selected为0表示没做操作就退出了
    int selected = m_panel->number();
    if (selected == 0)
    {
        return;
    }

    int previous = m_grids[r][c]->value();
    changeNumber(r, c, previous, selected);

    m_ops.append(Op(r, c, previous, selected));
    m_ops2.clear();
    m_undoButton->setEnabled(true);
    m_redoButton->setEnabled(false);
}

// 不要多次调用plus,冲突检测 | 完成
void MainWindow::clearAll()
{
    QVector<int> counts(10, 0);
    for (int r = 0; r < 9; r++)
    {
        for (int c = 0; c < 9; c++)
        {
            if (m_grids[r][c]->isEnabled() && m_grids[r][c]->value())
            {
                ++counts[m_grids[r][c]->value()];
                m_grids[r][c]->setValue(0);
            }
            m_grids[r][c]->clearConflict();
        }
    }

    for (int i = 1; i < 10; i++)
    {
        m_counters[i - 1]->plus(counts[i]);
    }

    m_ops.clear();
    m_ops2.clear();
    m_undoButton->setEnabled(false);
    m_redoButton->setEnabled(false);
}



// 预计算每一格要显示的范围 | 完成
void MainWindow::smartAssistOff(int r, int c)
{
    for (auto pair : m_controlRanges[r][c])
    {
        m_grids[pair.first][pair.second]->revealButton();
    }
    m_grids[r][c]->revealButton();
}

// 同上 | 完成
void MainWindow::smartAssistOn(int r, int c)
{
    for (auto pair : m_controlRanges[r][c])
    {
        m_grids[pair.first][pair.second]->hideButton();
    }
    m_grids[r][c]->hideButton();
}



// 随机生成谜题
void MainWindow::loadRandomPuzzle()
{
    QFile file("D:/test.txt");
    if(!file.open(QFile::ReadOnly))
    {
        return;
    }

    QVector<int> counts(10, 0);

    int num;
    QString array = file.readAll();
    QStringList rows = array.split('\n');
    for (int r = 0; r < 9; r++)
    {
        QStringList cols = rows.at(r).split(' ');
        for (int c = 0; c < 9; c++)
        {
            num = cols.at(c).toInt();
            m_grids[r][c]->setEnabled(num == 0);  // 值为0表示待填充，即可操作
            m_grids[r][c]->setValue(num);
            ++counts[num];
        }
    }

    for (int i = 1; i <= 9; i++)
    {
        m_counters[i - 1]->setCount(9 - counts[i]);
    }

    m_ops.clear();
}

// 没有结果时的处理
void MainWindow::solve()
{
    std::vector<std::vector<int>> puzzle(9, std::vector<int>(9, 0));
    for (int r = 0; r < 9; r++)
    {
        for (int c = 0; c < 9; c++)
        {
            puzzle[r][c] = m_grids[r][c]->value();
        }
    }

    SudokuSolver solver(puzzle);
    solver.Solve();

    for (int r = 0; r < 9; r++)
    {
        m_counters[r]->setCount(0);
        for (int c = 0; c < 9; c++)
        {
            m_grids[r][c]->setValue(solver.m_res[r][c]);
        }
    }
}



// 撤销回退
void MainWindow::redo()
{
    Op op = m_ops2.pop();
    changeNumber(op.row, op.col, op.before, op.after);
    m_ops.push(op);

    m_undoButton->setEnabled(true);
    if (m_ops2.size() == 0)
    {
        m_redoButton->setEnabled(false);
        // set redo button unabled
    }
}

// 回退
void MainWindow::undo()
{
    Op op = m_ops.pop();
    qDebug() << m_ops.size();
    changeNumber(op.row, op.col, op.after, op.before);
    m_ops2.push(op);

    m_redoButton->setEnabled(true);
    if (m_ops.size() == 0)
    {
        m_undoButton->setEnabled(false);
        // set undo button unabled
    }
}
