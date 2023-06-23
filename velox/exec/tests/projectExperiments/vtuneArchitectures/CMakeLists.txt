add_executable(filterproj FilterProj.cpp)

target_link_libraries(
  filterproj
  velox_aggregates
  velox_type
  velox_vector
  velox_vector_test_lib
  velox_exec
  velox_exec_test_lib
  velox_tpch_connector
  velox_memory
  -lpthread
  /opt/intel/oneapi/vtune/latest/sdk/lib64/libittnotify.a
  /opt/intel/oneapi/vtune/latest/sdk/lib64
  -ldl
)

add_executable(batchq BatchedQueries.cpp)

target_link_libraries(
  batchq
  velox_aggregates
  velox_type
  velox_vector
  velox_vector_test_lib
  velox_exec
  velox_exec_test_lib
  velox_tpch_connector
  velox_memory
  -lpthread
  /opt/intel/oneapi/vtune/latest/sdk/lib64/libittnotify.a
  /opt/intel/oneapi/vtune/latest/sdk/lib64
  -ldl
)

add_executable(filter Filter.cpp)

target_link_libraries(
  filter
  velox_aggregates
  velox_type
  velox_vector
  velox_vector_test_lib
  velox_exec
  velox_exec_test_lib
  velox_tpch_connector
  velox_memory
  -lpthread
  /opt/intel/oneapi/vtune/latest/sdk/lib64/libittnotify.a
  /opt/intel/oneapi/vtune/latest/sdk/lib64
  -ldl
)

add_executable(partfield PartitionField.cpp)

target_link_libraries(
  partfield
  velox_aggregates
  velox_type
  velox_vector
  velox_vector_test_lib
  velox_exec
  velox_exec_test_lib
  velox_tpch_connector
  velox_memory
  -lpthread
  /opt/intel/oneapi/vtune/latest/sdk/lib64/libittnotify.a
  /opt/intel/oneapi/vtune/latest/sdk/lib64
  -ldl
)

add_executable(partrrobin PartitionRoundRobin.cpp)

target_link_libraries(
  partrrobin
  velox_aggregates
  velox_type
  velox_vector
  velox_vector_test_lib
  velox_exec
  velox_exec_test_lib
  velox_tpch_connector
  velox_memory
  -lpthread
  /opt/intel/oneapi/vtune/latest/sdk/lib64/libittnotify.a
  /opt/intel/oneapi/vtune/latest/sdk/lib64
  -ldl
)

add_executable(prepartdata PrePartitionData.cpp)

target_link_libraries(
  prepartdata
  velox_aggregates
  velox_type
  velox_vector
  velox_vector_test_lib
  velox_exec
  velox_exec_test_lib
  velox_tpch_connector
  velox_memory
  -lpthread
  /opt/intel/oneapi/vtune/latest/sdk/lib64/libittnotify.a
  /opt/intel/oneapi/vtune/latest/sdk/lib64
  -ldl
)