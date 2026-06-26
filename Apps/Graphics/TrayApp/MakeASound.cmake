set(MAKEASOUND_BUILD_APPS OFF CACHE BOOL "" FORCE)

CPMAddPackage(
        NAME MakeASound
        GITHUB_REPOSITORY eyalamirmusic/MakeASound
        GIT_TAG main
        OPTIONS
            "MAKEASOUND_BUILD_APPS OFF")
