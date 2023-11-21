// SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMainWindow>

#include "MandelbrotWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void closeEvent(QCloseEvent *event) override;

private:
    MandelbrotWidget *m_singleThread;
    MandelbrotWidget *m_multiThread;
    MandelbrotWidget *m_compute;
};
