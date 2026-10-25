include_guard(GLOBAL)

# Patch the reviewed upstream text in the caller's scope; never silently accept
# a changed anchor. The upstream checkout itself remains read-only.
function(drakengard_replace_pinned variable anchor replacement contract)
    string(FIND "${${variable}}" "${anchor}" position)
    if(position LESS 0)
        message(FATAL_ERROR "Pinned ${contract} changed")
    endif()
    string(REPLACE "${anchor}" "${replacement}" patched "${${variable}}")
    set("${variable}" "${patched}" PARENT_SCOPE)
endfunction()
