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
    auto renderJob = QtConcurrent::run([this] {
        QPainter painter(&m_pixmap);

        using AllDataType = std::pair<std::pair<int, int>, std::complex<double>>;
        std::vector<AllDataType> points;
        for (int row = 0; row < m_size; ++row)
        {
            for (int col = 0; col < m_size; ++col)
            {
                switch (m_view)
                {
                case MandelbrotWidget::FractalView::EntireSet:
                    points.push_back({{row, col}, {-2.5 + (4.0 * row / m_size), -2.0 + (4.0 * col / m_size)}});
                    break;
                case MandelbrotWidget::FractalView::LeftSpike:
                    points.push_back({{row, col}, {-1.7 + (0.25 * row / m_size), -0.125 + (0.25 * col / m_size)}});
                    break;
                }
            }
        }

        QElapsedTimer timer;
        timer.start();
        if (m_renderType == RenderType::CpuSingleThread)
        {
            for (const auto &point : points)
            {
                const auto result = calculateMandelbrot(point.second);
                if (result == 0)
                    painter.setPen(QPen{QColor{0, 0, 0}});
                else
                    painter.setPen(QPen{QColor{255 - std::min(static_cast<int>(255 / result / 4), 255),
                                               255 - std::min(static_cast<int>(255 / result / 0.8) + 50, 255),
                                               255 - std::min(static_cast<int>(255 / result / 0.8) + 50, 255)}});
                painter.drawPoint(point.first.first, point.first.second);
            }
        }
        else if (m_renderType == RenderType::CpuMultiThread)
        {
            auto results =
                QtConcurrent::blockingMapped(points, [](const AllDataType &point) -> std::pair<std::pair<int, int>, int> {
                    return {point.first, calculateMandelbrot(point.second)};
                });
            for (const auto result : results)
            {
                if (result.second == 0)
                    painter.setPen(QPen{QColor{0, 0, 0}});
                else
                    painter.setPen(QPen{QColor{255 - std::min(static_cast<int>(255 / result.second / 0.8) + 50, 255),
                                               255 - std::min(static_cast<int>(255 / result.second / 4) + 50, 255),
                                               255 - std::min(static_cast<int>(255 / result.second / 0.8) + 50, 255)}});
                painter.drawPoint(result.first.first, result.first.second);
            }
        }
        else if (m_renderType == RenderType::Gpu)
        {
            try
            {
                BOOST_COMPUTE_FUNCTION(int, calculateMandelbrotCompute, (std::pair<double, double> c), {
                    if (sqrt(c.first * c.first + c.second * c.second) > 2)
                        return 1;
                    else
                    {
                        _pair_double_double_t zSquaredPlusC = c;
                        for (int i = 0; i < 100; ++i)
                        {
                            _pair_double_double_t newzc;
                            newzc.first = (zSquaredPlusC.first * zSquaredPlusC.first) - (zSquaredPlusC.second * zSquaredPlusC.second) + c.first;
                            newzc.second = (2 * zSquaredPlusC.first * zSquaredPlusC.second) + c.second;
                            zSquaredPlusC = newzc;
                            if (((zSquaredPlusC.first * zSquaredPlusC.first) + (zSquaredPlusC.second * zSquaredPlusC.second)) > 4)
                                return i + 1;
                        }
                        return 0;
                    }
                });

                // Since the points need transformed to a new storage type, we'll do that on CPU as it takes a long time to
                // manually copy values one by one to the GPU
                std::vector<std::pair<double, double>> tempPoints;
                for (const auto &p : points)
                    tempPoints.push_back({p.second.real(), p.second.imag()});

                compute::vector<std::pair<double, double>> points_compute(tempPoints.size());
                compute::copy(tempPoints.begin(), tempPoints.end(), points_compute.begin());

                compute::vector<double> results_compute(points_compute.size());
                compute::transform(points_compute.begin(), points_compute.end(), results_compute.begin(), calculateMandelbrotCompute);

                std::vector<double> results(points_compute.size());
                compute::copy(results_compute.begin(), results_compute.end(), results.begin());

                for (int i = 0; i < results.size() && i < points.size(); ++i)
                {
                    if (results[i] == 0)
                        painter.setPen(QPen{QColor{0, 0, 0}});
                    else
                        painter.setPen(QPen{QColor{255 - std::min(static_cast<int>(255 / results[i] / 0.8) + 50, 255),
                                                   255 - std::min(static_cast<int>(255 / results[i] / 0.8) + 50, 255),
                                                   255 - std::min(static_cast<int>(255 / results[i] / 4) + 50, 255)}});
                    painter.drawPoint(points[i].first.first, points[i].first.second);
                }
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
        painter.fillRect(10, 10, m_debugLabel->width() + 20, m_debugLabel->height() + 20, qApp->palette().base());
    }
    else
        painter.fillRect(rect(), qApp->palette().base());
}
