#pragma once
#include <string>
#include <ghc/filesystem.hpp>

namespace ed {
	class CommandLineOptionParser {
	public:
		CommandLineOptionParser();

		void Parse(const ghc::filesystem::path& cmdDir, int argc, char* argv[]);

		bool LaunchUI;

		bool Fullscreen;
		bool Maximized;
		bool PerformanceMode;
		int WindowWidth, WindowHeight;
		bool MinimalMode;
		std::string ProjectFile;
	};
}