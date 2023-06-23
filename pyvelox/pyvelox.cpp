/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pyvelox.h"
#include "signatures.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <unordered_set>

#include <folly/init/Init.h>
#include "velox/connectors/tpch/TpchConnector.h"
#include "velox/connectors/tpch/TpchConnectorSplit.h"
#include "velox/core/Expressions.h"
#include "velox/exec/tests/utils/AssertQueryBuilder.h"
#include "velox/exec/tests/utils/PlanBuilder.h"
#include "velox/exec/tests/utils/ProjectModifyExpressionUtils.h"
#include "velox/exec/PlanNodeStats.h"
#include "velox/exec/Split.h"
#include "velox/expression/Expr.h"
#include "velox/functions/prestosql/aggregates/RegisterAggregateFunctions.h"
#include "velox/functions/prestosql/registration/RegistrationFunctions.h"
#include "velox/parse/Expressions.h"
#include "velox/parse/ExpressionsParser.h"
#include "velox/parse/TypeResolver.h"
#include "velox/tpch/gen/TpchGen.h"
#include "velox/vector/tests/utils/VectorTestBase.h"

namespace facebook::velox::py {
using namespace velox;
using namespace facebook::velox;
using namespace facebook::velox::test;
using namespace facebook::velox::exec::test;
namespace py = pybind11;

//Project code

class ProjectInterface : public VectorTestBase{
  public:
  
  //setup
  int maxDrivers = 1;
  std::shared_ptr<facebook::velox::core::PlanNodeIdGenerator> planNodeIdGenerator = 
    std::make_shared<facebook::velox::core::PlanNodeIdGenerator>();

  //data
  std::vector<facebook::velox::RowVectorPtr> data; 
  //

  //mutations
  std::vector<std::unordered_set<std::string>> modifsCall;

  std::map<std::string, int> constantDistribution;
  std::map<std::string, int> constantMin;
  std::map<std::string, int> constantMax;
  std::map<std::string, double> constantStddev;

  void init(){
    // Register Presto scalar functions.
    functions::prestosql::registerAllScalarFunctions();

    // Register Presto aggregate functions.
    aggregate::prestosql::registerAllAggregateFunctions();

    // Register type resolver with DuckDB SQL parser.
    parse::registerTypeResolver();
  }

  core::TypedExprPtr parseExpression(
      const std::string& text,
      const RowTypePtr& rowType) {
    parse::ParseOptions options;
    auto untyped = parse::parseExpr(text, options);
    core::TypedExprPtr ret = core::Expressions::inferTypes(untyped, rowType, pool_.get());
    return ret;
  }

  std::vector<core::TypedExprPtr> compileQueriesFromFile(std::string queriesLocation){
    std::vector<std::string> filters;

    std::vector<std::string> row;
    std::string line, word;
    std::vector<std::vector<std::string>> queries;
    std::fstream queryFile (queriesLocation, std::ios::in);
    if(queryFile.is_open())
    {
      while(getline(queryFile, line))
      {
      row.clear();

      std::stringstream str(line);
      
      while(getline(str, word, ' '))
        row.push_back(word);
      queries.push_back(row);
      }
    }
    else
      std::cout<<"Could not open the file\n";
    queryFile.close();

    for(int i = 0; i < queries.size(); i++){
      std::string filter = "";
      for(int j = 0; j < queries[i].size(); j++){
        if(j >= 5){
          filter += " " + queries[i][j];
        }
      }

      if(queries[i].size() < 5) {
        filter = "TRUE";
      }

      filters.push_back(filter);
    }

    return compileQueries(filters);
  }

  std::vector<core::TypedExprPtr> compileSQLQueries(std::vector<std::string> queries){
    std::vector<std::string> filters;

    for(int i = 0; i < queries.size(); i++){
      std::string filter = "";
      std::vector<std::string> query;
      std::istringstream iss(queries[i]);
      for (std::string s; iss >> s; )
        query.push_back(s);
      for(int j = 0; j < query.size(); j++){
        if(j >= 5){
          filter += " " + query[j];
        }
      }

      if(query.size() < 5) {
        filter = "TRUE";
      }

      filters.push_back(filter);
    }

    return compileQueries(filters);
  }

  std::vector<core::TypedExprPtr> compileFilters(std::vector<std::string> input){
    std::vector<std::string> filters;

    for(int i = 0; i < input.size(); i++){
      filters.push_back(input[i]);
    }

    return compileQueries(filters);
  }

  void loadData(std::string dataLocation){
    std::vector<std::vector<std::string>> rawData;
    std::vector<std::string> labels;
    std::vector<std::string> row;
    std::string line, word;
    std::fstream file (dataLocation, std::ios::in);
    if(file.is_open())
    {
      while(getline(file, line, '\n'))
      {
      std::vector<std::string> row;
      row.clear();

      std::stringstream str(line);
      
      int i =0;
      while(getline(str, word, ',')){
        if (i == 13) {
          word = word.substr(0, word.length());
        }
        row.push_back(word);
        i++;
      }
      rawData.push_back(row);
      }
    }
    else
      std::cout<<"Could not open the file\n";
    file.close();

    //transform content matrix into column matrix and convert types
    std::vector<std::vector<std::int64_t>> columns;
    std::vector<std::int64_t> column;
    for(int j=0; j<rawData[0].size(); j++)
    {
      for(int i=1; i<rawData.size(); i++)
      {
          column.push_back(std::stoi(rawData[i][j]));
      }
      columns.push_back(column);
      column.clear();
    }

    //push data to velox
    std::vector<std::shared_ptr<facebook::velox::BaseVector>> vectorColumns;

    for(int i=0; i<columns.size(); i++)
    {
      vectorColumns.push_back(makeFlatVector<int64_t>(columns[i]));
    }

    rawData[0][rawData[0].size() - 1] = rawData[0][rawData[0].size() - 1].substr(0,  rawData[0][rawData[0].size() - 1].length() - 1);
    labels = rawData[0];
    auto db = makeRowVector(rawData[0], vectorColumns);
    data.push_back(db);
  }

  void loadDataActualTypes(std::string dataLocation){
    std::vector<std::vector<std::string>> rawData;
    std::vector<std::string> labels;
    std::vector<std::string> row;
    std::string line, word;
    std::fstream file (dataLocation, std::ios::in);
    if(file.is_open())
    {
      while(getline(file, line, '\n'))
      {
      std::vector<std::string> row;
      row.clear();

      std::stringstream str(line);
      
      int i =0;
      while(getline(str, word, ',')){
        if (i == 13) {
          word = word.substr(0, word.length() - 1);
        }
        row.push_back(word);
        i++;
      }
      rawData.push_back(row);
      }
    }
    else
      std::cout<<"Could not open the file\n";
    file.close();

    //transform content matrix into column matrix and convert types
    std::vector<std::int64_t> idCol;
    std::vector<std::vector<std::int8_t>> columns;
    std::vector<std::int8_t> column;
    for(int i = 1; i<rawData.size(); i++) {
      idCol.push_back(std::stoi(rawData[i][0]));
    }

    for(int j=1; j<rawData[0].size(); j++)
    {
      for(int i=1; i<rawData.size(); i++)
      {
          column.push_back(std::stoi(rawData[i][j]));
      }
      columns.push_back(column);
      column.clear();
    }

    //push data to velox
    std::vector<std::shared_ptr<facebook::velox::BaseVector>> vectorColumns;

    vectorColumns.push_back(makeFlatVector<int64_t>(idCol));

    for(int i=0; i<columns.size(); i++)
    {
      vectorColumns.push_back(makeFlatVector<int8_t>(columns[i]));
    }

    labels = rawData[0];
    auto db = makeRowVector(rawData[0], vectorColumns);
    data.push_back(db);
  }

  void setNumThreads(int numThreads) {
    executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(numThreads);
    queryCtx_ = std::make_shared<core::QueryCtx>(executor_.get());
  }

  void setMaxDrivers(int drivers) {
    maxDrivers = drivers;
  }

  std::vector<core::TypedExprPtr> compileQueries(std::vector<std::string> filters){
    std::vector<core::TypedExprPtr> ret;

    for(int i = 0; i < filters.size(); i++)
      ret.push_back(parseExpression(filters[i], asRowType(data[0]->type())));

    return ret;
  }

  std::vector<std::vector<int64_t>> runQueries(std::vector<core::TypedExprPtr> compiledFilters, std::map<int, std::vector<int>> dbMapping){
    auto planList = PlanBuilder(planNodeIdGenerator).planList();

    std::int64_t queryId = 0;
    for(int i = 0; i < compiledFilters.size(); i++){
      std::vector<std::int64_t> queryIdV = {queryId};
      auto queryVector = makeRowVector({"queryId"}, {makeFlatVector<int64_t>({queryIdV})});

      if ( dbMapping.find(i) == dbMapping.end() ) {
        for (facebook::velox::RowVectorPtr db : data){
          planList.push_back(PlanBuilder(planNodeIdGenerator).values({db})
            .filterFromExpr(compiledFilters[i]).optionalProject({"id"}).singleAggregation(
              {},
              {"array_agg(id) AS result"})
            .crossJoin(
              PlanBuilder(planNodeIdGenerator).values({queryVector}).planNode(),
              {"result", "queryId"})
            .planNode());
          }
      } else {
        for (int dbId : dbMapping[i]){
          planList.push_back(PlanBuilder(planNodeIdGenerator).values({data[dbId]})
            .filterFromExpr(compiledFilters[i]).optionalProject({"id"}).singleAggregation(
              {},
              {"array_agg(id) AS result"})
            .crossJoin(
              PlanBuilder(planNodeIdGenerator).values({queryVector}).planNode(),
              {"result", "queryId"})
            .planNode());
        }
      }
        queryId++;
    }
    
    auto plan = PlanBuilder(planNodeIdGenerator).localPartition({}, planList);
    auto execPlan = plan.planNode();

    auto results = AssertQueryBuilder(execPlan).queryCtx(queryCtx_)
      .maxDrivers(maxDrivers).returnResults(pool());

    //postProcess result
    std::vector<std::vector<int64_t>> res(results->size());

    auto temp1= results->childAt(1);
    auto queryIds = temp1->asFlatVector<int64_t>()->rawValues();

    std::shared_ptr<facebook::velox::BaseVector> temp0 = results->childAt(0);
    auto arrayVec = temp0->as<ArrayVector>();
    auto queryResults = arrayVec->elements()->asFlatVector<int64_t>()->rawValues();

    int index = 0;
    for(int i = 0; i < results->size(); i++){
      if(arrayVec->isNullAt(i)){
        res[queryIds[i]] = {};
      } else {
        int size = arrayVec->sizeAt(i);
        std::vector<int64_t> currRes;
        currRes.assign(queryResults + index, queryResults + index + size);
        index += size;
        res[queryIds[i]] = currRes;
      }
    }

    return res;
  }

 std::vector<int64_t> runQueriesOneThread(core::TypedExprPtr compiledFilter, int dataId){
    auto plan = PlanBuilder(planNodeIdGenerator).values({data[dataId]})
      .filterFromExpr(compiledFilter).optionalProject({"id"})
      .planNode();
      
    auto result = AssertQueryBuilder(plan).queryCtx(queryCtx_)
      .maxDrivers(maxDrivers).returnResults(pool());

    std::vector<int64_t> ret;
    if(!result){
      return ret;
    }
    
    auto ids = result->childAt(0)->asFlatVector<int64_t>();
    auto rawval = ids->rawValues();
    return std::vector<int64_t>(rawval, rawval + ids->size());

  }

  //MUTATIONS
  /*modify call nodes by adding sets of interchangable types
    example: {"or", "and"}*/
  void addModifyCalls(std::unordered_set<std::string> modif) {
    modifsCall.push_back(modif);
  }

  void addModifyConstantUniform(std::string label, int min, int max) {
    constantDistribution[label] = 0;
    constantMin[label] = min;
    constantMax[label] = max;
  }

  void addModifyConstantGaussian(std::string label, int min, int max, double stddev) {
    constantDistribution[label] = 1;
    constantMin[label] = min;
    constantMax[label] = max;
    constantStddev[label] = stddev;
  }

  void resetMutations() {
    modifsCall.clear();
    constantDistribution.clear();
    constantMin.clear();
    constantMax.clear();
    constantStddev.clear();
  }

  core::TypedExprPtr modifyRandomNodes(core::TypedExprPtr expression){
    core::TypedExprPtr ret;
    std::vector<std::shared_ptr<const core::ITypedExpr>> inputs;
    std::string label = "";

    //init label if existent
    for(std::shared_ptr<const core::ITypedExpr> child : expression -> inputs()){
        if(auto fieldAccess = isFieldAccessExpr(child)){
            label = fieldAccess -> name();
        }
    }

    //modify children
    for(std::shared_ptr<const core::ITypedExpr> child : expression -> inputs()){
    if(auto constant = isConstantExpr(child)){
        if(label != "" && constantDistribution.find(label) != constantDistribution.end()) {
          if(constantDistribution[label] == 0) {
            inputs.push_back(modifyConstantUniform(constant, constantMin[label], constantMax[label]));
          }else if(constantDistribution[label] == 1) {
            inputs.push_back(modifyConstantGaussian(constant, constantMin[label], constantMax[label], constantStddev[label]));
          }
          else {
          inputs.push_back(child);
          }
        }else {
        inputs.push_back(child);
        }
    } else {
        inputs.push_back(modifyRandomNodes(child));
    }
    }

    if(auto call = isCallExpr(expression)){
      for(int i=0; i<modifsCall.size(); i++){
        if(modifsCall[i].find(call->name()) != std::end(modifsCall[i])){
          ret = modifyCall(call, inputs, modifsCall[i]);
          return ret;
        }
      }
    }
    
    return expression -> copyWInputs(inputs);
  }

  core::TypedExprPtr mutate(core::TypedExprPtr expr){
    auto result = modifyRandomNodes(expr);
    return result;
  }

  std::shared_ptr<folly::Executor> executor_{
    std::make_shared<folly::CPUThreadPoolExecutor>(
        std::thread::hardware_concurrency())};
  std::shared_ptr<core::QueryCtx> queryCtx_{
      std::make_shared<core::QueryCtx>(executor_.get())};
};

class ITypedExprModifier {
  public:
    static std::string getSQL(core::TypedExprPtr expr){
      return expr->toSQLString();
    }

    static std::string getString(core::TypedExprPtr expr){
      return expr->toString();
    }
};

void addProjInterfaceBindings(py::module& m, bool asModuleLocalDefinitions){
  using namespace facebook::velox;

  struct ProjectWrapper {
    ProjectInterface proj;
  };

  struct TypedExprWrapper {
    core::TypedExprPtr expr;
  };

  struct CallExprWrapper {
    std::shared_ptr<const core::CallTypedExpr> expr;
  };

  struct FieldAccessExprWrapper {
    std::shared_ptr<const core::FieldAccessTypedExpr> expr;
  };

  struct ConstantExprWrapper {
    std::shared_ptr<const core::ConstantTypedExpr> expr;
  };

  struct InputExprWrapper {
    std::shared_ptr<const core::InputTypedExpr> expr;
  };

  struct ConcatExprWrapper {
    std::shared_ptr<const core::ConcatTypedExpr> expr;
  };

  struct LambdaExprWrapper {
    std::shared_ptr<const core::LambdaTypedExpr> expr;
  };

  struct CastExprWrapper {
    std::shared_ptr<const core::CastTypedExpr> expr;
  };

  py::class_<TypedExprWrapper> typedExpr(
      m, "TypedExpr", py::module_local(asModuleLocalDefinitions));

  typedExpr.def(
          "getSQL",
          [](TypedExprWrapper& wexpr) { return wexpr.expr->toSQLString(); },
          "Returns the sql string of the given ITypedExpr")
      .def(
          "getString",
          [](TypedExprWrapper& wexpr) { return wexpr.expr->toString(); },
          "Returns the string of the given ITypedExpr")
      .def(
          "getInputs",
          [](TypedExprWrapper& wexpr) { 
            std::vector<TypedExprWrapper> ret;
            auto inputs = wexpr.expr->inputs();
            for(int i=0; i < inputs.size(); i++)
              ret.push_back(TypedExprWrapper{inputs[i]});
            return ret; },
          "Returns a vector of inputs")
      .def(
          "copyWInputs",
          [](TypedExprWrapper& wexpr, std::vector<TypedExprWrapper> winputs) { 
            std::vector<core::TypedExprPtr> inputs;
            for(int i=0; i<winputs.size(); i++)
              inputs.push_back(winputs[i].expr);
            return TypedExprWrapper{wexpr.expr->copyWInputs(inputs)}; },
          "Returns a copy of the node with the specified inputs")
      .def(
          "isCallExpr",
          [](TypedExprWrapper& wexpr) {return isCallExpr(wexpr.expr) != NULL;},
          "Return bool representing if the actual type is a CallExpr")
      .def(
          "toCallExpr",
          [](TypedExprWrapper& wexpr) {return CallExprWrapper{toCallExpr(wexpr.expr)};},
          "Cast to CallExpr")
      .def(
          "isFieldAccessExpr",
          [](TypedExprWrapper& wexpr) {return isFieldAccessExpr(wexpr.expr) != NULL;},
          "Return bool representing if the actual type is a FieldAccessExpr")
      .def(
          "toFieldAccessExpr",
          [](TypedExprWrapper& wexpr) {return FieldAccessExprWrapper{toFieldAccessExpr(wexpr.expr)};},
          "Cast to FieldAccessExpr")
      .def(
          "isConstantExpr",
          [](TypedExprWrapper& wexpr) {return isConstantExpr(wexpr.expr) != NULL;},
          "Return bool representing if the actual type is a ConstantExpr")
      .def(
          "toConstantExpr",
          [](TypedExprWrapper& wexpr) {return ConstantExprWrapper{toConstantExpr(wexpr.expr)};},
          "Cast to ConstantExpr")
      .def(
          "isInputExpr",
          [](TypedExprWrapper& wexpr) {return isInputExpr(wexpr.expr) != NULL;},
          "Return bool representing if the actual type is a InputExpr")
      .def(
          "toInputExpr",
          [](TypedExprWrapper& wexpr) {return InputExprWrapper{toInputExpr(wexpr.expr)};},
          "Cast to InputExpr")
      .def(
          "isConcatExpr",
          [](TypedExprWrapper& wexpr) {return isConcatExpr(wexpr.expr) != NULL;},
          "Return bool representing if the actual type is a ConcatExpr")
      .def(
          "toConcatExpr",
          [](TypedExprWrapper& wexpr) {return ConcatExprWrapper{toConcatExpr(wexpr.expr)};},
          "Cast to ConcatExpr")
      .def(
          "isLambdaExpr",
          [](TypedExprWrapper& wexpr) {return isLambdaExpr(wexpr.expr) != NULL;},
          "Return bool representing if the actual type is a LambdaExpr")
      .def(
          "toLambdaExpr",
          [](TypedExprWrapper& wexpr) {return LambdaExprWrapper{toLambdaExpr(wexpr.expr)};},
          "Cast to LambdaExpr")
      .def(
          "isCastExpr",
          [](TypedExprWrapper& wexpr) {return isCastExpr(wexpr.expr) != NULL;},
          "Return bool representing if the actual type is a CastExpr")
      .def(
          "toCastExpr",
          [](TypedExprWrapper& wexpr) {return CastExprWrapper{toCastExpr(wexpr.expr)};},
          "Cast to CastExpr");

  
  py::class_<CallExprWrapper>(
      m, "CallExpr", typedExpr, py::module_local(asModuleLocalDefinitions))
      .def(
        "getName",
        [](CallExprWrapper& wexpr){return wexpr.expr->name();},
        "Returns the name of the call expr"
      )
      .def(
        "mutate",
        [](CallExprWrapper& wexpr, std::string name, std::vector<TypedExprWrapper> winputs) { 
          std::vector<core::TypedExprPtr> inputs;
          for(int i=0; i<winputs.size(); i++)
            inputs.push_back(winputs[i].expr);
          return CallExprWrapper{std::make_shared<core::CallTypedExpr>(
            wexpr.expr->type(), inputs, name)};
          },
        "Returns a modified copy of the node");

  py::class_<FieldAccessExprWrapper>(
      m, "FieldAccessExpr", typedExpr, py::module_local(asModuleLocalDefinitions))
      .def(
        "getName",
        [](FieldAccessExprWrapper& wexpr) { return wexpr.expr->name();},
        "Returns the name of the field accessed")
      .def(
        "mutate",
        [](FieldAccessExprWrapper& wexpr, std::string name) { 
          return FieldAccessExprWrapper{
            std::make_shared<core::FieldAccessTypedExpr>(wexpr.expr->type(), name)};
          },
        "Returns a modified copy of the field access");
  
  py::class_<ConstantExprWrapper>(
      m, "ConstantExpr", typedExpr, py::module_local(asModuleLocalDefinitions))
      .def(
        "getValue",
        [](ConstantExprWrapper& wexpr) { return wexpr.expr->value().value<int64_t>();},
        "Returns the value of the node")
      .def(
        "mutate",
        [](ConstantExprWrapper& wexpr, std::int64_t val) { 
          return ConstantExprWrapper{std::make_shared<core::ConstantTypedExpr>(wexpr.expr->type(), val)};
          },
        "Returns a modified copy of the node");

  py::class_<InputExprWrapper>(
      m, "InputExpr", typedExpr, py::module_local(asModuleLocalDefinitions));
  
  py::class_<ConcatExprWrapper>(
      m, "ConcatExpr", typedExpr, py::module_local(asModuleLocalDefinitions));

  py::class_<LambdaExprWrapper>(
      m, "LambdaExpr", typedExpr, py::module_local(asModuleLocalDefinitions));

  py::class_<CastExprWrapper>(
      m, "CastExpr", typedExpr, py::module_local(asModuleLocalDefinitions));

  py::class_<ProjectWrapper>(
      m, "Project", py::module_local(asModuleLocalDefinitions))
      .def(
          "compileQueriesFromFile",
          [](ProjectWrapper& wproj, std::string& str) { 
            auto wrap = [](core::TypedExprPtr expr) {
              return TypedExprWrapper{expr};
            };
            std::vector<TypedExprWrapper> res;
            auto list = wproj.proj.compileQueriesFromFile(str); 
            std::transform(list.begin(), list.end(), std::back_inserter(res), wrap);
            return res;
            },
          "Loads the queries")
      .def(
          "compileSQLQueries",
          [](ProjectWrapper& wproj, std::vector<std::string> queries) { 
            auto wrap = [](core::TypedExprPtr expr) {
              return TypedExprWrapper{expr};
            };
            std::vector<TypedExprWrapper> res;
            auto list = wproj.proj.compileSQLQueries(queries); 
            std::transform(list.begin(), list.end(), std::back_inserter(res), wrap);
            return res;
            },
          "Loads the queries")
      .def(
          "compileFilters",
          [](ProjectWrapper& wproj, std::vector<std::string> filters) { 
            auto wrap = [](core::TypedExprPtr expr) {
              return TypedExprWrapper{expr};
            };
            std::vector<TypedExprWrapper> res;
            auto list = wproj.proj.compileFilters(filters); 
            std::transform(list.begin(), list.end(), std::back_inserter(res), wrap);
            return res;
            },
          "Loads the queries")
      .def(
          "loadData",
          [](ProjectWrapper& wproj, std::string& str) { wproj.proj.loadData(str); },
          "Loads the data")
      .def(
          "loadDataActualTypes",
          [](ProjectWrapper& wproj, std::string& str) { wproj.proj.loadDataActualTypes(str); },
          "Loads the data")
      .def(
          "setNumThreads",
          [](ProjectWrapper& wproj, int numThreads) {wproj.proj.setNumThreads(numThreads);},
          "Sets the number of threads"
      )
      .def(
          "setMaxDrivers",
          [](ProjectWrapper& wproj, int numDrivers) {wproj.proj.setMaxDrivers(numDrivers);},
          "Sets the number of maxDrivers"
      )
      .def(
          "runQueries",
          [](ProjectWrapper& wproj, std::vector<TypedExprWrapper> wexprs, std::map<int, std::vector<int>> dbMapping) { 
            auto unwrap = [](TypedExprWrapper wexpr) {
              return wexpr.expr;
            };
            std::vector<core::TypedExprPtr> exprs;
            std::transform(wexprs.begin(), wexprs.end(), std::back_inserter(exprs), unwrap);
            return wproj.proj.runQueries(exprs, dbMapping); 
            },
          "Runs the queries")
      .def(
          "runQueryOneThread",
          [](ProjectWrapper& wproj, TypedExprWrapper wexpr, int queryId) { 
            return wproj.proj.runQueriesOneThread(wexpr.expr, queryId); 
            },
          "Runs the queries")
      .def(
          "addModifyCalls",
          [](ProjectWrapper& wproj, std::unordered_set<std::string> modif) {wproj.proj.addModifyCalls(modif);},
          "Adds list of modification")
      .def(
          "addModifyConstantUniform",
          [](ProjectWrapper& wproj, std::string label, int min, int max) {wproj.proj.addModifyConstantUniform(label, min, max);},
          "Adds lmodification for constants with a uniform distribution")
      .def(
          "addModifyConstantGaussian",
          [](ProjectWrapper& wproj, std::string label, int min, int max, double stddev) {wproj.proj.addModifyConstantGaussian(label, min, max, stddev);},
          "Adds lmodification for constants with a normal distribution")
      .def(
          "resetMutations",
          [](ProjectWrapper& wproj) {wproj.proj.resetMutations();},
          "Resets setup for mutations")
      .def(
          "mutate",
          [](ProjectWrapper& wproj, TypedExprWrapper wexpr) {return TypedExprWrapper{wproj.proj.mutate(wexpr.expr)};},
          "Mutate the specified loaded query")
      .def_static(
          "create",
          []() {
          srand((unsigned)time(NULL));
          ProjectInterface proj;
          proj.init();
          return ProjectWrapper{proj};
        });
}

//Project code end
static std::string serializeType(
    const std::shared_ptr<const velox::Type>& type) {
  const auto& obj = type->serialize();
  return folly::json::serialize(obj, velox::getSerializationOptions());
}

static VectorPtr evaluateExpression(
    std::shared_ptr<const facebook::velox::core::IExpr>& expr,
    std::vector<std::string> names,
    std::vector<VectorPtr>& inputs) {
  using namespace facebook::velox;
  if (names.size() != inputs.size()) {
    throw py::value_error("Must specify the same number of names as inputs");
  }
  vector_size_t numRows = inputs.empty() ? 0 : inputs[0]->size();
  std::vector<std::shared_ptr<const Type>> types;
  types.reserve(inputs.size());
  for (auto vector : inputs) {
    types.push_back(vector->type());
    if (vector->size() != numRows) {
      throw py::value_error("Inputs must have matching number of rows");
    }
  }
  auto rowType = ROW(std::move(names), std::move(types));
  memory::MemoryPool* pool = PyVeloxContext::getInstance().pool();
  RowVectorPtr rowVector = std::make_shared<RowVector>(
      pool, rowType, BufferPtr{nullptr}, numRows, inputs);
  core::TypedExprPtr typed = core::Expressions::inferTypes(expr, rowType, pool);
  exec::ExprSet set({typed}, PyVeloxContext::getInstance().execCtx());
  exec::EvalCtx evalCtx(
      PyVeloxContext::getInstance().execCtx(), &set, rowVector.get());
  SelectivityVector rows(numRows);
  std::vector<VectorPtr> result;
  set.eval(rows, evalCtx, result);
  return result[0];
}

static void addExpressionBindings(
    py::module& m,
    bool asModuleLocalDefinitions) {
  using namespace facebook::velox;
  functions::prestosql::registerAllScalarFunctions();
  parse::registerTypeResolver();

  // PyBind11's classes cannot be const, but the parse functions return const
  // shared_ptrs, so we wrap in a non-const class.
  struct IExprWrapper {
    std::shared_ptr<const core::IExpr> expr;
  };

  py::class_<IExprWrapper>(
      m, "Expression", py::module_local(asModuleLocalDefinitions))
      .def(
          "__str__",
          [](IExprWrapper& e) { return e.expr->toString(); },
          "Returns the string representation of the expression")
      .def(
          "getInputs",
          [](IExprWrapper& e) {
            const std::vector<std::shared_ptr<const core::IExpr>>& inputs =
                e.expr->getInputs();
            std::vector<IExprWrapper> wrapped_inputs;
            wrapped_inputs.resize(inputs.size());
            for (const std::shared_ptr<const core::IExpr>& input : inputs) {
              wrapped_inputs.push_back({input});
            }
            return wrapped_inputs;
          },
          "Returns a list of expressions that the inputs to this expression")
      .def(
          "evaluate",
          [](IExprWrapper& e,
             std::vector<std::string> names,
             std::vector<VectorPtr>& inputs) {
            return evaluateExpression(e.expr, names, inputs);
          },
          "Evaluates the expression for a given set of inputs. Inputs are specified with a list of names and a list of vectors, with each vector having the corresponding name")
      .def(
          "evaluate",
          [](IExprWrapper& e,
             std::unordered_map<std::string, VectorPtr> name_input_map) {
            std::vector<std::string> names;
            std::vector<VectorPtr> inputs;
            names.reserve(name_input_map.size());
            inputs.reserve(name_input_map.size());
            for (const std::pair<std::string, VectorPtr>& pair :
                 name_input_map) {
              names.push_back(pair.first);
              inputs.push_back(pair.second);
            }
            return evaluateExpression(e.expr, names, inputs);
          },
          "Evaluates the expression, taking in a map from names to input vectors")
      .def_static("from_string", [](std::string& str) {
        parse::ParseOptions opts;
        return IExprWrapper{parse::parseExpr(str, opts)};
      });
}

#ifdef CREATE_PYVELOX_MODULE
PYBIND11_MODULE(pyvelox, m) {
  m.doc() = R"pbdoc(
      PyVelox native code module
      --------------------------

      .. currentmodule:: pyvelox.pyvelox

      .. autosummary::
         :toctree: _generate

  )pbdoc";

  addVeloxBindings(m);
  addSignatureBindings(m);
  m.attr("__version__") = "dev";
}
#endif
} // namespace facebook::velox::py
