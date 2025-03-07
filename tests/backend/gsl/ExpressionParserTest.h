#ifndef EXPRESSIONPARSERTEST_H
#define EXPRESSIONPARSERTEST_H

#include "../../CommonTest.h"

class ExpressionParserTest : public CommonTest {
	Q_OBJECT

private Q_SLOTS:
	void testFunctionArguments1();
	void testFunctionArguments2();
	void testUniques();
	void testgreaterThan();
	void testgreaterEqualThan();
	void testlessThan();
	void testlessEqualThan();
	void testequal();
	void testifCondition();
	void testandFunction();
	void testorFunction();
	void testxorFunction();
	void testbetweenIncluded();
	void testoutsideIncluded();
	void testbetween();
	void testoutside();
	void testequalEpsilon();
	void testRoundn();
	void testSpecialFunctions();

	void testevaluateCartesian();
	void testevaluateCartesianConstExpr();
	// void testevaluateCartesianConstExpr2();

	void testIsValid();
	void testIsValidStdev();
	void testLog2();
};

#endif // EXPRESSIONPARSERTEST_H
