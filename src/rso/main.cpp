// src/main.cpp

#include <cstdlib>   // EXIT_FAILURE
#include <exception> // std::exception in catch
#include <iostream>  // std::cerr output

#include "Application.hpp"
#include "engine/ConsoleLogger.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	try
	{
		return Application::run(/* TODO pass args */); // returns EXIT_SUCCESS
	}
	catch ( const std::exception& e )
	{
		std::cerr << svk::ConsoleLogger::COLOR_RED << "Error: " << e.what() << svk::ConsoleLogger::COLOR_RESET << std::endl;
	}
	catch ( ... )
	{
		std::cerr << svk::ConsoleLogger::COLOR_RED << "An unknown error occurred." << svk::ConsoleLogger::COLOR_RESET << std::endl;
	}
	return EXIT_FAILURE; // If error occured 
}
