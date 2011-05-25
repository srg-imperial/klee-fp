#ifndef EXPRCPRINTER_H
#define EXPRCPRINTER_H

#include <klee/Expr.h>
#include <llvm/ADT/StringSet.h>
#include <map>
#include <ostream>

namespace klee {

class ExprCPrinter {
public:
  enum CType {
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float,
    Double,

    FirstInt = Int8,
    LastInt = UInt64,
    FirstFloat = Float,
    LastFloat = Double
  };
private:
  ExprCPrinter(std::ostream &structOut, std::ostream &fnOut)
    : structOut(structOut), fnOut(fnOut) {}

  typedef std::pair<CType, unsigned> ExprBinding;
  std::map<ref<Expr>, ExprBinding> bindings;
  llvm::StringSet<> parmDecls;

  std::ostream &structOut, &fnOut;

  static CType getUIntType(unsigned w);
  static CType getSIntType(unsigned w);
  static CType getFloatType(unsigned w);

  ExprBinding getExprBinding(ref<Expr> e);
  ExprBinding bindExpr(ref<Expr> e);

  static const char *getTypeName(CType ty);

  void printExpr(std::ostream &out, CType &ty, ref<Expr> e);

  void printFUnaryMath(std::ostream &out, CType &ty, FUnaryExpr &e,
                       const char *floatName, const char *doubleName);

  void printNotOptimized(std::ostream &out, CType &ty, NotOptimizedExpr &e);
  void printConstant(std::ostream &out, CType &ty, ConstantExpr &e);
  void printRead(std::ostream &out, CType &ty, ReadExpr &e);
  void printSelect(std::ostream &out, CType &ty, SelectExpr &e);
  void printConcat(std::ostream &out, CType &ty, ConcatExpr &e);
  void printExtract(std::ostream &out, CType &ty, ExtractExpr &e);
  void printZExt(std::ostream &out, CType &ty, ZExtExpr &e);
  void printSExt(std::ostream &out, CType &ty, SExtExpr &e);
  void printFPExt(std::ostream &out, CType &ty, FPExtExpr &e);
  void printFPTrunc(std::ostream &out, CType &ty, FPTruncExpr &e);
  void printUIToFP(std::ostream &out, CType &ty, UIToFPExpr &e);
  void printSIToFP(std::ostream &out, CType &ty, SIToFPExpr &e);
  void printFPToUI(std::ostream &out, CType &ty, FPToUIExpr &e);
  void printFPToSI(std::ostream &out, CType &ty, FPToSIExpr &e);
  void printFOrd1(std::ostream &out, CType &ty, FOrd1Expr &e);
  void printFSqrt(std::ostream &out, CType &ty, FSqrtExpr &e);
  void printFCos(std::ostream &out, CType &ty, FCosExpr &e);
  void printFSin(std::ostream &out, CType &ty, FSinExpr &e);
  void printAdd(std::ostream &out, CType &ty, AddExpr &e);
  void printSub(std::ostream &out, CType &ty, SubExpr &e);
  void printMul(std::ostream &out, CType &ty, MulExpr &e);
  void printUDiv(std::ostream &out, CType &ty, UDivExpr &e);
  void printSDiv(std::ostream &out, CType &ty, SDivExpr &e);
  void printURem(std::ostream &out, CType &ty, URemExpr &e);
  void printSRem(std::ostream &out, CType &ty, SRemExpr &e);
  void printFAdd(std::ostream &out, CType &ty, FAddExpr &e);
  void printFSub(std::ostream &out, CType &ty, FSubExpr &e);
  void printFMul(std::ostream &out, CType &ty, FMulExpr &e);
  void printFDiv(std::ostream &out, CType &ty, FDivExpr &e);
  void printFRem(std::ostream &out, CType &ty, FRemExpr &e);
  void printNot(std::ostream &out, CType &ty, NotExpr &e);
  void printAnd(std::ostream &out, CType &ty, AndExpr &e);
  void printOr(std::ostream &out, CType &ty, OrExpr &e);
  void printXor(std::ostream &out, CType &ty, XorExpr &e);
  void printShl(std::ostream &out, CType &ty, ShlExpr &e);
  void printLShr(std::ostream &out, CType &ty, LShrExpr &e);
  void printAShr(std::ostream &out, CType &ty, AShrExpr &e);
  void printEq(std::ostream &out, CType &ty, EqExpr &e);
  void printNe(std::ostream &out, CType &ty, NeExpr &e);
  void printUlt(std::ostream &out, CType &ty, UltExpr &e);
  void printUle(std::ostream &out, CType &ty, UleExpr &e);
  void printUgt(std::ostream &out, CType &ty, UgtExpr &e);
  void printUge(std::ostream &out, CType &ty, UgeExpr &e);
  void printSlt(std::ostream &out, CType &ty, SltExpr &e);
  void printSle(std::ostream &out, CType &ty, SleExpr &e);
  void printSgt(std::ostream &out, CType &ty, SgtExpr &e);
  void printSge(std::ostream &out, CType &ty, SgeExpr &e);
  void printFCmp(std::ostream &out, CType &ty, FCmpExpr &e);

  void printBindingRef(std::ostream &out, unsigned binding);
  void printSubExpr(std::ostream &out, CType ty, ref<Expr> e);
  void printAnyTySubExpr(std::ostream &out, CType &ty, ref<Expr> e);

  void printUIntSubExpr(std::ostream &out, ref<Expr> e);
  void printSIntSubExpr(std::ostream &out, ref<Expr> e);
  void printZExtSubExpr(std::ostream &out, ref<Expr> e);
  void printSExtSubExpr(std::ostream &out, ref<Expr> e);
  void printFloatSubExpr(std::ostream &out, ref<Expr> e);

  void printConstantExpr(std::ostream &out, CType ty, ConstantExpr *ce);

public:
  static void printExprEvaluator(std::ostream &out, ref<Expr> e);
};

}

#endif
