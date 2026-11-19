execute_process(
    COMMAND "${EXAMPLE}"
    INPUT_FILE "${INPUT}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "CLI failed (${result}): ${error}")
endif()
foreach(expected IN ITEMS
        "Commands:"
        "error: usage: event Sensed"
        "error: usage: set"
        "error: unknown guard unknown"
        "error: unknown event UnknownEvent"
        "error: unexpected event argument"
        "error: unexpected argument"
        "error: event TargetSpotted is undefined in NpcMap::Idle"
        "state: CombatMap::Strafe, stack: 1"
        "guards: boss=false token=true lowhp=true far=false"
        "state: CombatMap::Retreat, stack: 1"
        "state: NpcMap::Returning, stack: 0")
    string(FIND "${output}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "CLI output missing '${expected}':\n${output}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E echo ""
    COMMAND "${EXAMPLE}"
    OUTPUT_QUIET
    ERROR_VARIABLE eof_error
    RESULT_VARIABLE eof_result)
if(NOT eof_result EQUAL 0)
    message(FATAL_ERROR "CLI failed on EOF (${eof_result}): ${eof_error}")
endif()
