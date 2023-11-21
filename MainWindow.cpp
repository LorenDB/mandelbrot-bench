// SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QProgressBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    auto cw = new QWidget;
    auto layout = new QVBoxLayout{cw};

    auto progress = new QProgressBar;
    progress->setMinimum(0);
    progress->setMaximum(3);
    progress->setValue(0);
    layout->addWidget(progress);

    layout->addStretch(0);

    auto fullSet = new QRadioButton{"Show full set"};
    auto spike = new QRadioButton{"Zoom in on left spike"};
    fullSet->setChecked(true);
    layout->addWidget(fullSet);
    layout->addWidget(spike);

    layout->addStretch(0);

    auto renderBtn = new QPushButton{"Re-render"};
    layout->addWidget(renderBtn);
    renderBtn->setDisabled(true);

    setCentralWidget(cw);

    m_singleThread = new MandelbrotWidget{MandelbrotWidget::RenderType::CpuSingleThread};
    m_multiThread = new MandelbrotWidget{MandelbrotWidget::RenderType::CpuMultiThread};
    m_compute = new MandelbrotWidget{MandelbrotWidget::RenderType::Gpu};
    m_singleThread->show();
    m_multiThread->show();
    m_compute->show();

    connect(fullSet, &QRadioButton::clicked, this, [this](bool checked) {
        if (!checked)
            return;
        m_singleThread->setView(MandelbrotWidget::FractalView::EntireSet);
        m_multiThread->setView(MandelbrotWidget::FractalView::EntireSet);
        m_compute->setView(MandelbrotWidget::FractalView::EntireSet);
    });
    connect(spike, &QRadioButton::clicked, this, [this](bool checked) {
        if (!checked)
            return;
        m_singleThread->setView(MandelbrotWidget::FractalView::LeftSpike);
        m_multiThread->setView(MandelbrotWidget::FractalView::LeftSpike);
        m_compute->setView(MandelbrotWidget::FractalView::LeftSpike);
    });

    connect(renderBtn, &QPushButton::clicked, this, [this, renderBtn] {
        renderBtn->setDisabled(true);
        m_singleThread->rerender();
        m_multiThread->rerender();
        m_compute->rerender();
    });

    auto updateWidgets = [this, renderBtn, progress] {
        if (m_singleThread->rendering() || m_multiThread->rendering() || m_compute->rendering())
            renderBtn->setDisabled(true);
        else
            renderBtn->setEnabled(true);

        auto progressCount = 3;
        if (m_singleThread->rendering())
            --progressCount;
        if (m_multiThread->rendering())
            --progressCount;
        if (m_compute->rendering())
            --progressCount;
        progress->setValue(progressCount);
    };

    connect(m_singleThread, &MandelbrotWidget::doneRendering, this, updateWidgets);
    connect(m_multiThread, &MandelbrotWidget::doneRendering, this, updateWidgets);
    connect(m_compute, &MandelbrotWidget::doneRendering, this, updateWidgets);
}

MainWindow::~MainWindow()
{
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_singleThread->close();
    m_multiThread->close();
    m_compute->close();
}

