// SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MandelbrotWidget.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QtConcurrent/QtConcurrent>

#include <boost/compute.hpp>
#include <boost/compute/types.hpp>

namespace compute = boost::compute;
using doublepair = std::pair<double, double>;

#include <complex>
#include <iostream>

int calculateMandelbrot(std::complex<double> c)
{
    if (std::abs(c) > 2)
        return 1;
    else
    {
        auto zSquaredPlusC = c;
        for (int i = 0; i < 100; ++i)
        {
            zSquaredPlusC = std::pow(zSquaredPlusC, 2) + c;
            if (std::pow(zSquaredPlusC.real(), 2) + std::pow(zSquaredPlusC.imag(), 2) > 4)
                return i + 1;
        }
        return 0;
    }
}

MandelbrotWidget::MandelbrotWidget(RenderType renderType, QWidget *parent)
    : QWidget{parent},
      m_renderType{renderType},
      m_debugLabel{new QLabel{this}}
{
    const auto screenSize = qApp->primaryScreen()->availableSize();
    m_size = std::min(screenSize.width(), screenSize.height()) * 0.9;
    setFixedSize(m_size, m_size);
    m_pixmap = QPixmap{m_size, m_size};
    m_debugLabel->move(20, 20);
    setWindowFlags((Qt::CustomizeWindowHint | Qt::WindowTitleHint) & ~Qt::WindowCloseButtonHint);

    rerender();
}

void MandelbrotWidget::setView(FractalView view)
{
    m_view = view;
}

void MandelbrotWidget::rerender()
{
    m_doneRendering = false;
    m_debugLabel->setText({});

    // we're using a dedicated thread pool for this lambda because it doesn't actually consume a significant amount of CPU;
    // therefore, it can coexist with a render thread on the same core
    static auto threadPool = new QThreadPool{this};
    threadPool->setMaxThreadCount(3);
    auto renderJob = QtConcurrent::run(threadPool, [this] {
        QPainter painter(&m_pixmap);

        std::vector<QPoint> pixels;
        std::vector<std::complex<double>> points;
        for (int row = 0; row < m_size; ++row)
        {
            for (int col = 0; col < m_size; ++col)
            {
                switch (m_view)
                {
                case MandelbrotWidget::FractalView::EntireSet:
                    points.push_back({-2.5 + (4.0 * row / m_size), -2.0 + (4.0 * col / m_size)});
                    break;
                case MandelbrotWidget::FractalView::LeftSpike:
                    points.push_back({-1.7 + (0.25 * row / m_size), -0.125 + (0.25 * col / m_size)});
                    break;
                }
                pixels.push_back({row, col});
            }
        }
        std::vector<int> results(points.size());

        QElapsedTimer timer;
        timer.start();
        if (m_renderType == RenderType::CpuSingleThread)
            for (int i = 0; i < points.size(); ++i)
                results[i] = calculateMandelbrot(points[i]);
        else if (m_renderType == RenderType::CpuMultiThread)
            results =
                QtConcurrent::blockingMapped(points, [](const std::complex<double> &point) -> int {
                    return calculateMandelbrot(point);
                });
        else if (m_renderType == RenderType::Gpu)
        {
            try
            {
                BOOST_COMPUTE_FUNCTION(int, calculateMandelbrotCompute, (std::complex<double> c), {
                    if (sqrt(c.x * c.x + c.y * c.y) > 2)
                        return 1;
                    else
                    {
                        double2 zSquaredPlusC = c;
                        for (int i = 0; i < 100; ++i)
                        {
                            double2 newzc;
                            newzc.x = (zSquaredPlusC.x * zSquaredPlusC.x) - (zSquaredPlusC.y * zSquaredPlusC.y) + c.x;
                            newzc.y = (2 * zSquaredPlusC.x * zSquaredPlusC.y) + c.y;
                            zSquaredPlusC = newzc;
                            if (((zSquaredPlusC.x * zSquaredPlusC.x) + (zSquaredPlusC.y * zSquaredPlusC.y)) > 4)
                                return i + 1;
                        }
                        return 0;
                    }
                });

                compute::vector<std::complex<double>> points_compute(points.size());
                compute::copy(points.begin(), points.end(), points_compute.begin());

                compute::vector<int> results_compute(points_compute.size());
                compute::transform(points_compute.begin(), points_compute.end(), results_compute.begin(), calculateMandelbrotCompute);

                compute::copy(results_compute.begin(), results_compute.end(), results.begin());
            }
            catch (const boost::wrapexcept<boost::compute::program_build_failure> &f)
            {
                std::cout << f.build_log() << std::endl;
                std::cout << f.what() << std::endl;
                std::cout << f.error_code() << std::endl;
                std::cout << f.error_string() << std::endl;
            }
        }

        const auto time = timer.durationElapsed();
        QString timeText;
        switch (m_renderType)
        {
        case RenderType::CpuSingleThread:
            timeText = QStringLiteral("Single-threaded CPU");
            break;
        case RenderType::CpuMultiThread:
            timeText = QStringLiteral("Multi-threaded CPU (%1 threads)").arg(QThreadPool::globalInstance()->maxThreadCount());
            break;
        case RenderType::Gpu:
            timeText = QString::fromStdString(compute::system::default_device().name());
            break;
        }

        m_debugLabel->setText(QStringLiteral("%1\n%2x%2 px\n%3 ns\n%4 ms\n%5 s")
                                  .arg(timeText,
                                       QString::number(m_size),
                                       QString::number(time.count()),
                                       QString::number((double)time.count() / 1000000),
                                       QString::number((double)time.count() / 1000000000)));
        m_debugLabel->resize(m_debugLabel->sizeHint());

        for (int i = 0; i < results.size(); ++i)
        {
            if (results[i] == 0)
                painter.setPen(QPen{QColor{0, 0, 0}});
            else
            {
                switch (m_renderType)
                {
                case RenderType::CpuSingleThread:
                    painter.setPen(QPen{QColor{255 - std::min(static_cast<int>(255 / results[i] / 4), 255),
                                               255 - std::min(static_cast<int>(255 / results[i] / 0.8) + 50, 255),
                                               255 - std::min(static_cast<int>(255 / results[i] / 0.8) + 50, 255)}});
                    break;
                case RenderType::CpuMultiThread:
                    painter.setPen(QPen{QColor{255 - std::min(static_cast<int>(255 / results[i]/ 0.8) + 50, 255),
                                               255 - std::min(static_cast<int>(255 / results[i]/ 4) + 50, 255),
                                               255 - std::min(static_cast<int>(255 / results[i]/ 0.8) + 50, 255)}});
                    break;
                case RenderType::Gpu:
                    painter.setPen(QPen{QColor{255 - std::min(static_cast<int>(255 / results[i] / 0.8) + 50, 255),
                                               255 - std::min(static_cast<int>(255 / results[i] / 0.8) + 50, 255),
                                               255 - std::min(static_cast<int>(255 / results[i] / 4) + 50, 255)}});
                    break;
                }

            }

            painter.drawPoint(pixels[i]);
        }

        m_doneRendering = true;
        emit doneRendering();
        update();
    });
    emit rendering(renderJob);
    update();
}

void MandelbrotWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    if (m_doneRendering)
    {
        painter.drawPixmap(0, 0, m_pixmap);
        if (!m_debugLabel->text().isEmpty())
            painter.fillRect(10, 10, m_debugLabel->width() + 20, m_debugLabel->height() + 20, qApp->palette().base());
    }
    else
        painter.fillRect(rect(), qApp->palette().base());
}
