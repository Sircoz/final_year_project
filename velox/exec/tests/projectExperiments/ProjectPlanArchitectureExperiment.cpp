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

class ProjectPlanArchitectureExperiment : public VectorTestBase {
 public:
  const std::string kTpchConnectorId = "test-tpch";

  int runningOption;
  int numThreads;
  int nbDrivers;

  std::string dataLocation = "adult/data/discrete0.0.csv";

  int nbQueries;
  std::string queriesLocation;
  std::string outputLocation = "adult/output.txt";

  std::vector<std::vector<std::string>> queries;
  std::vector<std::vector<std::string>> rawData;
  std::vector<std::string> labels;
  facebook::velox::RowVectorPtr data;

  facebook::velox::RowVectorPtr data1;
  facebook::velox::RowVectorPtr data2;
  facebook::velox::RowVectorPtr data3;
  facebook::velox::RowVectorPtr data4;

  int nbPartitions = 1;
  std::vector<facebook::velox::RowVectorPtr> dataPartitions;

  ProjectPlanArchitectureExperiment() {
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

  ~ProjectPlanArchitectureExperiment() {
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

    if(runningOption == 5)
    {
      for(int k = 0; k < nbPartitions; k++)
      {
        vectorColumns.clear();
        for(int i=0; i<columns.size(); i++)
        {
          auto start = columns[i].cbegin() + columns[i].size() * k / nbPartitions;
          auto end = columns[i].cbegin() + columns[i].size() * (k+1) / nbPartitions;
          std::vector<std::int64_t> temp(start, end);
          vectorColumns.push_back(makeFlatVector<int64_t>(temp));
        }
        dataPartitions.push_back(makeRowVector(rawData[0], vectorColumns));
      }
    }

    if(runningOption == 0)
    {
      vectorColumns.clear();
      for(int i=0; i<columns.size(); i++)
      {
        auto start = columns[i].cbegin();
        auto end = columns[i].cbegin() + columns[i].size() / 4;
        std::vector<std::int64_t> temp(start, end);
        vectorColumns.push_back(makeFlatVector<int64_t>(temp));
      }
      data1 = makeRowVector(rawData[0], vectorColumns);

      vectorColumns.clear();
      for(int i=0; i<columns.size(); i++)
      {
        auto start = columns[i].cbegin() + columns[i].size() / 4;
        auto end = columns[i].cbegin() + columns[i].size() / 2;
        std::vector<std::int64_t> temp(start, end);
        vectorColumns.push_back(makeFlatVector<int64_t>(temp));
      }
      data2 = makeRowVector(rawData[0], vectorColumns);

      vectorColumns.clear();
      for(int i=0; i<columns.size(); i++)
      {
        auto start = columns[i].cbegin() + columns[i].size() / 2;
        auto end = columns[i].cbegin() + columns[i].size() * 3 / 4;
        std::vector<std::int64_t> temp(start, end);
        vectorColumns.push_back(makeFlatVector<int64_t>(temp));
      }
      data3 = makeRowVector(rawData[0], vectorColumns);

      vectorColumns.clear();
      for(int i=0; i<columns.size(); i++)
      {
        auto start = columns[i].cbegin() + columns[i].size() * 3 / 4;
        auto end = columns[i].cbegin() + columns[i].size();
        std::vector<std::int64_t> temp(start, end);
        vectorColumns.push_back(makeFlatVector<int64_t>(temp));
      }
      data4 = makeRowVector(rawData[0], vectorColumns);

      /*std::ofstream outputFile("adult/output1.txt");
      outputFile << data -> size() << "||||" << data1-> size() << "||||" << data1->toString(0, 1)<< std::endl;
      outputFile << data -> size() << "||||" << data2-> size() << "||||" << data2->toString(0, 1)<< std::endl;
      outputFile << data -> size() << "||||" << data3-> size() << "||||" << data3->toString(0, 1)<< std::endl;
      outputFile << data -> size() << "||||" << data4-> size() << "||||" << data4->toString(0, 1)<< std::endl;
      outputFile.close();*/
    }
  }

  void setNumThreads() {
    executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(numThreads);
    queryCtx_ = std::make_shared<core::QueryCtx>(executor_.get());
  }

/*Test plan used for quick experimentation*/
  void runTestPlan(std::vector<double> & execTimes, std::vector<double> & compilationTimes) {
    std::ofstream outputFile(outputLocation);

    auto beforecompilation = std::chrono::high_resolution_clock::now();
    
    auto plan = PlanBuilder().values({data2, data1, data3, data4});
    auto execPlan = plan.planNode();
    
    auto afterCompilation = std::chrono::high_resolution_clock::now();
    auto beforeExecution = std::chrono::high_resolution_clock::now();
    auto [task, results] = AssertQueryBuilder(execPlan).assertAndReturnResults(data);
    auto afterExecution = std::chrono::high_resolution_clock::now();

    for(int j = 0; j < results.size(); j++){
      outputFile << results[j]->toString(0, results[j]->size()) << std::endl;
    }

    outputFile << printPlanWithStats(*execPlan, task->taskStats(), false) << std::endl;
    outputFile.close();
  }

/*The plan is Values->Filter. The selection of 'id' is done after the plan execution*/
  void runFilterPlan(std::vector<double> & execTimes, std::vector<double> & compilationTimes) { 

    std::ofstream outputFile(outputLocation);
    for(int i = 0; i < queries.size(); i++){
        //Gets filter options from query
        std::string filter = "";
        for(int j = 0; j < queries[i].size(); j++){
          if(j >= 5){
            filter += " " + queries[i][j];
          }
          outputFile << queries[i][j] << " ";
        }
        outputFile << "\n";
        int idIndex = getLabelIndex(queries[i][1]);

        auto beforecompilation = std::chrono::high_resolution_clock::now();

        //compile plan
        auto plan = PlanBuilder().values({data});
        if(queries[i].size() >= 5) {
          plan = plan.filter(filter);
        }
        auto execPlan = plan.planNode();

        auto beforeExecution = std::chrono::high_resolution_clock::now();

        //run plan
        auto res = AssertQueryBuilder(execPlan).maxDrivers(1).returnResults(pool());

        auto afterExecution = std::chrono::high_resolution_clock::now();

        //print times
        std::chrono::duration<double, std::milli> compilationTime = beforeExecution - beforecompilation;
        std::chrono::duration<double, std::milli> executionTime = afterExecution - beforeExecution;

        compilationTimes.push_back(compilationTime.count());
        execTimes.push_back(executionTime.count());

        outputFile << "compilation time: " << compilationTime.count() << "ms" << std::endl;
        outputFile << "Execution time: " << executionTime.count() << "ms" << std::endl;
        outputFile << res->toString() << std::endl;
        outputFile << res->toString(0, res->size()) << std::endl;
    }


    outputFile.close();
  }

/*The plan is Values->Filter->Project.*/
  void runFilterProjectPlan(std::vector<double> & execTimes, std::vector<double> & compilationTimes) { 
    std::ofstream outputFile(outputLocation);

    for(int i = 0; i < queries.size(); i++){
        std::string filter = "";
        for(int j = 0; j < queries[i].size(); j++){
          if(j >= 5){
            filter += " " + queries[i][j];
          }
          outputFile << queries[i][j] << " ";
        }
        outputFile << "\n";

        int idIndex = getLabelIndex(queries[i][1]);

        auto beforecompilation = std::chrono::high_resolution_clock::now();
        auto plan = PlanBuilder().values({data});
        if(queries[i].size() >= 5) {
          plan = plan.filter(filter);
        }
        plan = plan.optionalProject({"id"});
        auto execPlan = plan.planNode();
        auto afterCompilation = std::chrono::high_resolution_clock::now();
        auto beforeExecution = std::chrono::high_resolution_clock::now();
        auto res = AssertQueryBuilder(execPlan).queryCtx(queryCtx_).maxDrivers(1).returnResults(pool());
        auto afterExecution = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double, std::milli> compilationTime = afterCompilation - beforecompilation;
        std::chrono::duration<double, std::milli> executionTime = afterExecution - beforeExecution;
        compilationTimes.push_back(compilationTime.count());
        execTimes.push_back(executionTime.count());
        outputFile << "compilation time: " << compilationTime.count() << "ms" << std::endl;
        outputFile << "Execution time: " << executionTime.count() << "ms" << std::endl;

    }
    outputFile.close();
  }

/*The plan is Values->LocalPartition->Filter->Project. LocalPartition is done by an attribute field.*/
  void runPartitionAttributeFilterProject(std::vector<double> & execTimes, std::vector<double> & compilationTimes, std::string partAttr) { 
    std::ofstream outputFile(outputLocation);

    for(int i = 0; i < queries.size(); i++){
        std::string filter = "";
        for(int j = 0; j < queries[i].size(); j++){
          if(j >= 5){
            filter += " " + queries[i][j];
          }
          outputFile << queries[i][j] << " ";
        }
        outputFile << "\n";

        int idIndex = getLabelIndex(queries[i][1]);

        auto beforecompilation = std::chrono::high_resolution_clock::now();
        auto plan = PlanBuilder().values({data}).localPartition({partAttr});
        if(queries[i].size() >= 5) {
          plan = plan.filter(filter);
        }
        plan = plan.optionalProject({"id"});
        auto execPlan = plan.planNode();
        auto afterCompilation = std::chrono::high_resolution_clock::now();
        auto beforeExecution = std::chrono::high_resolution_clock::now();
        auto res = AssertQueryBuilder(execPlan).queryCtx(queryCtx_).maxDrivers(nbDrivers).returnResults(pool());
        auto afterExecution = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> compilationTime = afterCompilation - beforecompilation;
        std::chrono::duration<double, std::milli> executionTime = afterExecution - beforeExecution;
        compilationTimes.push_back(compilationTime.count());
        execTimes.push_back(executionTime.count());
        outputFile << "compilation time: " << compilationTime.count() << "ms" << std::endl;
        outputFile << "Execution time: " << executionTime.count() << "ms" << std::endl;

    }
    outputFile.close();
  }
  
/*The plan is Values->LocalPartition->Filter->Project. LocalPartition is done in round robin manner.*/
  void runPartitionRoundRobinPlan(std::vector<double> & execTimes, std::vector<double> & compilationTimes) { 
    std::ofstream outputFile(outputLocation);

    for(int i = 0; i < queries.size(); i++){
        std::string filter = "";
        for(int j = 0; j < queries[i].size(); j++){
          if(j >= 5){
            filter += " " + queries[i][j];
          }
          outputFile << queries[i][j] << " ";
        }
        outputFile << "\n";

        int idIndex = getLabelIndex(queries[i][1]);

        auto beforecompilation = std::chrono::high_resolution_clock::now();
        auto plan = PlanBuilder().values({data}).localPartitionRoundRobin();
        if(queries[i].size() >= 5) {
          plan = plan.filter(filter);
        }
        plan = plan.optionalProject({"id"});
        
        auto execPlan = plan.planNode();
        auto afterCompilation = std::chrono::high_resolution_clock::now();
        auto beforeExecution = std::chrono::high_resolution_clock::now();
        auto res = AssertQueryBuilder(execPlan).queryCtx(queryCtx_).maxDrivers(nbDrivers).returnResults(pool());
        auto afterExecution = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> compilationTime = afterCompilation - beforecompilation;
        std::chrono::duration<double, std::milli> executionTime = afterExecution - beforeExecution;
        compilationTimes.push_back(compilationTime.count());
        execTimes.push_back(executionTime.count());
        outputFile << "compilation time: " << compilationTime.count() << "ms" << std::endl;
        outputFile << "Execution time: " << executionTime.count() << "ms" << std::endl;

    }
    outputFile.close();
  }

/*The plan is (Values->Filter->Project)* N -> LocalPartition. Each Values node contains 1/n of the initial input data.*/
  void runPrePartitionInputPlan(std::vector<double> & execTimes, std::vector<double> & compilationTimes){
    std::ofstream outputFile(outputLocation);

    for(int i = 0; i < queries.size(); i++){
      std::string filter = "";
      for(int j = 0; j < queries[i].size(); j++){
        if(j >= 5){
          filter += " " + queries[i][j];
        }
        outputFile << queries[i][j] << " ";
      }
      outputFile << "\n";

      int idIndex = getLabelIndex(queries[i][1]);

      if(queries[i].size() < 5) {
        filter = "TRUE";
      }
      
      auto beforecompilation = std::chrono::high_resolution_clock::now();
      auto planNodeIdGenerator = std::make_shared<facebook::velox::core::PlanNodeIdGenerator>();

      auto expr = parseExpression(filter, asRowType(data->type()));;
      auto planList = PlanBuilder(planNodeIdGenerator).planList();
      for (int k = 0; k < dataPartitions.size(); k++) {
        planList.push_back(PlanBuilder(planNodeIdGenerator).values({dataPartitions[k]}).filterFromExpr(expr).optionalProject({"id"}).planNode());
      }
      auto plan = PlanBuilder(planNodeIdGenerator).localPartition({}, planList);
      auto execPlan = plan.planNode();
      auto afterCompilation = std::chrono::high_resolution_clock::now();

      auto beforeExecution = std::chrono::high_resolution_clock::now();
      auto results = AssertQueryBuilder(execPlan).queryCtx(queryCtx_).maxDrivers(1).returnResults(pool());
      auto afterExecution = std::chrono::high_resolution_clock::now();

      std::chrono::duration<double, std::milli> compilationTime = afterCompilation - beforecompilation;
      std::chrono::duration<double, std::milli> executionTime = afterExecution - beforeExecution;
      compilationTimes.push_back(compilationTime.count());
      execTimes.push_back(executionTime.count());
      outputFile << "compilation time: " << compilationTime.count() << "ms" << std::endl;
      outputFile << "Execution time: " << executionTime.count() << "ms" << std::endl;

    }
    outputFile.close();
  }

/*The plan is (Values->Filter->Project->ArrayAggregation->CrossJoin(queryId))* Queries -> LocalPartition. Each Filter node contains the filter for each query.*/
  void runBatchedQueriesPlan(std::vector<double> & execTimes, std::vector<double> & compilationTimes){
    std::ofstream outputFile(outputLocation);

    auto planNodeIdGenerator = std::make_shared<facebook::velox::core::PlanNodeIdGenerator>();
    auto planList = PlanBuilder(planNodeIdGenerator).planList();


    auto beforecompilation = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < queries.size(); i++){
      std::string filter = "";
      for(int j = 0; j < queries[i].size(); j++){
        if(j >= 5){
          filter += " " + queries[i][j];
        }
        outputFile << queries[i][j] << " ";
      }
      outputFile << "\n";

      int idIndex = getLabelIndex(queries[i][1]);

      if(queries[i].size() < 5) {
        filter = "TRUE";
      }

      std::int64_t queryId = i + 1;
      std::vector<std::int64_t> queryIdV = {queryId};
      auto queryVector = makeRowVector({"queryId"}, {makeFlatVector<int64_t>({queryIdV})});

      planList.push_back(PlanBuilder(planNodeIdGenerator).values({data})
        .filter(filter).optionalProject({"id"}).singleAggregation(
          {},
          {"array_agg(id) AS result"})
        .crossJoin(
          PlanBuilder(planNodeIdGenerator).values({queryVector}).planNode(),
          {"result", "queryId"})
        .planNode());
    }
      
    auto plan = PlanBuilder(planNodeIdGenerator).localPartition({}, planList);
    auto execPlan = plan.planNode();
    auto afterCompilation = std::chrono::high_resolution_clock::now();

    auto beforeExecution = std::chrono::high_resolution_clock::now();
    auto results = AssertQueryBuilder(execPlan)
      .queryCtx(queryCtx_)
      .maxDrivers(1).returnResults(pool());
    auto afterExecution = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> compilationTime = afterCompilation - beforecompilation;
    std::chrono::duration<double, std::milli> executionTime = afterExecution - beforeExecution;
    compilationTimes.push_back(compilationTime.count());
    execTimes.push_back(executionTime.count());
    outputFile << "compilation time: " << compilationTime.count() << "ms" << std::endl;
    outputFile << "Execution time: " << executionTime.count() << "ms" << std::endl;

    outputFile.close();
  }

  //Experiment
  void runPlanArchitectureExperiment(){
    while(true){
      std::cout << "Pick queries location: ";
      std::cin >> queriesLocation;
      std::cout << std::endl;

      std::cout << "Pick number of threads: ";
      std::cin >> numThreads;

      std::cout << "Pick the option to run:" << std::endl;
      std::cout << "TestPlan: 0" << std::endl;
      std::cout << "Filter plan then project: 1" << std::endl;
      std::cout << "FilterProject plan: 2" << std::endl;
      std::cout << "PartitionFilterProject plan: 3" << std::endl;
      std::cout << "Partition Round Robin: 4" << std::endl;
      std::cout << "PrePartition in different plans: 5" << std::endl;
      std::cout << "Partition by queries in different plans: 6" << std::endl;

      std::cin >> runningOption;
      if (runningOption == 5) 
      {
        std::cout << "Type the number of partitions: ";
        std::cin >> nbPartitions;
      }
      loadData();
      setNumThreads();
      std::vector<double> execTimes;
      std::vector<double> compilationTimes;
      if (runningOption == 0)
      {
        runTestPlan(execTimes, compilationTimes);
        return;
      } else if (runningOption == 1)
      {
        runFilterPlan(execTimes, compilationTimes);
      } else if (runningOption == 2)
      {
        runFilterProjectPlan(execTimes, compilationTimes);
      } else if (runningOption == 3)
      {
        std::cout << "Type the attrinbute to partition by: ";
        std::string partAttr;
        std::cin >> partAttr;
        std::cout << "Driver number: ";
        std::cin >> nbDrivers;
        runPartitionAttributeFilterProject(execTimes, compilationTimes, partAttr);
      } else if (runningOption == 4)
      {
        runPartitionRoundRobinPlan(execTimes, compilationTimes);
      } else if (runningOption == 5) 
      {
        runPrePartitionInputPlan(execTimes, compilationTimes);
      } else if (runningOption == 6) 
      {
        runBatchedQueriesPlan(execTimes, compilationTimes);
      } else {
        std::cout << "Option unsupported";
        return;
      }
      
      auto exec = makeFlatVector<double>(execTimes);
      auto compilation = makeFlatVector<double>(compilationTimes);
      auto times = makeRowVector({"execution", "compilation"}, {exec, compilation});
      auto plan = PlanBuilder().values({times})
        .singleAggregation(
          {},
          {"avg(execution) AS executiontimeavg",
          "stddev(execution) AS executiontimestd",
          "avg(compilation) AS compilationtimesavg",
          "stddev(compilation) AS compilationtimesstd"}).planNode();
      auto res = AssertQueryBuilder(plan).copyResults(pool());
      std::cout << res->toString(0, res->size()) << std::endl;
      return;
    }
  }

  /// Run the demo.
  void run();

  std::shared_ptr<folly::Executor> executor_{
      std::make_shared<folly::CPUThreadPoolExecutor>(1)};//std::thread::hardware_concurrency())};
  std::shared_ptr<core::QueryCtx> queryCtx_{
      std::make_shared<core::QueryCtx>(executor_.get())};
  std::unique_ptr<core::ExecCtx> execCtx_{
      std::make_unique<core::ExecCtx>(pool_.get(), queryCtx_.get())};
};

void ProjectPlanArchitectureExperiment::run() {
  runPlanArchitectureExperiment();
}

int main(int argc, char** argv) {
  folly::init(&argc, &argv, false);

  ProjectPlanArchitectureExperiment demo;
  demo.run();
  return 0;
}