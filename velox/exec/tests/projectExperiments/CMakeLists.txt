
add_executable(project_plan_architecture_experiment ProjectPlanArchitectureExperiment.cpp)

target_link_libraries(
  project_plan_architecture_experiment
  velox_aggregates
  velox_type
  velox_vector
  velox_vector_test_lib
  velox_exec
  velox_exec_test_lib
  velox_tpch_connector
  velox_memory)


add_executable(project_query_modif_experiment ProjectQueryModifExperiment.cpp)

target_link_libraries(
  project_query_modif_experiment
  velox_aggregates
  velox_type
  velox_vector
  velox_vector_test_lib
  velox_exec
  velox_exec_test_lib
  velox_tpch_connector
  velox_memory)

add_executable(project_hardcoded_queries_experiment ProjectHardcodedQueriesExperiment.cpp)

target_link_libraries(
  project_hardcoded_queries_experiment
  velox_aggregates
  velox_type
  velox_vector
  velox_vector_test_lib
  velox_exec
  velox_exec_test_lib
  velox_tpch_connector
  velox_memory)

add_executable(project_compilation_times_experiment ProjectCompilationTimesExperiment.cpp)

target_link_libraries(
  project_compilation_times_experiment
  velox_aggregates
  velox_type
  velox_vector
  velox_vector_test_lib
  velox_exec
  velox_exec_test_lib
  velox_tpch_connector
  velox_memory)

#add_subdirectory(vtuneArchitectures)