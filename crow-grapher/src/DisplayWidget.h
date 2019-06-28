#ifndef DISPLAY_WIGET_H
#define DISPLAY_WIGET_H

#include <QWidget>
#include <QtCharts>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

#include <ralgo/signal/spectre.h>
#include <nos/print.h>

class DisplayWidget : public QWidget
{
	Q_OBJECT

	QChart *m_chart;
    QLineSeries *m_series ;
    QChartView *chartView;

	QChart *m_chart_fft;
    QLineSeries *m_series_fft;
    QChartView *chartView_fft;

    int buffer_size = 32000;
    const int fftbuf_size = (1<<9);
    int yscale = 6000;
    int fft_yscale = 800;
    int cursor = 0;

    QVector<double> fftbuf;
    QVector<double> fftresult_fon;
    QVector<double> fftresult;
    QVector<QPointF> fftresult_vis;
    int fftbuf_cursor = 0;

    QVector<QPointF> buffer;
    QMutex mutex;

public:
	DisplayWidget(QWidget * parent = nullptr) : QWidget(parent),
		m_chart(new QChart),
	    m_series(new QLineSeries),
		m_chart_fft(new QChart),
	    m_series_fft(new QLineSeries)
	{
		chartView = new QChartView(m_chart);
		chartView_fft = new QChartView(m_chart_fft);

		fftbuf.resize(fftbuf_size);
		fftresult.resize(fftbuf_size / 2);
		fftresult_fon.resize(fftbuf_size / 2);
		fftresult_vis.resize(fftbuf_size / 2);

		buffer.resize(buffer_size);
		for (int i = 0; i< buffer_size; ++i) 
		{
			buffer[i] = QPointF(i,0);		
		}
		for (int i = 0; i< fftresult_vis.size(); ++i) 
		{
			fftresult_vis[i] = QPointF(i,0.00001);
		}

		//chartView->setMinimumSize(800, 600);
		m_chart->addSeries(m_series);
		m_chart_fft->addSeries(m_series_fft);

		QValueAxis *axisX = new QValueAxis;
		axisX->setTitleText("Samples");

		QValueAxis *axisY = new QValueAxis;
		axisY->setTitleText("Audio level");

		QValueAxis *axisX_fft = new QValueAxis;
		axisX_fft->setTitleText("Samples");

		QLogValueAxis *axisY_fft = new QLogValueAxis;
		//QValueAxis *axisY_fft = new QValueAxis;
		axisY_fft->setTitleText("Audio level");

		m_chart->setAxisX(axisX, m_series);
		m_chart->setAxisY(axisY, m_series);
		m_chart_fft->setAxisX(axisX_fft, m_series_fft);
		m_chart_fft->setAxisY(axisY_fft, m_series_fft);
		m_chart->legend()->hide();
		m_chart_fft->legend()->hide();

		axisX->setRange(0, buffer_size);
		axisY->setRange(-yscale, yscale);  
		axisX_fft->setRange(0, fftresult.size());
		axisY_fft->setRange(0.01, 1500);  
		//axisY_fft->setRange(-1000, 1000);  

		m_series->replace(buffer);
		m_series->setUseOpenGL(true);
		m_series_fft->setUseOpenGL(true);

		QVBoxLayout *mainLayout = new QVBoxLayout(this);
		mainLayout->addWidget(chartView);
		mainLayout->addWidget(chartView_fft);

		setMinimumSize(1000,1000);

		setLayout(mainLayout);
	}

	QQueue<QVector<int16_t>> q;

	void frepaint() 
	{
		while(!q.empty()) {
			mutex.lock();
			QVector<int16_t> v = q.dequeue();
			mutex.unlock();
			for (auto u : v) 
			{
				buffer[cursor] = QPointF(cursor, u);
				fftbuf[fftbuf_cursor++] = u;
				if (++cursor == buffer_size) cursor = 0;
				if (fftbuf_cursor == fftbuf_size) 
				{
					fftbuf_cursor = 0;
					ralgo::signal::spectre(fftbuf.data(), fftresult.data(), fftbuf_size, fftresult.size());

					for (unsigned int i = 0; i < fftresult_vis.size(); ++i) {
						double r = fftresult[i];
						//fftresult_fon[i] += 0.001 * (r - fftresult_fon[i]);
						fftresult_vis[i] = QPointF(i, r);
						m_series_fft->replace(fftresult_vis);
					}
				}
			}
		}

		m_series->replace(buffer);		
	}

	void new_package(QVector<int16_t>&& arr) 
	{
		mutex.lock();
		q.enqueue(arr);
		mutex.unlock();
		emit updsignal();
	}

	signals:
		void updsignal();
};

#endif