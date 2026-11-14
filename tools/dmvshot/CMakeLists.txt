# dmvshot - the interface drawn on a PC (host only): the real view, src/ui.c and the stub backend (see main.c)
set(DMOD_MODULE_NAME        dmvshot)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         8192)
set(DMOD_PRIORITY           0)
set(DMOD_DMR_PATH           ${CMAKE_CURRENT_SOURCE_DIR}/dmvshot.dmr)
set(DMOD_MAL_IMPLS          )

dmod_add_executable(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    main.c
    ../../src/ui.c
    ../../src/backend_stub.c
)
target_include_directories(${DMOD_MODULE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

dmod_link_modules(${DMOD_MODULE_NAME}
    dmdrvi@>=2.15
    dmosi
    libdmview@>=0.17
)

# The tour of the pages with the real view and the stub backend (ctest): every touch opens what it should, and every property the application
# sets exists in the view. The applications are the three icons of tests/icons; they cannot be started on a PC, so a touch on one goes to its
# page and back to the applications. The pictures go to the build directory (dmvshot-tour/).
find_program(DMF_GET_EXECUTABLE dmf-get HINTS ${DMOD_TOOLS_BIN_DIR} PATHS ENV PATH)
find_program(DMOD_LOADER_EXECUTABLE dmod_loader HINTS ${DMOD_TOOLS_BIN_DIR} PATHS ENV PATH)
if(DMF_GET_EXECUTABLE AND DMOD_LOADER_EXECUTABLE)
    foreach(SCREEN IN LISTS PHONEUI_SCREENS)
        add_test(
            NAME dmvshot_tour_${SCREEN}
            COMMAND ${CMAKE_COMMAND} -E env DMOD_DMF_DIR=${DMOD_DMF_DIR} PHONEUI_ICONS=${CMAKE_CURRENT_SOURCE_DIR}/../../tests/icons
                bash -c "${DMF_GET_EXECUTABLE} install -d ${DMOD_DMF_DIR}/dmvshot-local.dmd -y && mkdir -p ${CMAKE_BINARY_DIR}/dmvshot-tour/${SCREEN} && ${DMOD_LOADER_EXECUTABLE} ${DMOD_DMF_DIR}/dmvshot.dmf --args ${CMAKE_BINARY_DIR}/views/${SCREEN}/phoneui.dmv ${CMAKE_BINARY_DIR}/dmvshot-tour/${SCREEN} shot:home emit:char:88 click:120,235 expect:search emit:char:72 emit:char:105 emit:char:261 emit:char:35 emit:char:49 emit:char:40 emit:char:63 shot:typed emit:key:42 shot:erased emit:key:41 click:200,307 expect:page:1 wait:600 shot:apps click:30,80 wait:600 expect:page:1 shot:apps-launch-failed click:120,307 expect:page:0 wait:3000 click:76,271 expect:page:2 shot:inbox click:120,96 expect:page:3 shot:chat click:40,307 expect:page:2 click:20,9 expect:page:4 click:20,100 expect:quick:0 shot:quick click:120,307 click:120,235 expect:search"
        )
    endforeach()
endif()
