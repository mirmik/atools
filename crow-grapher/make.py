#!/usr/bin/env python3

import licant
import os

licant.include("igris")
licant.include("nos")
licant.include("crow")
licant.include("ralgo")
licant.include("linalg-v3")

#os.environ["LD_LIBRARY_PATH"] = "/usr/lib/x86_64-linux-gnu/"

licant.cxx_application("crow-chart",
	mdepends = [
		"crow",
		"crow.threads",

		"nos",

		"ralgo",

		"igris",
		"igris.syslock",

		"crow.udpgate",
		("igris.ctrobj", "linux")
	],

	sources = ["src/main.cpp"],
	include_paths = [".", 
	"src",
	"/usr/include/x86_64-linux-gnu/qt5/", 
	"/usr/include/x86_64-linux-gnu/qt5/QtCore",
	"/usr/include/x86_64-linux-gnu/qt5/QtWidgets",
	"/usr/include/x86_64-linux-gnu/qt5/QtCharts"],

	cxx_flags = "-fPIC -Xlinker -Map=output.map",
	ld_flags = "-fPIC -Xlinker -Map=output.map",

	moc=["src/DisplayWidget.h"],

	libs=["Qt5Core", "Qt5Gui", "Qt5Widgets", "Qt5Charts", "pthread"]
)

licant.ex("crow-chart")