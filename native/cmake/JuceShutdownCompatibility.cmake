# MuseScore 4.7 can release a VST instance from its static Async queue destructor,
# after JUCE's static DeletedAtShutdown registry has already been destroyed.
# Keep the tiny registry container alive for the process lifetime. deleteAll()
# still destroys the registered objects and clears its backing storage normally.
# This changes only this project's pinned JUCE checkout, never a system library.
if(UNIX AND NOT APPLE)
    set(_shutdown_source "${JUCE_SOURCE_CHECKOUT}/modules/juce_events/messages/juce_DeletedAtShutdown.cpp")
    file(READ "${_shutdown_source}" _shutdown_contents)
    if(NOT _shutdown_contents MATCHES "process-lifetime registry: supports hosts")
        string(REPLACE "\r\n" "\n" _shutdown_contents "${_shutdown_contents}")
        set(_original "    static Array<DeletedAtShutdown*> objects;\n    return objects;")
        set(_replacement "    // ScoreBacking process-lifetime registry: supports hosts with late VST cleanup.\n    static auto* const objects = new Array<DeletedAtShutdown*>;\n    return *objects;")
        string(FIND "${_shutdown_contents}" "${_original}" _original_position)
        if(_original_position EQUAL -1)
            message(FATAL_ERROR "JUCE shutdown compatibility patch does not match; use JUCE 7.0.12")
        endif()
        string(REPLACE "${_original}" "${_replacement}" _shutdown_contents "${_shutdown_contents}")
        file(WRITE "${_shutdown_source}" "${_shutdown_contents}")
    endif()
endif()
