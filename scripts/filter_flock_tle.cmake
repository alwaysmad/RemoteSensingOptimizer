if(NOT DEFINED RAW_TLE_FILE)
	message(FATAL_ERROR "RAW_TLE_FILE is not set")
endif()

if(NOT DEFINED OUTPUT_HEADER_FILE)
	message(FATAL_ERROR "OUTPUT_HEADER_FILE is not set")
endif()

file(READ "${RAW_TLE_FILE}" raw_tle_text)
string(REPLACE "\r\n" "\n" raw_tle_text "${raw_tle_text}")
string(REPLACE "\r" "\n" raw_tle_text "${raw_tle_text}")
string(REGEX REPLACE "\n+$" "" raw_tle_text "${raw_tle_text}")
string(REPLACE "\n" ";" raw_tle_lines "${raw_tle_text}")

set(filtered_records "")
set(record_lines "")

foreach(line IN LISTS raw_tle_lines)
	if(line STREQUAL "")
		continue()
	endif()

	list(APPEND record_lines "${line}")
	list(LENGTH record_lines record_length)
	if(record_length EQUAL 3)
		list(GET record_lines 0 satellite_name)
		list(GET record_lines 1 line_one)
		list(GET record_lines 2 line_two)

		string(FIND "${satellite_name}" "FLOCK" flock_match)
		if(NOT flock_match EQUAL -1)
			list(APPEND filtered_records "${satellite_name}" "${line_one}" "${line_two}")
		endif()

		set(record_lines "")
	endif()
endforeach()

list(LENGTH filtered_records filtered_line_count)
math(EXPR tle_count "${filtered_line_count} / 3")

get_filename_component(output_dir "${OUTPUT_HEADER_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

set(header_text "#pragma once\n\n#include <array>\n#include <SGP4.h>\n\nnamespace FLOCK_tle_data\n{\n")
string(APPEND header_text "inline const std::array<libsgp4::Tle, ${tle_count}> tle_data = {\n")

set(record_index 0)
while(record_index LESS filtered_line_count)
	list(GET filtered_records ${record_index} satellite_name)
	math(EXPR line_one_index "${record_index} + 1")
	math(EXPR line_two_index "${record_index} + 2")
	list(GET filtered_records ${line_one_index} line_one)
	list(GET filtered_records ${line_two_index} line_two)

	string(APPEND header_text "	libsgp4::Tle(\"")
	string(APPEND header_text "${satellite_name}")
	string(APPEND header_text "\", \"")
	string(APPEND header_text "${line_one}")
	string(APPEND header_text "\", \"")
	string(APPEND header_text "${line_two}")
	string(APPEND header_text "\")")
	math(EXPR next_record_index "${record_index} + 3")
	if(next_record_index LESS filtered_line_count)
		string(APPEND header_text ",")
	endif()
	string(APPEND header_text "\n")

	math(EXPR record_index "${record_index} + 3")
endwhile()

string(APPEND header_text "};\n} // namespace FLOCK_tle_data\n")

file(WRITE "${OUTPUT_HEADER_FILE}" "${header_text}")