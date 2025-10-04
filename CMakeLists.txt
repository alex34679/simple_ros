cmake_minimum_required(VERSION 3.10)
project(simple_ros_comm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== 找到依赖 =====
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)


message(STATUS "Protobuf include dirs: ${Protobuf_INCLUDE_DIRS}")
message(STATUS "Protobuf libs: ${Protobuf_LIBRARIES}")
message(STATUS "gRPC include dirs: ${gRPC_INCLUDE_DIRS}")
message(STATUS "gRPC libs: ${gRPC_LIBRARIES}")

# ===== 核心库 =====
add_library(ros_rpc_core
    src/ros_rpc_client.cpp
    src/ros_rpc_server.cpp
    src/poll_manager.cpp
    src/msg_factory.cpp
    src/global_init.cpp
    src/subscriber.cpp
    src/node_handle.cpp
    src/message_graph.cpp
    src/timer.cpp
    src/master_tcp_server.cpp
    src/subscription_handler_registry.cpp
    src/generated/ros_rpc.pb.cc
    src/generated/ros_rpc.grpc.pb.cc
    src/generated/example.pb.cc
    src/generated/marker.pb.cc
    src/generated/geometry_msgs.pb.cc
)

target_include_directories(ros_rpc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated
    ${Protobuf_INCLUDE_DIRS}
    ${gRPC_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
)

target_link_libraries(ros_rpc_core
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
        gRPC::grpc++_reflection
        pthread
        muduo_net
        muduo_base
        nlohmann_json::nlohmann_json
)

# ===== 测试 =====
enable_testing()

# 测试列表
set(TESTS
    test/test_ros_rpc.cpp
    test/test_poll_manager.cpp
    test/test_msg_factory.cpp
    test/test_init.cpp
    test/test_sub.cpp
    test/test_pub.cpp
)

# 编译测试文件 -> 放到 bin/tests
foreach(test_src IN LISTS TESTS)
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE ros_rpc_core gtest gtest_main pthread)
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
    )
    add_test(NAME ${test_name} COMMAND ${CMAKE_BINARY_DIR}/bin/tests/${test_name})
endforeach()

# ===== 工具 =====
set(TOOLS
    tools/master.cpp
    tools/rosnode.cpp
    tools/rostopic.cpp
)

foreach(tool_src IN LISTS TOOLS)
    get_filename_component(tool_name ${tool_src} NAME_WE)
    add_executable(${tool_name} ${tool_src})
    target_link_libraries(${tool_name} PRIVATE ros_rpc_core)
    set_target_properties(${tool_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tools
    )
endforeach()


# ===== Foxglove Bridge (可选) =====
option(ENABLE_FOXGLOVE "Enable Foxglove visualization support" ON)
if(ENABLE_FOXGLOVE)
    set(FOXGLOVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/foxglove")
    
    # 编译 marker Protobuf
    set(MARKER_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/marker.pb.cc)
    
    # Foxglove Bridge 库
    add_library(foxglove_bridge
        src/foxglove_bridge.cpp
        ${MARKER_PROTO}
    )
    
    target_include_directories(foxglove_bridge PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/generated
        ${CMAKE_CURRENT_BINARY_DIR}
        ${FOXGLOVE_DIR}/include
        ${Protobuf_INCLUDE_DIRS}
        ${gRPC_INCLUDE_DIRS}
    )
    
    # 添加 Foxglove C++ wrapper 源码
    file(GLOB FOXGLOVE_SOURCES CONFIGURE_DEPENDS
        ${FOXGLOVE_DIR}/src/*.cpp
        ${FOXGLOVE_DIR}/src/server/*.cpp
        ${FOXGLOVE_DIR}/src/service/*.cpp
        ${FOXGLOVE_DIR}/src/parameter/*.cpp
    )
    target_sources(foxglove_bridge PRIVATE ${FOXGLOVE_SOURCES})
    
    # 链接库顺序：C++ wrapper -> C SDK
    target_link_libraries(foxglove_bridge
        PUBLIC
        ros_rpc_core
        protobuf::libprotobuf
        gRPC::grpc++
        muduo_net
        muduo_base
        nlohmann_json::nlohmann_json
        ${FOXGLOVE_DIR}/lib/libfoxglove.a
        pthread   
        dl        
        rt       
)

    target_compile_definitions(foxglove_bridge PUBLIC ENABLE_FOXGLOVE)
    
    # Foxglove Bridge 工具
    add_executable(foxglove_bridge_tool tools/foxglove_bridge_node.cpp)
    set_target_properties(foxglove_bridge_tool PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tools
    )
    target_link_libraries(foxglove_bridge_tool PRIVATE foxglove_bridge)
    
    # 示例程序
    add_executable(quad_simulator_node examples/quad_simulator.cpp)
    set_target_properties(quad_simulator_node PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples
    )
    target_link_libraries(quad_simulator_node PRIVATE foxglove_bridge)

    add_executable(quad_visualizer_node examples/quad_visualizer.cpp)
    set_target_properties(quad_visualizer_node PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples
    )
    target_link_libraries(quad_visualizer_node PRIVATE foxglove_bridge)
    
    message(STATUS "Foxglove Bridge enabled")
else()
    message(STATUS "Foxglove Bridge disabled - bridge files will not be generated")
endif()
