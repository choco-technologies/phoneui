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
    dmdrvi@>=2.7
    dmosi
    libdmview@>=0.17
)

# The tour of the pages with the real view and the stub backend (ctest): every touch opens what it should, and every property the application
# sets exists in the view. The pictures go to the build directory (dmvshot-tour/).
find_program(DMF_GET_EXECUTABLE dmf-get HINTS ${DMOD_TOOLS_BIN_DIR} PATHS ENV PATH)
find_program(DMOD_LOADER_EXECUTABLE dmod_loader HINTS ${DMOD_TOOLS_BIN_DIR} PATHS ENV PATH)
if(DMF_GET_EXECUTABLE AND DMOD_LOADER_EXECUTABLE)
    foreach(SCREEN IN LISTS PHONEUI_SCREENS)
        add_test(
            NAME dmvshot_tour_${SCREEN}
            COMMAND ${CMAKE_COMMAND} -E env DMOD_DMF_DIR=${DMOD_DMF_DIR}
                bash -c "${DMF_GET_EXECUTABLE} install -d ${DMOD_DMF_DIR}/dmvshot-local.dmd -y && mkdir -p ${CMAKE_BINARY_DIR}/dmvshot-tour/${SCREEN} && ${DMOD_LOADER_EXECUTABLE} ${DMOD_DMF_DIR}/dmvshot.dmf --args ${CMAKE_BINARY_DIR}/views/${SCREEN}/phoneui.dmv ${CMAKE_BINARY_DIR}/dmvshot-tour/${SCREEN} shot:home click:200,307 expect:page:1 shot:apps click:210,240 expect:page:5 shot:store click:120,307 expect:page:0 click:76,271 expect:page:2 shot:inbox click:120,96 expect:page:3 shot:chat click:40,307 expect:page:2 click:20,9 expect:page:4 click:20,100 expect:quick:0 shot:quick click:120,307 click:120,235 expect:search"
        )
    endforeach()
endif()
