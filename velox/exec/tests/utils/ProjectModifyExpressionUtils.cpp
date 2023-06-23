#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <thread>
#include <random>
#include <map>
#include <unordered_set>
#include <folly/init/Init.h>
#include "velox/connectors/tpch/TpchConnector.h"
#include "velox/connectors/tpch/TpchConnectorSplit.h"
#include "velox/core/Expressions.h"
#include "velox/exec/tests/utils/AssertQueryBuilder.h"
#include "velox/exec/tests/utils/PlanBuilder.h"
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
using namespace facebook::velox::connector;

namespace facebook::velox::exec::test{

    std::default_random_engine generator;

    //Casting expressions: returns NULL if cast fails
    core::CallTypedExpr const* isCallExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        core::CallTypedExpr const* ret = dynamic_cast<core::CallTypedExpr const *>(expr.get());
        return ret;
    }

    std::shared_ptr<const core::CallTypedExpr> toCallExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        std::shared_ptr<const core::CallTypedExpr> ret = std::dynamic_pointer_cast<const core::CallTypedExpr>(expr);
        return ret;
    }

    core::FieldAccessTypedExpr const* isFieldAccessExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        core::FieldAccessTypedExpr const* ret = dynamic_cast<core::FieldAccessTypedExpr const*>(expr.get());
        return ret;
    }

    std::shared_ptr<const core::FieldAccessTypedExpr> toFieldAccessExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        std::shared_ptr<const core::FieldAccessTypedExpr> ret = std::dynamic_pointer_cast<const core::FieldAccessTypedExpr>(expr);
        return ret;
    }

    core::ConstantTypedExpr const* isConstantExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        core::ConstantTypedExpr const* ret = dynamic_cast<core::ConstantTypedExpr const *>(expr.get());
        return ret;
    }

    std::shared_ptr<const core::ConstantTypedExpr> toConstantExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        std::shared_ptr<const core::ConstantTypedExpr> ret = std::dynamic_pointer_cast<const core::ConstantTypedExpr>(expr);
        return ret;
    }

    core::InputTypedExpr const* isInputExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        core::InputTypedExpr const* ret = dynamic_cast<core::InputTypedExpr const *>(expr.get());
        return ret;
    }

    std::shared_ptr<const core::InputTypedExpr> toInputExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        std::shared_ptr<const core::InputTypedExpr> ret = std::dynamic_pointer_cast<const core::InputTypedExpr>(expr);
        return ret;
    }
    
    core::ConcatTypedExpr const* isConcatExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        core::ConcatTypedExpr const* ret = dynamic_cast<core::ConcatTypedExpr const *>(expr.get());
        return ret;
    }

    std::shared_ptr<const core::ConcatTypedExpr> toConcatExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        std::shared_ptr<const core::ConcatTypedExpr> ret = std::dynamic_pointer_cast<const core::ConcatTypedExpr>(expr);
        return ret;
    }

    core::LambdaTypedExpr const* isLambdaExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        core::LambdaTypedExpr const* ret = dynamic_cast<core::LambdaTypedExpr const *>(expr.get());
        return ret;
    }

    std::shared_ptr<const core::LambdaTypedExpr> toLambdaExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        std::shared_ptr<const core::LambdaTypedExpr> ret = std::dynamic_pointer_cast<const core::LambdaTypedExpr>(expr);
        return ret;
    }

    core::CastTypedExpr const* isCastExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        core::CastTypedExpr const* ret = dynamic_cast<core::CastTypedExpr const *>(expr.get());
        return ret;
    }

    std::shared_ptr<const core::CastTypedExpr> toCastExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    ){
        std::shared_ptr<const core::CastTypedExpr> ret = std::dynamic_pointer_cast<const core::CastTypedExpr>(expr);
        return ret;
    }

    //Expression creation
    std::shared_ptr<const core::CallTypedExpr> copyCallExpr(
        core::CallTypedExpr const* expr,
        std::vector<std::shared_ptr<const core::ITypedExpr>> inputs,
        std::string name
    ){
        return std::make_shared<const core::CallTypedExpr>(expr -> type(), inputs, name);
    }

    std::shared_ptr<const core::ConstantTypedExpr> mutateConstantExpr(
        core::ConstantTypedExpr const* expr,
        variant value
    ){
        return std::make_shared<const core::ConstantTypedExpr>(expr -> type(), value);
    }

    std::shared_ptr<const core::ConstantTypedExpr> copyConstantExpr(
        core::ConstantTypedExpr const* expr,
        variant value
    ){
        return std::make_shared<const core::ConstantTypedExpr>(expr -> type(), value);
    }

    //Expression modification
    std::shared_ptr<const core::ITypedExpr> modifyCall(
        core::CallTypedExpr const* call,
        std::vector<std::shared_ptr<const core::ITypedExpr>> inputs,
        std::vector<std::string> options
    ){
        int option = rand() % options.size();
        return copyCallExpr(call, inputs, options[option]);
    }

    std::shared_ptr<const core::ITypedExpr> modifyCall(
        core::CallTypedExpr const* call,
        std::vector<std::shared_ptr<const core::ITypedExpr>> inputs,
        std::unordered_set<std::string> options
    ){
        int option = rand() % options.size();
        std::string x = *std::next(options.begin(), option);
        return copyCallExpr(call, inputs, x);
    }

    std::shared_ptr<const core::ITypedExpr> modifyConstantUniform(
        core::ConstantTypedExpr const* constantExpr,
        int min,
        int max
    ){
        std::int64_t value = min + rand() % (max - min);
        return copyConstantExpr(constantExpr, value);
    }

    std::shared_ptr<const core::ITypedExpr> modifyConstantGaussian(
        core::ConstantTypedExpr const* constantExpr,
        int min,
        int max,
        double stddev
    ){
        int constantValue = constantExpr->value().value<int64_t>();
        std::normal_distribution<double> d((double)constantValue, stddev);
        std::int64_t value = d(generator);
        if(value < min)
            value = min;
        if(value > max)
            value = max;
        return copyConstantExpr(constantExpr, value);
    }

    core::TypedExprPtr modifyRandomNodes(
        core::TypedExprPtr expression,
        std::map<std::string, int> minimumValues,
        std::map<std::string, int> maximumValues
    ){
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
            int modify = rand() % 3;
            if(modify && label != "") {
            inputs.push_back(modifyConstantUniform(constant, minimumValues[label], maximumValues[label]));
            }else {
            inputs.push_back(child);
            }
        } else {
            inputs.push_back(modifyRandomNodes(child, minimumValues, maximumValues));
        }
        }

        int modify = rand() % 3;
        if(modify){
        if(auto call = isCallExpr(expression)){
            //change conjuncts
            std::vector<std::string> conj{"and", "or"};
            ret = modifyCall(call, inputs, conj);
            if(ret){
                return ret;
            }

            //change comparators
            std::vector<std::string> comp{"eq", "neq", "gte", "lte", "gt", "lt"};
            ret = modifyCall(call, inputs, comp);
            if(ret){
                return ret;
            }
        }
        }
        return expression -> copyWInputs(inputs);
    }

}