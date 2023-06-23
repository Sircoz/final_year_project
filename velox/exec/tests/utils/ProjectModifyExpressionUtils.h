#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <thread>
#include <map>
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


namespace facebook::velox::exec::test {
    //Casting expressions: returns NULL if cast fails
    core::CallTypedExpr const* isCallExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    std::shared_ptr<const core::CallTypedExpr> toCallExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );
    
    core::FieldAccessTypedExpr const* isFieldAccessExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    std::shared_ptr<const core::FieldAccessTypedExpr> toFieldAccessExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    core::ConstantTypedExpr const* isConstantExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    std::shared_ptr<const core::ConstantTypedExpr> toConstantExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    core::InputTypedExpr const* isInputExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    std::shared_ptr<const core::InputTypedExpr> toInputExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    core::ConcatTypedExpr const* isConcatExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    std::shared_ptr<const core::ConcatTypedExpr> toConcatExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    core::LambdaTypedExpr const* isLambdaExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    std::shared_ptr<const core::LambdaTypedExpr> toLambdaExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    core::CastTypedExpr const* isCastExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    std::shared_ptr<const core::CastTypedExpr> toCastExpr(
        std::shared_ptr<const core::ITypedExpr> expr
    );

    //Expression creation
    std::shared_ptr<const core::CallTypedExpr> copyCallExpr(
        core::CallTypedExpr const* expr,
        std::vector<std::shared_ptr<const core::ITypedExpr>> inputs,
        std::string name);

    std::shared_ptr<const core::ConstantTypedExpr> copyConstantExpr(
        core::ConstantTypedExpr const* expr,
        variant value);

    //Expression modification
    std::shared_ptr<const core::ITypedExpr> modifyCall(
        core::CallTypedExpr const* call,
        std::vector<std::shared_ptr<const core::ITypedExpr>> inputs,
        std::vector<std::string> options);

    std::shared_ptr<const core::ITypedExpr> modifyCall(
        core::CallTypedExpr const* call,
        std::vector<std::shared_ptr<const core::ITypedExpr>> inputs,
        std::unordered_set<std::string> options
    );

    std::shared_ptr<const core::ITypedExpr> modifyConstantUniform(
        core::ConstantTypedExpr const* constantExpr,
        int min,
        int max);

    std::shared_ptr<const core::ITypedExpr> modifyConstantGaussian(
        core::ConstantTypedExpr const* constantExpr,
        int min,
        int max,
        double stddev
    );

    core::TypedExprPtr modifyRandomNodes(
        core::TypedExprPtr expression,
        std::map<std::string, int> minimumValues,
        std::map<std::string, int> maximumValues);
}