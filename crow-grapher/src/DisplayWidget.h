#ifndef DISPLAY_WIGET_H
#define DISPLAY_WIGET_H

#include <QWidget>
#include <QtCharts>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

#include <ralgo/signal/fft.h>
#include <ralgo/signal/voice.h>
#include <ralgo/signal/convolution.h>
#include <nos/print.h>

#include <ralgo/util/math.h>

#define FFTBUF_OFFSET 8

class TrackWidget : public QWidget
{
public:

	static int const wider = 4;
	static int const string_size = ((1 << FFTBUF_OFFSET) / 2 + 1) * wider;
	static int const ywider = 4;

	TrackWidget() : QWidget(nullptr)
	{
		setMinimumSize(string_size, 200);
		//pixmap.fill(Qt::black);
		//setScaledContents(true);
		//setFixedSize(0,0);

		for (auto& f : imgbuf) f = 0x88888888;
	}

	uint32_t imgbuf[string_size * 200 * 2];

	//QImage img;

	int ysize;
	int y = 0;

	QMutex qmut;
	QImage img;
	QQueue<QVector<double>> vectors;

	void paintEvent(QPaintEvent*)
	{
		qmut.lock();

		while (!vectors.empty())
		{
			QVector<double> v = vectors.dequeue();
			qmut.unlock();

			ralgo::inplace::normalize(v);

			for (int x = 0; x < v.size(); ++x)
			{
				double val = v[x];
				uint8_t a = 255;
				uint8_t g = 0;
				//uint8_t r = val * 255;
				//uint8_t b = val * 255;
				//uint8_t r = val < 0.5 ? 0 : (val - 0.5) * 2 * 255;
				//uint8_t b = val > 0.5 ? 255 : val * 2 * 255;
				uint8_t r = val > 0 ? 0 : ralgo::clamp<double>(- val * 255 * 4, 0, 255);
				uint8_t b = val < 0 ? 0 : ralgo::clamp<double>(val * 255 * 4, 0, 255);
				uint32_t clr = (a << 24) | (r << 16) | (g << 8) | (b << 0);

				for (int k = 0; k < wider; ++k)
				{
					for (int l = 0; l < ywider; ++l)
					{
						imgbuf[x * wider + y * string_size + k + l * string_size] = clr;
						imgbuf[x * wider + y * string_size + string_size * 200 + k + l * string_size] = clr;
					}
				}
			}

			qmut.lock();
		}

		qmut.unlock();

		y += ywider;

		if (y >= 200) y = 0;

		img = QImage((uint8_t*) (imgbuf + y * string_size), string_size, 200, QImage::Format_ARGB32);

		QPainter painter(this);
		painter.drawImage(QRect(0, 0, string_size, 200), img);
	}

	void add_vector(const QVector<double>& vec)
	{
		qmut.lock();
		vectors.enqueue(vec);
		qmut.unlock();
		update();
	}
};

class DisplayWidget : public QWidget
{
	Q_OBJECT

	QChart *m_chart;
	QLineSeries *m_series ;
	QChartView *chartView;

	QChart *m_chart_fft;
	QLineSeries *m_series_fft;
	QChartView *chartView_fft;

	QChart *m_chart_mel;
	QLineSeries *m_series_mel;
	QChartView *chartView_mel;

	int buffer_size = 32000;
	const int fftbuf_size = (1 << FFTBUF_OFFSET);
	int yscale = 6000;
	int fft_yscale = 800;
	int cursor = 0;

	QVector<double> fftbuf;
	QVector<double> fftresult_fon;
	QVector<double> fftresult;
	QVector<QPointF> fftresult_vis;
	int fftbuf_cursor = 0;

	TrackWidget* timetrack;

	QVector<QPointF> buffer;
	QMutex mutex;

	std::vector<double> freqlist;

	std::vector<double> melfreqs;
	std::vector<ralgo::signal::triangle_window> windows;
	std::vector<std::vector<double>> windows_keys;
	std::vector<std::vector<double>> windows_values;

public:
	DisplayWidget(QWidget * parent = nullptr) : QWidget(parent),
		m_chart(new QChart),
		m_series(new QLineSeries),
		m_chart_fft(new QChart),
		m_series_fft(new QLineSeries),
		m_chart_mel(new QChart),
		m_series_mel(new QLineSeries)
	{
		chartView = new QChartView(m_chart);
		chartView_fft = new QChartView(m_chart_fft);
		chartView_mel = new QChartView(m_chart_mel);

		fftbuf.resize(fftbuf_size);
		fftresult.resize(fftbuf_size / 2 + 1);
		fftresult_fon.resize(fftbuf_size / 2 + 1);
		fftresult_vis.resize(fftbuf_size / 2 + 1);

		buffer.resize(buffer_size);

		for (int i = 0; i < buffer_size; ++i)
		{
			buffer[i] = QPointF(i, 0);
		}

		for (int i = 0; i < fftresult_vis.size(); ++i)
		{
			fftresult_vis[i] = QPointF(i, 0.00001);
		}

		//chartView->setMinimumSize(800, 600);
		m_chart->addSeries(m_series);
		m_chart_fft->addSeries(m_series_fft);
		m_chart_mel->addSeries(m_series_mel);

		QValueAxis *axisX = new QValueAxis;
		axisX->setTitleText("Samples");

		QValueAxis *axisY = new QValueAxis;
		axisY->setTitleText("Audio level");

		QValueAxis *axisX_fft = new QValueAxis;
		axisX_fft->setTitleText("Samples");

		//QLogValueAxis *axisY_fft = new QLogValueAxis;
		QLogValueAxis *axisY_fft = new QLogValueAxis;
		axisY_fft->setTitleText("Audio level");

		QValueAxis *axisX_mel = new QValueAxis;
		axisX_fft->setTitleText("Samples");

		QValueAxis *axisY_mel = new QValueAxis;
		axisY_fft->setTitleText("Audio level");

		m_chart->setAxisX(axisX, m_series);
		m_chart->setAxisY(axisY, m_series);
		m_chart_fft->setAxisX(axisX_fft, m_series_fft);
		m_chart_fft->setAxisY(axisY_fft, m_series_fft);
		m_chart_mel->setAxisX(axisX_mel, m_series_mel);
		m_chart_mel->setAxisY(axisY_mel, m_series_mel);
		m_chart->legend()->hide();
		m_chart_fft->legend()->hide();
		m_chart_mel->legend()->hide();

		axisX->setRange(0, buffer_size);
		axisY->setRange(-yscale, yscale);
		axisX_fft->setRange(0, fftresult.size());
		axisY_fft->setRange(0.01, 1500);
		axisY_mel->setRange(0, 1);

		m_series->replace(buffer);
		m_series->setUseOpenGL(true);
		m_series_fft->setUseOpenGL(true);
		m_series_mel->setUseOpenGL(true);

		timetrack = new TrackWidget;

		QVBoxLayout *mainLayout = new QVBoxLayout(this);
		mainLayout->addWidget(chartView);
		mainLayout->addWidget(chartView_fft);
		mainLayout->addWidget(chartView_mel);
		mainLayout->addWidget(timetrack);

		freqlist = ralgo::signal::rfftfreq(1 << FFTBUF_OFFSET, 1. / 8000);
		assert(freqlist.size() == fftbuf_size / 2 + 1);

		setMinimumSize(800, 1000);

		setLayout(mainLayout);

		melfreqs = ralgo::linspace<std::vector<double>>(0, ralgo::hz2mel(4000), 64);
		ralgo::mel2hz_vi(melfreqs);
		axisX_mel->setRange(0, melfreqs.size() - 2 - 1);

		//nos::println("melfreqs:", melfreqs);
		//nos::println("freqlist:", freqlist);

		for (int i = 0; i < melfreqs.size() - 2; ++i)
			windows.emplace_back(melfreqs[i], melfreqs[i + 2]);

		for (int i = 0; i < melfreqs.size() - 2; ++i)
		{
			//PRINT(i);
			windows_keys.push_back(ralgo::merge_sorted<std::vector<double>>(
			                           freqlist, windows[i].keypoints(), windows[i].strt, windows[i].fini));
			//PRINT(windows_keys[i]);
			windows_values.push_back(windows[i].lerp_values(windows_keys[i]));

			//PRINT(windows_values[i]);
		}
	}

	QQueue<QVector<int16_t>> q;

	void frepaint()
	{
		while (!q.empty())
		{
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
					//ralgo::signal::spectre(fftbuf.data(), fftresult.data(), fftbuf_size, fftresult.size());

//					std::vector<double> inverse_fft(fftresult.size());
//					ralgo::signal::spectre(fftresult.data(), inverse_fft.data(), fftresult.size(), fftresult.size());

					std::vector<std::complex<double>> extdata(std::begin(fftbuf), std::end(fftbuf));
					ralgo::signal::fft(extdata);

					fftresult = ralgo::vecops::abs<QVector<double>>
					            (ralgo::vecops::slice(extdata, 0, freqlist.size()));

					ralgo::vecops::inplace::div(fftresult, freqlist.size());

					QVector<double> mels(windows_keys.size());

					for (int i = 0; i < windows_keys.size(); ++i)
					{
						auto values = ralgo::signal::lerp_values<std::vector<double>>(fftresult, freqlist, windows_keys[i]);
						auto muls = ralgo::vecops::mul_vv(values, windows_values[i]);
						//ralgo::inplace::elementwise(muls, [](auto & x) { return x * x; });
						mels[i] = ralgo::trapz(windows_keys[i], muls);
					}

					//ralgo::inplace::elementwise(mels, [](auto & x) { return log(x); });
					ralgo::inplace::normalize(mels);

					QVector<QPointF> mels_viz = ralgo::elementwise2<QVector<QPointF>>(
					[](double y, int x) {return QPointF(x, y); },
					mels,
					ralgo::arange<std::vector<int>>(mels.size()));

					for (unsigned int i = 0; i < fftresult_vis.size(); ++i)
					{
						double r = fftresult[i];
						fftresult_vis[i] = QPointF(i, r);
					}

					ralgo::inplace::log(fftresult);
					
					std::vector<std::complex<double>> tmp = 
						ralgo::elementwise<std::vector<std::complex<double>>>(
					[](double x) {return std::complex<double>(x); },
					fftresult);

					tmp.resize(128);

					ralgo::signal::ifft(tmp);					
					fftresult = ralgo::vecops::abs<QVector<double>>(tmp);

					ralgo::inplace::normalize(fftresult);
					for (int i = 0; i < 10; ++i) 
					{
						fftresult[i] = 0.01;
						fftresult[fftresult.size()-i-1] = 0.01;
					}
					ralgo::inplace::normalize(fftresult);


					//dprln("here");*/
					
					m_series_fft->replace(fftresult_vis);
					m_series_mel->replace(mels_viz);
					timetrack->add_vector(fftresult);
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