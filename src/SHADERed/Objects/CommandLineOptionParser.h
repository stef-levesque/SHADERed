#pragma once
#include <SHADERed/FS.h>
#include <string>

namespace ed {
	class CommandLineOptionParser {
	public:
		CommandLineOptionParser();

		void Parse(const fs::path& cmdDir, int argc, char* argv[]);

		bool LaunchUI;

		bool Fullscreen;
		bool Maximized;
		bool PerformanceMode;
		int WindowWidth, WindowHeight;
		bool MinimalMode;
		std::string ProjectFile;
	};
}