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

class ProjectHardcodedQueriesExperiment : public VectorTestBase {
 public:
  const std::string kTpchConnectorId = "test-tpch";

  int runningOption;

  std::string dataLocation = "adult/discrete.csv";

  int nbQueries;
  std::string queriesLocation;
  std::string outputLocation = "adult/output.txt";

  std::vector<std::vector<std::string>> queries;
  std::vector<std::vector<std::string>> rawData;
  std::vector<std::string> labels;
  facebook::velox::RowVectorPtr data;

  ProjectHardcodedQueriesExperiment() {
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

  ~ProjectHardcodedQueriesExperiment() {
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

  void loadData(){

    //Read content from file
    std::vector<std::string> row;
    std::string line, word;
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

  void compileTwoQueriesWDuckDb(std::string query1, std::string query2, std::ofstream& outputFile){

    outputFile << "compilation time when using DuckDB on both:" << std::endl;
    outputFile << "Run query 1:" << std::endl;
    auto beforecompilation = std::chrono::high_resolution_clock::now();
    auto plan = PlanBuilder().values({data});
    plan = plan.filter(query1);
    plan = plan.optionalProject({"id"});
    auto execPlan = plan.planNode();
    auto afterCompilation = std::chrono::high_resolution_clock::now();
    auto beforeExecution = std::chrono::high_resolution_clock::now();
    auto [task1, results1] = AssertQueryBuilder(execPlan).assertAndReturnResults(data);
    auto afterExecution = std::chrono::high_resolution_clock::now();
    outputFile << printPlanWithStats(*execPlan, task1->taskStats(), false) << std::endl;
    std::chrono::duration<double, std::milli> compilationTime = afterCompilation - beforecompilation;
    outputFile << "compilation time: " << compilationTime.count() << "ms" << std::endl;

    outputFile << "Run query 2:" << std::endl;
    beforecompilation = std::chrono::high_resolution_clock::now();
    plan = PlanBuilder().values({data});
    plan = plan.filter(query2);
    plan = plan.optionalProject({"id"});
    execPlan = plan.planNode();
    afterCompilation = std::chrono::high_resolution_clock::now();
    beforeExecution = std::chrono::high_resolution_clock::now();
    auto [task2, results2] = AssertQueryBuilder(execPlan).assertAndReturnResults(data);
    afterExecution = std::chrono::high_resolution_clock::now();
    outputFile << printPlanWithStats(*execPlan, task2->taskStats(), false) << std::endl;
    compilationTime = afterCompilation - beforecompilation;
    outputFile << "compilation time: " << compilationTime.count() << "ms" << std::endl;

  }

  void compileTwoQueriesWModification(std::string query1, std::string query2, std::ofstream& outputFile){

    outputFile << "Compilation time when using DuckDB only for first:" << std::endl;
    outputFile << "Run query 1:" << std::endl;
    auto beforeCompilation = std::chrono::high_resolution_clock::now();
    auto plan = PlanBuilder().values({data});
    auto [plan1, typedExpression] = plan.filterCustom(query1);
    plan = plan1.optionalProject({"id"});
    auto execPlan = plan.planNode();
    auto afterCompilation = std::chrono::high_resolution_clock::now();
    auto beforeExecution = std::chrono::high_resolution_clock::now();
    auto [task1, results1] = AssertQueryBuilder(execPlan).assertAndReturnResults(data);
    auto afterExecution = std::chrono::high_resolution_clock::now();
    outputFile << printPlanWithStats(*execPlan, task1->taskStats(), false) << std::endl;
    std::chrono::duration<double, std::milli> compilationTime = afterCompilation - beforeCompilation;
    outputFile << "compilation time: " << compilationTime.count() << "ms" << std::endl;

    outputFile << "Run query 2:" << std::endl;
    beforeCompilation = std::chrono::high_resolution_clock::now();
    auto inputs = typedExpression->inputs();
    auto newValNode = parseExpression("21", asRowType(data->type()));
    inputs[1] = newValNode;
    auto newExpr = typedExpression -> copyWInputs(inputs);
    plan = PlanBuilder().values({data});
    plan = plan.filterFromExpr(newExpr);
    plan = plan1.optionalProject({"id"});
    execPlan = plan.planNode();
    afterCompilation = std::chrono::high_resolution_clock::now();
    beforeExecution = std::chrono::high_resolution_clock::now();
    auto [task2, results2] = AssertQueryBuilder(execPlan).assertAndReturnResults(data);
    afterExecution = std::chrono::high_resolution_clock::now();
    outputFile << printPlanWithStats(*execPlan, task2->taskStats(), false) << std::endl;
    compilationTime = afterCompilation - beforeCompilation;
    outputFile << "compilation time: " << compilationTime.count() << "ms" << std::endl;
  }

  //Experiment
  void runHardcodedQueryModificationExperiment(){
    queriesLocation = "adult/queries_100.txt";
    std::string query1 = "age = 22";
    std::string query2 = "age = 21";
    loadData();
    std::ofstream outputFile(outputLocation);

    compileTwoQueriesWModification(query1, query2, outputFile);
    compileTwoQueriesWDuckDb(query1, query2, outputFile);
    outputFile.close();
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

void ProjectHardcodedQueriesExperiment::run() {
  runHardcodedQueryModificationExperiment();
}

int main(int argc, char** argv) {
  folly::init(&argc, &argv, false);

  ProjectHardcodedQueriesExperiment demo;
  demo.run();
}
