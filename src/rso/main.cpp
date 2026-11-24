// src/main.cpp

#include <cstdlib>   // EXIT_FAILURE
#include <exception> // std::exception in catch

#include "Application.hpp"
#include "engine/ConsoleLogger.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	const svk::ConsoleLogger logger;
	try
	{
		return Application::run(logger /* TODO pass args */); // returns EXIT_SUCCESS
	}
	catch (const std::exception& e)
	{
		logger.cError("Error: {}", e.what());
	}
	catch (...)
	{
		logger.cError("An unknown error occurred.");
	}
	return EXIT_FAILURE; // If error occured
}
