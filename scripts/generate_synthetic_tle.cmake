# scripts/generate_synthetic_tle.cmake
if(NOT DEFINED OUTPUT_HEADER_FILE)
    message(FATAL_ERROR "OUTPUT_HEADER_FILE is not set")
endif()

if(NOT DEFINED NUM_SATELLITES)
    set(NUM_SATELLITES 10)
endif()

# Interval parameters in degrees (e.g. span 60 deg starting from 30 deg)
if(NOT DEFINED RAAN_START_DEG)
    set(RAAN_START_DEG 0)
endif()

if(NOT DEFINED RAAN_SPAN_DEG)
    set(RAAN_SPAN_DEG 60)
endif()

# Baseline Orbital Parameters (500 km SSO, Epoch 2026 day 100.0)
set(EPOCH_STR "26100.00000000")
set(INCLINATION " 97.4065")
set(ECC_STR "0001000")
set(ARG_PERIGEE "  0.0000")
set(MEAN_ANOMALY "  0.0000")
set(MEAN_MOTION "15.21850000")
set(BASE_NORAD_ID 90001)

# Helper function to compute standard modulo-10 NORAD TLE checksum
function(compute_tle_checksum input_line out_var)
    string(LENGTH "${input_line}" len)
    set(checksum 0)
    math(EXPR last_idx "${len} - 1")
    foreach(i RANGE 0 ${last_idx})
        string(SUBSTRING "${input_line}" ${i} 1 ch)
        if(ch MATCHES "[0-9]")
            math(EXPR checksum "(${checksum} + ${ch}) % 10")
        elseif(ch STREQUAL "-")
            math(EXPR checksum "(${checksum} + 1) % 10")
        endif()
    endforeach()
    set(${out_var} "${checksum}" PARENT_SCOPE)
endfunction()

# Prepare Header Output
get_filename_component(output_dir "${OUTPUT_HEADER_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

set(header_text "#pragma once\n\n#include <array>\n#include <SGP4.h>\n\nnamespace SYNTH_tle_data\n{\n")
string(APPEND header_text "inline const std::array<libsgp4::Tle, ${NUM_SATELLITES}> tle_data = {\n")

math(EXPR max_k "${NUM_SATELLITES} - 1")

# Scale by 10000 for 4 decimal places of fixed-point precision
math(EXPR raan_start_scaled "${RAAN_START_DEG} * 10000")
math(EXPR raan_span_scaled "${RAAN_SPAN_DEG} * 10000")

foreach(k RANGE 0 ${max_k})
    math(EXPR norad_id "${BASE_NORAD_ID} + ${k}")
    math(EXPR sat_num "${k} + 1")

    # Format Satellite Name (padded to standard 24 chars)
    string(LENGTH "${sat_num}" sat_num_len)
    if(sat_num_len EQUAL 1)
        set(name_str "SYNTH FLOCK 0${sat_num}           ")
    else()
        set(name_str "SYNTH FLOCK ${sat_num}           ")
    endif()

    if(NUM_SATELLITES GREATER 1)
        math(EXPR raan_step "(${k} * ${raan_span_scaled}) / ${max_k}")
        math(EXPR raan_int "(${raan_start_scaled} + ${raan_step}) % 3600000")
    else()
        math(EXPR raan_int "${raan_start_scaled} % 3600000")
    endif()

    math(EXPR raan_whole "${raan_int} / 10000")
    math(EXPR raan_frac "${raan_int} % 10000")

    # Zero-pad fraction (4 digits)
    string(LENGTH "${raan_frac}" frac_len)
    while(frac_len LESS 4)
        set(raan_frac "0${raan_frac}")
        string(LENGTH "${raan_frac}" frac_len)
    endwhile()

    # Right-pad whole degrees (3 digits)
    string(LENGTH "${raan_whole}" whole_len)
    if(whole_len EQUAL 1)
        set(raan_fmt "  ${raan_whole}.${raan_frac}")
    elseif(whole_len EQUAL 2)
        set(raan_fmt " ${raan_whole}.${raan_frac}")
    else() # 3 digits
        set(raan_fmt "${raan_whole}.${raan_frac}")
    endif()

    # Line 1: Cols 1-68 (Exactly 68 chars), Zero BSTAR / drag derivative ensures pure kinematic SSO propagation
    # Col 64: Ephemeris type '0', Col 65-68: Element set '   1'
    set(line1_raw "1 ${norad_id}U 26001A   ${EPOCH_STR}  .00000000  00000-0  10000-4 0    1")
    compute_tle_checksum("${line1_raw}" cs1)
    set(line1 "${line1_raw}${cs1}")

    # Line 2: Cols 1-68 (Exactly 68 chars), Even RAAN distribution, fixed M, w, i, n
    # Col 64-68: Rev number '00001'
    set(line2_raw "2 ${norad_id} ${INCLINATION} ${raan_fmt} ${ECC_STR} ${ARG_PERIGEE} ${MEAN_ANOMALY} ${MEAN_MOTION}00001")
    compute_tle_checksum("${line2_raw}" cs2)
    set(line2 "${line2_raw}${cs2}")

    # Append entry
    string(APPEND header_text "	libsgp4::Tle(\"${name_str}\", \"${line1}\", \"${line2}\")")
    if(k LESS max_k)
        string(APPEND header_text ",")
    endif()
    string(APPEND header_text "\n")
endforeach()

string(APPEND header_text "};\n} // namespace SYNTH_tle_data\n")

file(WRITE "${OUTPUT_HEADER_FILE}" "${header_text}")