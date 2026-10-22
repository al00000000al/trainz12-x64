// Jet::ResultClass: the result codes Jet functions return by value. One int, but the class has a constructor, so
// MSVC returns it in memory (hidden pointer after `this`), as the original does. The template arguments only
// shape the mangled names: ResultClass<&Owner::results, Owner::Results> mangles as
// ?AV?$ResultClass@$1?results@Owner@Jet@@2QBUResultTable@2@BW4Results@Owner@Jet@@@Jet@@.
#pragma once

namespace Jet {

struct ResultTable;

template <const ResultTable* const* Table, class Codes>
class ResultClass {
public:
    ResultClass(int c = 0) : code(c) {}
    int code;  // 0 = success
};

}  // namespace Jet
