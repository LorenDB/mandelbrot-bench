// SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>
#include <QLabel>
#include <QFuture>

class MandelbrotWidget : public QWidget
{
    Q_OBJECT

public:
    enum class RenderType
    {
        CpuSingleThread,
        CpuMultiThread,
        Gpu,
    };

    enum class FractalView
    {
        EntireSet,
        LeftSpike,
    };

    explicit MandelbrotWidget(RenderType renderType, QWidget *parent = nullptr);

    void setView(FractalView view);
    void rerender();

    bool rendering() const { return !m_doneRendering; }

signals:
    void doneRendering();
    void rendering(QFuture<void> renderJob);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    RenderType m_renderType;
    int m_size;
    bool m_doneRendering = false;
    QPixmap m_pixmap;
    QLabel *m_debugLabel;
    FractalView m_view{FractalView::EntireSet};
};
