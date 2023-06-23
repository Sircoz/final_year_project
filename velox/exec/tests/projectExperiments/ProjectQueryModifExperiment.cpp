#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>
#include <map>
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

using namespace facebook::velox;
using namespace facebook::velox::test;
using namespace facebook::velox::exec::test;

class ProjectQueryModifExperiment : public VectorTestBase {
 public:
  const std::string kTpchConnectorId = "test-tpch";

  int runningOption;

  std::string dataLocation = "adult/discrete.csv";

  int nbQueries;
  std::string queriesLocation;
  std::string tempQueries = "adult/temp/queries.txt";
  std::string outputLocation = "adult/output.txt";

  std::vector<std::vector<std::string>> queries;
  std::vector<std::vector<std::string>> rawData;
  std::vector<std::string> labels;
  facebook::velox::RowVectorPtr data;

  ProjectQueryModifExperiment() {
    // Register Presto scalar functions.
    functions::prestosql::registerAllScalarFunctions();

    // Register Presto aggregate functions.
    aggregate::prestosql::registerAllAggregateFunctions();

    // Register type resolver with DuckDB SQL parser.
    parse::registerTypeResolver();

    // Register TPC-H connector.
    auto tpchConnector =
        connector::getConnectorFactory(
            connector::tpch::TpchConnectorFactory::kTpchConnectorName)
            ->newConnector(kTpchConnectorId, nullptr);
    connector::registerConnector(tpchConnector);
  }

  ~ProjectQueryModifExperiment() {
    connector::unregisterConnector(kTpchConnectorId);
  }

  /// Parse SQL expression into a typed expression tree using DuckDB SQL parser.
  core::TypedExprPtr parseExpression(
      const std::string& text,
      const RowTypePtr& rowType) {
    parse::ParseOptions options;
    auto untyped = parse::parseExpr(text, options);
    core::TypedExprPtr ret = core::Expressions::inferTypes(untyped, rowType, execCtx_->pool());
    return ret;
  }

  /// Compile typed expression tree into an executable ExprSet.
  std::unique_ptr<exec::ExprSet> compileExpression(
      const std::string& expr,
      const RowTypePtr& rowType) {

    auto beforecompilation = std::chrono::high_resolution_clock::now();
    std::vector<core::TypedExprPtr> expressions = {
        parseExpression(expr, rowType)};

    auto aftercompilation = std::chrono::high_resolution_clock::now();
    auto ret = std::make_unique<exec::ExprSet>(
        std::move(expressions), execCtx_.get());
    auto afterExprSet = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> compilationTime = aftercompilation - beforecompilation;
    std::chrono::duration<double, std::milli> ExprSetTime = afterExprSet - aftercompilation;
    std::cout << "compilation time: " << compilationTime.count() << "ms" << std::endl;
    std::cout << "ExprSet time: " << ExprSetTime.count() << "ms" << std::endl;
    return ret;
  }

  /// Evaluate an expression on one batch of data.
  VectorPtr evaluate(exec::ExprSet& exprSet, const RowVectorPtr& input) {
    exec::EvalCtx context(execCtx_.get(), &exprSet, input.get());

    SelectivityVector rows(input->size());
    std::vector<VectorPtr> result(1);
    exprSet.eval(rows, context, result);
    return result[0];
  }

  /// Make TPC-H split to add to TableScan node.
  exec::Split makeTpchSplit() const {
    return exec::Split(std::make_shared<connector::tpch::TpchConnectorSplit>(
        kTpchConnectorId));
  }

  int getLabelIndex(std::string s){
    return std::find(labels.begin(), labels.end(), s) - labels.begin();
  }

  void loadQueries(std::string location){
    queries.clear();
    std::vector<std::string> row;
    std::string line, word;
    std::fstream queryFile (location, std::ios::in);
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
  }

  void loadData(){

    //Read content from file
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

    labels = rawData[0];
    data = makeRowVector(rawData[0], vectorColumns);
  }

  //Experiments
  void modifyQueryAtRandomToObtainSimilarQueries(){
    int nbQueries;
    int nbMutations;
    std::cout << "Type the number of queries to mutate: ";
    std::cin >> nbQueries;
    std::cout << "Type the number of mutations: ";
    std::cin >> nbMutations;

    queriesLocation = "adult/queries_" + std::to_string(nbQueries) +".txt";
    std::map<std::string, int> minimumValues = {
      {"age", 20},
      {"workclass", 0},
      {"education", 0},
      {"educationnum", 0},
      {"maritalstatus", 0},
      {"race", 0},
      {"sex", 0},
      {"occupation", 0},
      {"capitalgain", 0},
      {"capitalloss", 0},
      {"hoursperweek", 0},
      {"nativecountry", 0},
    };

    std::map<std::string, int> maximumValues = {
      {"age", 60},
      {"workclass", 10},
      {"education", 20},
      {"educationnum", 20},
      {"maritalstatus", 10},
      {"race", 5},
      {"sex", 1},
      {"occupation", 10},
      {"relationship", 5},
      {"capitalgain", 100},
      {"capitalloss", 100},
      {"hoursperweek", 100},
      {"nativecountry", 100},
    };

    std::vector<double> compilationDuckDB;
    std::vector<double> compilationMutation;
    loadData();
    loadQueries(queriesLocation);
    srand((unsigned)time(NULL));

    //run ITypedExpr modification
    std::ofstream outputFile(tempQueries);
    for(int i = 0; i < queries.size(); i++){
      if(queries[i].size() < 5) {
        continue;
      }

      std::string initialQuery = "";
      for(int j = 0; j < queries[i].size(); j++){
        if(j >= 5){
          initialQuery += " " + queries[i][j];
        }
      }
      outputFile << initialQuery << std::endl;

      auto beforeCompilationInitial = std::chrono::high_resolution_clock::now();
      core::TypedExprPtr initialExpr = parseExpression(initialQuery, asRowType(data->type()));
      auto afterCompilationInitial = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double, std::milli> compilationTimeInitial =
        afterCompilationInitial - beforeCompilationInitial;
      compilationMutation.push_back(compilationTimeInitial.count());

      for(int j = 0; j < nbMutations; j++) {
        auto beforeCompilationMutation = std::chrono::high_resolution_clock::now();
        core::TypedExprPtr modifiedExpr = modifyRandomNodes(initialExpr, minimumValues, maximumValues);
        std::string mutSQL = modifiedExpr -> toSQLString();
        auto afterCompilationMutation = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> compilationTimeMutation =
          afterCompilationMutation - beforeCompilationMutation;
        compilationMutation.push_back(compilationTimeMutation.count());
        
        outputFile << mutSQL << std::endl;
      }
    }
    outputFile.close();

    //run DuckDb compilation
    loadQueries(tempQueries);
    for(int i = 0; i < queries.size(); i++){

      std::string query = "";
      for(int j = 0; j < queries[i].size(); j++){
        query += " " + queries[i][j];
      }

      auto beforeCompilationInitial = std::chrono::high_resolution_clock::now();
      core::TypedExprPtr expr = parseExpression(query, asRowType(data->type()));
      auto afterCompilationInitial = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double, std::milli> compilationTimeInitial =
        afterCompilationInitial - beforeCompilationInitial;
      compilationDuckDB.push_back(compilationTimeInitial.count());
    }

    auto compDuckDB = makeFlatVector<double>(compilationDuckDB);
    auto compMutation = makeFlatVector<double>(compilationMutation);
    auto times = makeRowVector({"compduckdb", "compmutation"}, {compDuckDB, compMutation});
    auto plan = PlanBuilder().values({times})
      .singleAggregation(
        {},
        {"avg(compduckdb) AS compduckdbmean",
        "stddev(compduckdb) AS compduckdbstd",
        "avg(compmutation) AS compmutationmean",
        "stddev(compmutation) AS compmutationstd"}).planNode();
    auto res = AssertQueryBuilder(plan).copyResults(pool());
    std::cout << "compilation DuckDB & Mutation:" << res->toString(0, res -> size()) << std::endl;
  }

  /// Run the demo.
  void run();

  std::shared_ptr<folly::Executor> executor_{
      std::make_shared<folly::CPUThreadPoolExecutor>(
          std::thread::hardware_concurrency())};
  std::shared_ptr<core::QueryCtx> queryCtx_{
      std::make_shared<core::QueryCtx>(executor_.get())};
  std::unique_ptr<core::ExecCtx> execCtx_{
      std::make_unique<core::ExecCtx>(pool_.get(), queryCtx_.get())};
};

void ProjectQueryModifExperiment::run() {
  modifyQueryAtRandomToObtainSimilarQueries();
}

int main(int argc, char** argv) {
  folly::init(&argc, &argv, false);

  ProjectQueryModifExperiment demo;
  demo.run();
}
