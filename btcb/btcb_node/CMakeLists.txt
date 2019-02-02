add_executable (btcb_node
	daemon.cpp
	daemon.hpp
	entry.cpp)

target_link_libraries (btcb_node
	node
	secure
	argon2
	Boost::boost
	${PLATFORM_LIBS}
)

target_compile_definitions(btcb_node
	PRIVATE
		-DBTCB_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR}
		-DBTCB_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR})

set_target_properties (btcb_node
	PROPERTIES
		COMPILE_FLAGS
			"-DQT_NO_KEYWORDS -DBOOST_ASIO_HAS_STD_ARRAY=1")

if ((BTCB_GUI OR RAIBLOCKS_GUI) AND NOT APPLE)
	install(TARGETS btcb_node
			RUNTIME DESTINATION bin
			LIBRARY DESTINATION lib
			ARCHIVE DESTINATION lib
	)
endif()
