#include "Logger.h"
#include <iostream>
#include <fstream>
#include "Exception.h"

std::ofstream LogFile;

void InitLogger(const fpath& runtime, bool probe)
{
	std::filesystem::create_directories(runtime);
	std::filesystem::current_path(runtime);
	LogFile.open(runtime / (probe ? "probe.log" : "game.log"), std::ios::trunc);
	AddVectoredExceptionHandler(1, HandleException);
	Log("Started");
}

void Log(const str& message)
{
	std::cout << message << std::endl;
	if (LogFile) {
		LogFile << message << '\n';
		LogFile.flush();
	}
}