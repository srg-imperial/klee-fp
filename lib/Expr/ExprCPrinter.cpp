#include "klee/util/ExprCPrinter.h"
#include <sstream>

using namespace klee;

std::pair<ExprCPrinter::CType, unsigned> ExprCPrinter::getExprBinding(ref<Expr> e) {
  std::map<ref<Expr>, std::pair<CType, unsigned> >::iterator it = bindings.find(e);
  if (it != bindings.end())
    return it->second;

  return bindExpr(e);
}

ExprCPrinter::CType ExprCPrinter::getUIntType(unsigned w) {
  if (w <= 8)
    return UInt8;
  if (w <= 16)
    return UInt16;
  if (w <= 32)
    return UInt32;
  if (w <= 64)
    return UInt64;
  assert(0 && "expr too wide");
  return UInt64;
}

ExprCPrinter::CType ExprCPrinter::getSIntType(unsigned w) {
  if (w <= 8)
    return Int8;
  if (w <= 16)
    return Int16;
  if (w <= 32)
    return Int32;
  if (w <= 64)
    return Int64;
  assert(0 && "expr too wide");
  return Int64;
}

ExprCPrinter::CType ExprCPrinter::getFloatType(unsigned w) {
  if (w == 32)
    return Float;
  if (w == 64)
    return Double;
  assert(0 && "bad width for floating expr");
  return Double;
}

void ExprCPrinter::printExpr(std::ostream &out, CType &ty, ref<Expr> e) {
  Expr &ep = *e.get();

  switch (ep.getKind()) {
    case Expr::NotOptimized: printNotOptimized(out, ty, static_cast<NotOptimizedExpr&>(ep)); break;
    case Expr::Constant: printConstant(out, ty, static_cast<ConstantExpr&>(ep)); break;
    case Expr::Read: printRead(out, ty, static_cast<ReadExpr&>(ep)); break;
    case Expr::Select: printSelect(out, ty, static_cast<SelectExpr&>(ep)); break;
    case Expr::Concat: printConcat(out, ty, static_cast<ConcatExpr&>(ep)); break;
    case Expr::Extract: printExtract(out, ty, static_cast<ExtractExpr&>(ep)); break;
    case Expr::ZExt: printZExt(out, ty, static_cast<ZExtExpr&>(ep)); break;
    case Expr::SExt: printSExt(out, ty, static_cast<SExtExpr&>(ep)); break;
    case Expr::FPExt: printFPExt(out, ty, static_cast<FPExtExpr&>(ep)); break;
    case Expr::FPTrunc: printFPTrunc(out, ty, static_cast<FPTruncExpr&>(ep)); break;
    case Expr::UIToFP: printUIToFP(out, ty, static_cast<UIToFPExpr&>(ep)); break;
    case Expr::SIToFP: printSIToFP(out, ty, static_cast<SIToFPExpr&>(ep)); break;
    case Expr::FPToUI: printFPToUI(out, ty, static_cast<FPToUIExpr&>(ep)); break;
    case Expr::FPToSI: printFPToSI(out, ty, static_cast<FPToSIExpr&>(ep)); break;
    case Expr::FOrd1: printFOrd1(out, ty, static_cast<FOrd1Expr&>(ep)); break;
    case Expr::FSqrt: printFSqrt(out, ty, static_cast<FSqrtExpr&>(ep)); break;
    case Expr::FCos: printFCos(out, ty, static_cast<FCosExpr&>(ep)); break;
    case Expr::FSin: printFSin(out, ty, static_cast<FSinExpr&>(ep)); break;
    case Expr::Add: printAdd(out, ty, static_cast<AddExpr&>(ep)); break;
    case Expr::Sub: printSub(out, ty, static_cast<SubExpr&>(ep)); break;
    case Expr::Mul: printMul(out, ty, static_cast<MulExpr&>(ep)); break;
    case Expr::UDiv: printUDiv(out, ty, static_cast<UDivExpr&>(ep)); break;
    case Expr::SDiv: printSDiv(out, ty, static_cast<SDivExpr&>(ep)); break;
    case Expr::URem: printURem(out, ty, static_cast<URemExpr&>(ep)); break;
    case Expr::SRem: printSRem(out, ty, static_cast<SRemExpr&>(ep)); break;
    case Expr::FAdd: printFAdd(out, ty, static_cast<FAddExpr&>(ep)); break;
    case Expr::FSub: printFSub(out, ty, static_cast<FSubExpr&>(ep)); break;
    case Expr::FMul: printFMul(out, ty, static_cast<FMulExpr&>(ep)); break;
    case Expr::FDiv: printFDiv(out, ty, static_cast<FDivExpr&>(ep)); break;
    case Expr::FRem: printFRem(out, ty, static_cast<FRemExpr&>(ep)); break;
    case Expr::Not: printNot(out, ty, static_cast<NotExpr&>(ep)); break;
    case Expr::And: printAnd(out, ty, static_cast<AndExpr&>(ep)); break;
    case Expr::Or: printOr(out, ty, static_cast<OrExpr&>(ep)); break;
    case Expr::Xor: printXor(out, ty, static_cast<XorExpr&>(ep)); break;
    case Expr::Shl: printShl(out, ty, static_cast<ShlExpr&>(ep)); break;
    case Expr::LShr: printLShr(out, ty, static_cast<LShrExpr&>(ep)); break;
    case Expr::AShr: printAShr(out, ty, static_cast<AShrExpr&>(ep)); break;
    case Expr::Eq: printEq(out, ty, static_cast<EqExpr&>(ep)); break;
    case Expr::Ne: printNe(out, ty, static_cast<NeExpr&>(ep)); break;
    case Expr::Ult: printUlt(out, ty, static_cast<UltExpr&>(ep)); break;
    case Expr::Ule: printUle(out, ty, static_cast<UleExpr&>(ep)); break;
    case Expr::Ugt: printUgt(out, ty, static_cast<UgtExpr&>(ep)); break;
    case Expr::Uge: printUge(out, ty, static_cast<UgeExpr&>(ep)); break;
    case Expr::Slt: printSlt(out, ty, static_cast<SltExpr&>(ep)); break;
    case Expr::Sle: printSle(out, ty, static_cast<SleExpr&>(ep)); break;
    case Expr::Sgt: printSgt(out, ty, static_cast<SgtExpr&>(ep)); break;
    case Expr::Sge: printSge(out, ty, static_cast<SgeExpr&>(ep)); break;
    case Expr::FCmp: printFCmp(out, ty, static_cast<FCmpExpr&>(ep)); break;
    case Expr::InvalidKind: ;
  }
  assert(0 && "invalid expression kind");
}

void ExprCPrinter::printNotOptimized(std::ostream &out, CType &ty, NotOptimizedExpr &e) {
  printExpr(out, ty, e.src);
}

void ExprCPrinter::printConstant(std::ostream &out, CType &ty, ConstantExpr &e) {
  ty = getUIntType(e.getWidth());
  printConstantExpr(out, ty, &e);
}

void ExprCPrinter::printRead(std::ostream &out, CType &ty, ReadExpr &e) {
  // TODO: use the update list
  const std::string &arrName = e.updates.root->name;
  parmDecls.insert(arrName);

  out << arrName << "[";
  printUIntSubExpr(out, e.index);
  out << "]";

  ty = UInt8;
}

void ExprCPrinter::printSelect(std::ostream &out, CType &ty, SelectExpr &e) {}
void ExprCPrinter::printConcat(std::ostream &out, CType &ty, ConcatExpr &e) {}
void ExprCPrinter::printExtract(std::ostream &out, CType &ty, ExtractExpr &e) {}
void ExprCPrinter::printZExt(std::ostream &out, CType &ty, ZExtExpr &e) {}
void ExprCPrinter::printSExt(std::ostream &out, CType &ty, SExtExpr &e) {}
void ExprCPrinter::printFPExt(std::ostream &out, CType &ty, FPExtExpr &e) {}
void ExprCPrinter::printFPTrunc(std::ostream &out, CType &ty, FPTruncExpr &e) {}
void ExprCPrinter::printUIToFP(std::ostream &out, CType &ty, UIToFPExpr &e) {}
void ExprCPrinter::printSIToFP(std::ostream &out, CType &ty, SIToFPExpr &e) {}
void ExprCPrinter::printFPToUI(std::ostream &out, CType &ty, FPToUIExpr &e) {}
void ExprCPrinter::printFPToSI(std::ostream &out, CType &ty, FPToSIExpr &e) {}
void ExprCPrinter::printFOrd1(std::ostream &out, CType &ty, FOrd1Expr &e) {}
void ExprCPrinter::printFSqrt(std::ostream &out, CType &ty, FSqrtExpr &e) {}
void ExprCPrinter::printFCos(std::ostream &out, CType &ty, FCosExpr &e) {}
void ExprCPrinter::printFSin(std::ostream &out, CType &ty, FSinExpr &e) {}
void ExprCPrinter::printAdd(std::ostream &out, CType &ty, AddExpr &e) {}
void ExprCPrinter::printSub(std::ostream &out, CType &ty, SubExpr &e) {}
void ExprCPrinter::printMul(std::ostream &out, CType &ty, MulExpr &e) {}
void ExprCPrinter::printUDiv(std::ostream &out, CType &ty, UDivExpr &e) {}
void ExprCPrinter::printSDiv(std::ostream &out, CType &ty, SDivExpr &e) {}
void ExprCPrinter::printURem(std::ostream &out, CType &ty, URemExpr &e) {}
void ExprCPrinter::printSRem(std::ostream &out, CType &ty, SRemExpr &e) {}
void ExprCPrinter::printFAdd(std::ostream &out, CType &ty, FAddExpr &e) {}
void ExprCPrinter::printFSub(std::ostream &out, CType &ty, FSubExpr &e) {}
void ExprCPrinter::printFMul(std::ostream &out, CType &ty, FMulExpr &e) {}
void ExprCPrinter::printFDiv(std::ostream &out, CType &ty, FDivExpr &e) {}
void ExprCPrinter::printFRem(std::ostream &out, CType &ty, FRemExpr &e) {}
void ExprCPrinter::printNot(std::ostream &out, CType &ty, NotExpr &e) {}
void ExprCPrinter::printAnd(std::ostream &out, CType &ty, AndExpr &e) {}
void ExprCPrinter::printOr(std::ostream &out, CType &ty, OrExpr &e) {}
void ExprCPrinter::printXor(std::ostream &out, CType &ty, XorExpr &e) {}
void ExprCPrinter::printShl(std::ostream &out, CType &ty, ShlExpr &e) {}
void ExprCPrinter::printLShr(std::ostream &out, CType &ty, LShrExpr &e) {}
void ExprCPrinter::printAShr(std::ostream &out, CType &ty, AShrExpr &e) {}
void ExprCPrinter::printEq(std::ostream &out, CType &ty, EqExpr &e) {}
void ExprCPrinter::printNe(std::ostream &out, CType &ty, NeExpr &e) {}
void ExprCPrinter::printUlt(std::ostream &out, CType &ty, UltExpr &e) {}
void ExprCPrinter::printUle(std::ostream &out, CType &ty, UleExpr &e) {}
void ExprCPrinter::printUgt(std::ostream &out, CType &ty, UgtExpr &e) {}
void ExprCPrinter::printUge(std::ostream &out, CType &ty, UgeExpr &e) {}
void ExprCPrinter::printSlt(std::ostream &out, CType &ty, SltExpr &e) {}
void ExprCPrinter::printSle(std::ostream &out, CType &ty, SleExpr &e) {}
void ExprCPrinter::printSgt(std::ostream &out, CType &ty, SgtExpr &e) {}
void ExprCPrinter::printSge(std::ostream &out, CType &ty, SgeExpr &e) {}
void ExprCPrinter::printFCmp(std::ostream &out, CType &ty, FCmpExpr &e) {}

void ExprCPrinter::printSubExpr(std::ostream &out, CType ty, ref<Expr> e) {
   
}

void ExprCPrinter::printUIntSubExpr(std::ostream &out, ref<Expr> e) {
  printSubExpr(out, getUIntType(e->getWidth()), e);
}

const char *ExprCPrinter::getTypeName(CType ty) {
  switch (ty) {
  case Int8:   return "int8_t";
  case Int16:  return "int16_t";
  case Int32:  return "int32_t";
  case Int64:  return "int64_t";
  case UInt8:  return "uint8_t";
  case UInt16: return "uint16_t";
  case UInt32: return "uint32_t";
  case UInt64: return "uint64_t";
  case Float:  return "float";
  case Double: return "double";
  default:     assert(0 && "invalid ctype"); return 0;
  }
}


void ExprCPrinter::printConstantExpr(std::ostream &out, CType ty,
                                     ConstantExpr *ce) {
  if (ty != Float && ty != Double)
    out << "((" << getTypeName(ty) << ")";

  switch (ty) {
  case Int8:
  case Int16:
    out << ce->getAPValue().getSExtValue(); break;
  case Int32:
    out << ce->getAPValue().getSExtValue() << "L"; break;
  case Int64:
    out << ce->getAPValue().getSExtValue() << "LL"; break;
  case UInt8:
  case UInt16:
    out << ce->getZExtValue() << "U"; break;
  case UInt32:
    out << ce->getZExtValue() << "UL"; break;
  case UInt64:
    out << ce->getZExtValue() << "ULL"; break;
  case Float:
    out << ce->getAPFloatValue(false).convertToFloat() << "f"; break;
  case Double:
    out << ce->getAPFloatValue(false).convertToDouble(); break;
  }

  if (ty != Float && ty != Double)
    out << ")";
}

std::pair<ExprCPrinter::CType, unsigned> ExprCPrinter::bindExpr(ref<Expr> e) {
  unsigned bindingNo = bindings.size();
  std::pair<CType, unsigned> &binding = bindings[e];
  binding.second = bindingNo;

  std::ostringstream out;
  out << "  binding->n" << bindingNo << " = ";
  printExpr(out, binding.first, e);
  out << ";\n";
  fnOut << out.str();

  structOut << "  " << getTypeName(binding.first) << " n" << bindingNo << ";\n";

  return binding;
}

