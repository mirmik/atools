#include <QtWidgets>
#include <DisplayWidget.h>

#include <crow/hexer.h>
#include <crow/pubsub.h>
#include <crow/tower.h>
#include <crow/gates/udpgate.h>

#include <nos/print.h>
#include <nos/fprint.h>

DisplayWidget * w;

std::vector<uint8_t> crowker;
crow::subscriber data_theme;

void data_theme_handler(crow::packet * pack) 
{
	QVector<int16_t> vec;
	igris::buffer data = crow::pubsub::get_data(pack);

	if (data.size() % sizeof(uint16_t) != 0) 
	{
		nos::fprintln("Warn: wrong package size {}", data.size());
		crow::release(pack);
		return;
	}

	vec.resize(data.size() / sizeof(uint16_t));

	memcpy(vec.data(), data.data(), data.size()); 

	w->new_package(std::move(vec));

	crow::release(pack);
}

int main(int argc, char * argv []) 
{
	if (argc < 3) 
	{
		nos::fprintln("Usage: {} crow_address theme", argv[0]);
		exit(-1);
	}

	//crow::diagnostic_enable();
	crow::create_udpgate(12, 0);

	crowker = compile_address(argv[1]);
	std::string theme = argv[2];

	crow::pubsub_protocol.enable();
	crow::pubsub_protocol.start_resubscribe_thread(1000);

	QApplication app(argc, argv);
	
	w = new DisplayWidget;

	QObject::connect(w, &DisplayWidget::updsignal, w, &DisplayWidget::frepaint);

	crow::start_thread();
	data_theme.subscribe(crowker, theme.c_str(), 0, 200, 0, 200, data_theme_handler);

	w->show();
	app.exec();
}