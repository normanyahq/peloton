/*-------------------------------------------------------------------------
 *
 * expression_util.cpp
 * file description
 *
 * Copyright(c) 2015, CMU
 *
 * /n-store/src/expression/expression_util.cpp
 *
 *-------------------------------------------------------------------------
 */


#include "expression/expression_util.h"

#include "common/value_factory.h"
#include "common/exception.h"
#include "expression/abstract_expression.h"
#include <cassert>
#include <sstream>
#include <cstdlib>
#include <stdexcept>

#include "expression.h"
#include <json_spirit.h>

namespace nstore {
namespace expression {

/** Function static helper templated functions to vivify an optimal
    comparison class. */
AbstractExpression*
getGeneral(ExpressionType c,
           AbstractExpression *l,
           AbstractExpression *r)
{
    assert (l);
    assert (r);
    switch (c) {
    case (EXPRESSION_TYPE_COMPARE_EQUAL):
        return new ComparisonExpression<CmpEq>(c, l, r);
    case (EXPRESSION_TYPE_COMPARE_NOTEQUAL):
        return new ComparisonExpression<CmpNe>(c, l, r);
    case (EXPRESSION_TYPE_COMPARE_LESSTHAN):
        return new ComparisonExpression<CmpLt>(c, l, r);
    case (EXPRESSION_TYPE_COMPARE_GREATERTHAN):
        return new ComparisonExpression<CmpGt>(c, l, r);
    case (EXPRESSION_TYPE_COMPARE_LESSTHANOREQUALTO):
        return new ComparisonExpression<CmpLte>(c, l, r);
    case (EXPRESSION_TYPE_COMPARE_GREATERTHANOREQUALTO):
        return new ComparisonExpression<CmpGte>(c, l, r);
    default:
        char message[256];
        sprintf(message, "Invalid ExpressionType '%s' called"
                " for ComparisonExpression",
                GetTypeName(c).c_str());
        throw ExpressionException(message);
    }
}


template <typename L, typename R>
AbstractExpression*
getMoreSpecialized(ExpressionType c, L* l, R* r)
{
    assert (l);
    assert (r);
    switch (c) {
    case (EXPRESSION_TYPE_COMPARE_EQUAL):
        return new InlinedComparisonExpression<CmpEq, L, R>(c, l, r);
    case (EXPRESSION_TYPE_COMPARE_NOTEQUAL):
        return new InlinedComparisonExpression<CmpNe, L, R>(c, l, r);
    case (EXPRESSION_TYPE_COMPARE_LESSTHAN):
        return new InlinedComparisonExpression<CmpLt, L, R>(c, l, r);
    case (EXPRESSION_TYPE_COMPARE_GREATERTHAN):
        return new InlinedComparisonExpression<CmpGt, L, R>(c, l, r);
    case (EXPRESSION_TYPE_COMPARE_LESSTHANOREQUALTO):
        return new InlinedComparisonExpression<CmpLte, L, R>(c, l, r);
    case (EXPRESSION_TYPE_COMPARE_GREATERTHANOREQUALTO):
        return new InlinedComparisonExpression<CmpGte, L, R>(c, l, r);
    default:
        char message[256];
        sprintf(message, "Invalid ExpressionType '%s' called for"
                " ComparisonExpression", GetTypeName(c).c_str());
        throw ExpressionException(message);
    }
}

/** convert the enumerated value type into a concrete c type for the
 * comparison helper templates. */
AbstractExpression *
comparisonFactory(ExpressionType c,
                  AbstractExpression *lc, AbstractExpression *rc)
{
    assert(lc);
    /*printf("left: %s\n", left_optimized->debug("").c_str());
    fflush(stdout);
    printf("right: %s\n", right_optimized->debug("").c_str());
    fflush(stdout);*/

    //printf("%s\n", right_optimized->debug().c_str());
    //fflush(stdout);

    //VOLT_TRACE("comparisonFactoryHelper request:%s left:%s(%p) right:%s(%p)",
    //           expressionutil::GetTypeName(c).c_str(),
    //           typeid(*(lc)).name(), lc,
    //           typeid(*(rc)).name(), rc);

    /*if (!l) {
        printf("no left\n");
        fflush(stdout);
        exit(0);
    }*/

    // more specialization available?
    ConstantValueExpression *l_const =
      dynamic_cast<ConstantValueExpression*>(lc);

    ConstantValueExpression *r_const =
      dynamic_cast<ConstantValueExpression*>(rc);

    TupleValueExpression *l_tuple =
      dynamic_cast<TupleValueExpression*>(lc);

    TupleValueExpression *r_tuple =
      dynamic_cast<TupleValueExpression*>(rc);

    // this will inline getValue(), hooray!
    if (l_const != NULL && r_const != NULL) { // CONST-CONST can it happen?
        return getMoreSpecialized<ConstantValueExpression,
            ConstantValueExpression>(c, l_const, r_const);
    } else if (l_const != NULL && r_tuple != NULL) { // CONST-TUPLE
        return getMoreSpecialized<ConstantValueExpression,
          TupleValueExpression>(c, l_const, r_tuple);
    } else if (l_tuple != NULL && r_const != NULL) { // TUPLE-CONST
        return getMoreSpecialized<TupleValueExpression,
          ConstantValueExpression >(c, l_tuple, r_const);
    } else if (l_tuple != NULL && r_tuple != NULL) { // TUPLE-TUPLE
        return getMoreSpecialized<TupleValueExpression,
          TupleValueExpression>(c, l_tuple, r_tuple);
    }

    //okay, still getTypedValue is beneficial.
    return getGeneral(c, lc, rc);
}

/** convert the enumerated value type into a concrete c type for the
 *  operator expression templated ctors */
AbstractExpression *
operatorFactory(ExpressionType et,
                AbstractExpression *lc, AbstractExpression *rc)
{
    AbstractExpression *ret = NULL;

   switch(et) {
     case (EXPRESSION_TYPE_OPERATOR_PLUS):
       ret = new OperatorExpression<OpPlus>(et, lc, rc);
       break;

     case (EXPRESSION_TYPE_OPERATOR_MINUS):
       ret = new OperatorExpression<OpMinus>(et, lc, rc);
       break;

     case (EXPRESSION_TYPE_OPERATOR_MULTIPLY):
       ret = new OperatorExpression<OpMultiply>(et, lc, rc);
       break;

     case (EXPRESSION_TYPE_OPERATOR_DIVIDE):
       ret = new OperatorExpression<OpDivide>(et, lc, rc);
       break;

     case (EXPRESSION_TYPE_OPERATOR_NOT):
       ret = new OperatorNotExpression(lc);
       break;

     case (EXPRESSION_TYPE_OPERATOR_MOD):
       throw ExpressionException("Mod operator is not yet supported.");

     case (EXPRESSION_TYPE_OPERATOR_CONCAT):
       throw ExpressionException("Concat operator not yet supported.");

     case (EXPRESSION_TYPE_OPERATOR_CAST):
       throw ExpressionException("Cast operator not yet supported.");

     default:
       throw ExpressionException("operator ctor helper out of sync");
   }
   return ret;
}

/** convert the enumerated value type into a concrete c type for
 * constant value expressions templated ctors */
AbstractExpression*
constantValueFactory(json_spirit::Object &obj,
                     __attribute__((unused)) ValueType vt, __attribute__((unused)) ExpressionType et,
                     __attribute__((unused)) AbstractExpression *lc, __attribute__((unused)) AbstractExpression *rc)
{
    // read before ctor - can then instantiate fully init'd obj.
    Value newvalue;
    json_spirit::Value valueValue = json_spirit::find_value( obj, "VALUE");
    if (valueValue == json_spirit::Value::null) {
        throw ExpressionException("constantValueFactory: Could not find"
                                      " VALUE value");
    }

    if (valueValue.type() == json_spirit::str_type)
    {
        std::string nullcheck = valueValue.get_str();
        if (nullcheck == "NULL")
        {
            newvalue = Value::GetNullValue(vt);
            return constantValueFactory(newvalue);
        }
    }

    switch (vt) {
    case VALUE_TYPE_INVALID:
        throw ExpressionException("constantValueFactory: Value type should"
                                      " never be VALUE_TYPE_INVALID");
    case VALUE_TYPE_NULL:
        throw ExpressionException("constantValueFactory: And they should be"
                                      " never be this either! VALUE_TYPE_NULL");
    case VALUE_TYPE_TINYINT:
        newvalue = ValueFactory::GetTinyIntValue(static_cast<int8_t>(valueValue.get_int64()));
        break;
    case VALUE_TYPE_SMALLINT:
        newvalue = ValueFactory::GetSmallIntValue(static_cast<int16_t>(valueValue.get_int64()));
        break;
    case VALUE_TYPE_INTEGER:
        newvalue = ValueFactory::GetIntegerValue(static_cast<int32_t>(valueValue.get_int64()));
        break;
    case VALUE_TYPE_BIGINT:
        newvalue = ValueFactory::GetBigIntValue(static_cast<int64_t>(valueValue.get_int64()));
        break;
    case VALUE_TYPE_DOUBLE:
        newvalue = ValueFactory::GetDoubleValue(static_cast<double>(valueValue.get_real()));
        break;
    case VALUE_TYPE_VARCHAR:
        newvalue = ValueFactory::GetStringValue(valueValue.get_str());
        break;
    case VALUE_TYPE_VARBINARY:
    // uses hex encoding
    newvalue = ValueFactory::GetBinaryValue(valueValue.get_str());
    break;
    case VALUE_TYPE_TIMESTAMP:
        newvalue = ValueFactory::GetTimestampValue(static_cast<int64_t>(valueValue.get_int64()));
        break;
    case VALUE_TYPE_DECIMAL:
        newvalue = ValueFactory::GetDecimalValueFromString(valueValue.get_str());
        break;
    default:
        throw ExpressionException("constantValueFactory: Unrecognized value"
                                      " type");
    }

    return constantValueFactory(newvalue);
}

/** provide an interface for creating constant value expressions that
 * is more useful to testcases */
AbstractExpression *
constantValueFactory(const Value &newvalue)
{
    return new ConstantValueExpression(newvalue);
 /*   switch (vt) {
        case (VALUE_TYPE_TINYINT):
        case (VALUE_TYPE_SMALLINT):
        case (VALUE_TYPE_INTEGER):
        case (VALUE_TYPE_TIMESTAMP):
        case (VALUE_TYPE_BIGINT):
            return new ConstantValueExpression<int64_t>(newvalue);
        case (VALUE_TYPE_DOUBLE):
            return new OptimizedConstantValueExpression<double>(newvalue);
        case (VALUE_TYPE_VARCHAR):
            return new OptimizedConstantValueExpression<VoltString>(newvalue);
        case (VALUE_TYPE_DECIMAL):
            return new OptimizedConstantValueExpression<VoltDecimal>(newvalue);
        default:
            VOLT_ERROR("unknown type '%d'", vt);
            assert (!"unknown type in constantValueFactory");
            return NULL;
    }*/
}

/** convert the enumerated value type into a concrete c type for
 * parameter value expression templated ctors */
AbstractExpression*
parameterValueFactory(json_spirit::Object &obj,
                      __attribute__((unused)) ExpressionType et,
                      __attribute__((unused)) AbstractExpression *lc, __attribute__((unused)) AbstractExpression *rc)
{
    // read before ctor - can then instantiate fully init'd obj.
    json_spirit::Value paramIdxValue = json_spirit::find_value( obj, "PARAM_IDX");
    if (paramIdxValue == json_spirit::Value::null) {
        throw ExpressionException("parameterValueFactory: Could not find"
                                      " PARAM_IDX value");
    }
    int param_idx = paramIdxValue.get_int();
    assert (param_idx >= 0);
    return parameterValueFactory(param_idx);
}

AbstractExpression * parameterValueFactory(int idx) {
    return new ParameterValueExpression(idx);
}

/** convert the enumerated value type into a concrete c type for
 * tuple value expression templated ctors */
AbstractExpression*
tupleValueFactory(json_spirit::Object &obj, __attribute__((unused)) ExpressionType et,
                  __attribute__((unused)) AbstractExpression *lc, __attribute__((unused)) AbstractExpression *rc)
{
    // read the tuple value expression specific data
    json_spirit::Value valueIdxValue =
      json_spirit::find_value( obj, "COLUMN_IDX");

    json_spirit::Value tableName =
      json_spirit::find_value(obj, "TABLE_NAME");

    json_spirit::Value columnName =
      json_spirit::find_value(obj, "COLUMN_NAME");

    // verify input
    if (valueIdxValue == json_spirit::Value::null) {
        throw ExpressionException("tupleValueFactory: Could not find"
                                      " COLUMN_IDX value");
    }
    if (valueIdxValue.get_int() < 0) {
        throw ExpressionException("tupleValueFactory: invalid column_idx.");
    }

    if (tableName == json_spirit::Value::null) {
        throw ExpressionException("tupleValueFactory: no table name in TVE");
    }

    if (columnName == json_spirit::Value::null) {
        throw ExpressionException("tupleValueFactory: no column name in"
                                      " TVE");
    }


    return new TupleValueExpression(valueIdxValue.get_int(),
                                    tableName.get_str(),
                                    columnName.get_str());
}

AbstractExpression *
conjunctionFactory(ExpressionType et, AbstractExpression *lc, AbstractExpression *rc)
{
    switch (et) {
    case (EXPRESSION_TYPE_CONJUNCTION_AND):
        return new ConjunctionExpression<ConjunctionAnd>(et, lc, rc);
    case (EXPRESSION_TYPE_CONJUNCTION_OR):
        return new ConjunctionExpression<ConjunctionOr>(et, lc, rc);
    default:
        return NULL;
    }

}


/** Given an expression type and a valuetype, find the best
 * templated ctor to invoke. Several helpers, above, aid in this
 * pursuit. Each instantiated expression must consume any
 * class-specific serialization from serialize_io. */

AbstractExpression*
expressionFactory(json_spirit::Object &obj,
                  ExpressionType et, ValueType vt, __attribute__((unused)) int vs,
                  AbstractExpression* lc,
                  AbstractExpression* rc)
{
    //VOLT_TRACE("expressionFactory request: %s(%d), %s(%d), %d, left: %p"
    //           " right: %p",
    //           expressionutil::GetTypeName(et).c_str(), et,
    //           getTypeName(vt).c_str(), vt, vs,
    //           lc, rc);

    AbstractExpression *ret = NULL;

    switch (et) {

        // Operators
    case (EXPRESSION_TYPE_OPERATOR_PLUS):
    case (EXPRESSION_TYPE_OPERATOR_MINUS):
    case (EXPRESSION_TYPE_OPERATOR_MULTIPLY):
    case (EXPRESSION_TYPE_OPERATOR_DIVIDE):
    case (EXPRESSION_TYPE_OPERATOR_CONCAT):
    case (EXPRESSION_TYPE_OPERATOR_MOD):
    case (EXPRESSION_TYPE_OPERATOR_CAST):
    case (EXPRESSION_TYPE_OPERATOR_NOT):
        ret = operatorFactory(et, lc, rc);
    break;

    // Comparisons
    case (EXPRESSION_TYPE_COMPARE_EQUAL):
    case (EXPRESSION_TYPE_COMPARE_NOTEQUAL):
    case (EXPRESSION_TYPE_COMPARE_LESSTHAN):
    case (EXPRESSION_TYPE_COMPARE_GREATERTHAN):
    case (EXPRESSION_TYPE_COMPARE_LESSTHANOREQUALTO):
    case (EXPRESSION_TYPE_COMPARE_GREATERTHANOREQUALTO):
    case (EXPRESSION_TYPE_COMPARE_LIKE):
        ret = comparisonFactory( et, lc, rc);
    break;

    // Conjunctions
    case (EXPRESSION_TYPE_CONJUNCTION_AND):
    case (EXPRESSION_TYPE_CONJUNCTION_OR):
        ret = conjunctionFactory(et, lc, rc);
    break;

    // Constant Values, parameters, tuples
    case (EXPRESSION_TYPE_VALUE_CONSTANT):
        ret = constantValueFactory(obj, vt, et, lc, rc);
        break;

    case (EXPRESSION_TYPE_VALUE_PARAMETER):
        ret = parameterValueFactory(obj, et, lc, rc);
        break;

    case (EXPRESSION_TYPE_VALUE_TUPLE):
        ret = tupleValueFactory(obj, et, lc, rc);
        break;

    case (EXPRESSION_TYPE_VALUE_TUPLE_ADDRESS):
        ret = new TupleAddressExpression();
        break;

        // must handle all known expressions in this factory
    default:
        char message[256];
        sprintf(message, "Invalid ExpressionType '%s' requested from factory",
                GetTypeName(et).c_str());
        throw ExpressionException(message);
    }

    // written thusly to ease testing/inspecting return content.
    //VOLT_TRACE("Created '%s' expression %p", expressionutil::GetTypeName(et).c_str(), ret);
    return ret;
}

boost::shared_array<int>
convertIfAllTupleValues(const std::vector<AbstractExpression*> &expressions)
{
    size_t cnt = expressions.size();
    boost::shared_array<int> ret(new int[cnt]);
    for (uint32_t i = 0; i < cnt; ++i) {
        TupleValueExpressionMarker* casted=
          dynamic_cast<TupleValueExpressionMarker*>(expressions[i]);
        if (casted == NULL) {
            return boost::shared_array<int>();
        }
        ret[i] = casted->getColumnId();
    }
    return ret;
}

boost::shared_array<int>
convertIfAllParameterValues(const std::vector<AbstractExpression*> &expressions)
{
    size_t cnt = expressions.size();
    boost::shared_array<int> ret(new int[cnt]);
    for (uint32_t i = 0; i < cnt; ++i) {
        ParameterValueExpressionMarker *casted =
          dynamic_cast<ParameterValueExpressionMarker*>(expressions[i]);
        if (casted == NULL) {
            return boost::shared_array<int>();
        }
        ret[i] = casted->getParameterId();
    }
    return ret;
}


/** return a descriptive string for each typename. could just
    as easily be a lookup table */
std::string
getTypeName(ExpressionType type)
{
    std::string ret;
    switch (type) {
        case (EXPRESSION_TYPE_OPERATOR_PLUS):
            ret = "OPERATOR_PLUS";
            break;
        case (EXPRESSION_TYPE_OPERATOR_MINUS):
            ret = "OPERATOR_MINUS";
            break;
        case (EXPRESSION_TYPE_OPERATOR_MULTIPLY):
            ret = "OPERATOR_MULTIPLY";
            break;
        case (EXPRESSION_TYPE_OPERATOR_DIVIDE):
            ret = "OPERATOR_DIVIDE";
            break;
        case (EXPRESSION_TYPE_OPERATOR_CONCAT):
            ret = "OPERATOR_CONCAT";
            break;
        case (EXPRESSION_TYPE_OPERATOR_MOD):
            ret = "OPERATOR_MOD";
            break;
        case (EXPRESSION_TYPE_OPERATOR_CAST):
            ret = "OPERATOR_CAST";
            break;
        case (EXPRESSION_TYPE_OPERATOR_NOT):
            ret = "OPERATOR_NOT";
            break;
        case (EXPRESSION_TYPE_COMPARE_EQUAL):
            ret = "COMPARE_EQUAL";
            break;
        case (EXPRESSION_TYPE_COMPARE_NOTEQUAL):
            ret = "COMPARE_NOTEQUAL";
            break;
        case (EXPRESSION_TYPE_COMPARE_LESSTHAN):
            ret = "COMPARE_LESSTHAN";
            break;
        case (EXPRESSION_TYPE_COMPARE_GREATERTHAN):
            ret = "COMPARE_GREATERTHAN";
            break;
        case (EXPRESSION_TYPE_COMPARE_LESSTHANOREQUALTO):
            ret = "COMPARE_LESSTHANOREQUALTO";
            break;
        case (EXPRESSION_TYPE_COMPARE_GREATERTHANOREQUALTO):
            ret = "COMPARE_GREATERTHANOREQUALTO";
            break;
        case (EXPRESSION_TYPE_COMPARE_LIKE):
            ret = "COMPARE_LIKE";
            break;
        case (EXPRESSION_TYPE_CONJUNCTION_AND):
            ret = "CONJUNCTION_AND";
            break;
        case (EXPRESSION_TYPE_CONJUNCTION_OR):
            ret = "CONJUNCTION_OR";
            break;
        case (EXPRESSION_TYPE_VALUE_CONSTANT):
            ret = "VALUE_CONSTANT";
            break;
        case (EXPRESSION_TYPE_VALUE_PARAMETER):
            ret = "VALUE_PARAMETER";
            break;
        case (EXPRESSION_TYPE_VALUE_TUPLE):
            ret = "VALUE_TUPLE";
            break;
        case (EXPRESSION_TYPE_VALUE_TUPLE_ADDRESS):
            ret = "VALUE_TUPLE_ADDRESS";
            break;
        case (EXPRESSION_TYPE_VALUE_NULL):
            ret = "VALUE_NULL";
            break;
        case (EXPRESSION_TYPE_AGGREGATE_COUNT):
            ret = "AGGREGATE_COUNT";
            break;
        case (EXPRESSION_TYPE_AGGREGATE_COUNT_STAR):
            ret = "AGGREGATE_COUNT_STAR";
            break;
        case (EXPRESSION_TYPE_AGGREGATE_SUM):
            ret = "AGGREGATE_SUM";
            break;
        case (EXPRESSION_TYPE_AGGREGATE_MIN):
            ret = "AGGREGATE_MIN";
            break;
        case (EXPRESSION_TYPE_AGGREGATE_MAX):
            ret = "AGGREGATE_MAX";
            break;
        case (EXPRESSION_TYPE_AGGREGATE_AVG):
            ret = "AGGREGATE_AVG";
            break;
        case (EXPRESSION_TYPE_INVALID):
            ret = "INVALID";
            break;
        default: {
            char buffer[32];
            sprintf(buffer, "UNKNOWN[%d]", type);
            ret = buffer;
        }
    }
    return (ret);
}

} // End expression namespace
} // End nstore namespace


