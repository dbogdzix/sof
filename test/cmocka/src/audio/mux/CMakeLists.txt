# SPDX-License-Identifier: BSD-3-Clause

SET(arch_src ${PROJECT_SOURCE_DIR}/src/audio/mux/mux.c
             ${PROJECT_SOURCE_DIR}/src/audio/mux/mux_generic.c)

# make small lib for stripping so we don't have to care
# about unused missing references

add_compile_options(-fdata-sections -ffunction-sections -DUNIT_TEST)
link_libraries(-Wl,--gc-sections)

add_library(mux_for_audio STATIC ${arch_src})

target_link_libraries(mux_for_audio PRIVATE sof_options)

link_libraries(mux_for_audio)

cmocka_test(
	mux_set_values
	mux_set_values.c
)

cmocka_test(
	mux_tests
	mux_tests.c
	mock.c
)

cmocka_test(
	mux_copy
	mux_copy.c
	mock.c
)

cmocka_test(
	mux_generic
	mux_generic.c
)
