/**
 * Copyright (c) 2012, Akamai Technologies
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Akamai Technologies nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "CodeGenerationContext.hh"
#include "LLVMGen.h"
#include "RecordType.hh"
#include "TypeCheckContext.hh"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

/**
 * Call a decimal binary operator.
 */
IQLToLLVMValue::ValueType 
IQLToLLVMCreateBinaryDecimalCall(CodeGenerationContext * ctxt, 
				 IQLToLLVMValueRef lhs, 
				 IQLToLLVMValueRef rhs, 
				 LLVMValueRef ret,
				 enum DecimalOpCode opCode);

IQLToLLVMValue::IQLToLLVMValue (LLVMValueRef val, 
				IQLToLLVMValue::ValueType globalOrLocal)
  :
  mValue(val),
  mIsNull(NULL),
  mValueType(globalOrLocal)
{
}
  
IQLToLLVMValue::IQLToLLVMValue (LLVMValueRef val, llvm::Value * isNull, 
				IQLToLLVMValue::ValueType globalOrLocal)
  :
  mValue(val),
  mIsNull(isNull),
  mValueType(globalOrLocal)
{
}

LLVMValueRef IQLToLLVMValue::getValue() const 
{ 
  return mValue; 
}

llvm::Value * IQLToLLVMValue::getNull() const 
{ 
  return mIsNull; 
}

void IQLToLLVMValue::setNull(llvm::Value * nv) 
{ 
  mIsNull = nv; 
}

bool IQLToLLVMValue::isLiteralNull() const 
{ 
  return mValue == NULL; 
}

IQLToLLVMValue::ValueType IQLToLLVMValue::getValueType() const 
{ 
  return mValueType; 
}

LLVMTypeRef IQLToLLVMValue::getVariableType(CodeGenerationContext * ctxt,
					    const FieldType * ft)
{
  LLVMTypeRef ty = wrap(ft->LLVMGetType(ctxt));
  if (!isValueType(ty)) {
    ty = ::LLVMPointerType(ty,0);
  }
  return ty;
}

bool IQLToLLVMValue::isValueType(LLVMTypeRef ty) 
{
  LLVMContextRef c = LLVMGetTypeContext(ty);
  return ty == LLVMInt32TypeInContext(c) ||
    ty == LLVMInt64TypeInContext(c) ||
    ty == LLVMDoubleTypeInContext(c);
}

bool IQLToLLVMValue::isPointerToValueType(LLVMTypeRef ty) 
{
  if (LLVMGetTypeKind(ty) != LLVMPointerTypeKind)
    return false;
  ty = LLVMGetElementType(ty);
  LLVMContextRef c = LLVMGetTypeContext(ty);
  return ty == LLVMInt32TypeInContext(c) ||
    ty == LLVMInt64TypeInContext(c) ||
    ty == LLVMDoubleTypeInContext(c);
}

bool IQLToLLVMValue::isValueType() const 
{
  LLVMTypeRef ty = LLVMTypeOf(mValue);
  return isValueType(ty);
}

bool IQLToLLVMValue::isPointerToValueType() const 
{
  LLVMTypeRef ty = LLVMTypeOf(mValue);
  return isPointerToValueType(ty);
}

bool IQLToLLVMValue::isReferenceType() const
{
  return !isValueType();
}

IQLToLLVMValueRef IQLToLLVMValue::get(CodeGenerationContext * ctxt, 
				      LLVMValueRef val, 
				      IQLToLLVMValue::ValueType globalOrLocal)
{
  return get(ctxt, val, NULL, globalOrLocal);
}

IQLToLLVMValueRef IQLToLLVMValue::get(CodeGenerationContext * ctxt, 
				      LLVMValueRef val,
				      llvm::Value * nv,
				      IQLToLLVMValue::ValueType globalOrLocal)
{
  ctxt->ValueFactory.push_back(new IQLToLLVMValue(val, nv, globalOrLocal));
  return reinterpret_cast<IQLToLLVMValueRef>(ctxt->ValueFactory.back());
}

IQLToLLVMField::IQLToLLVMField(CodeGenerationContext * ctxt,
			       const RecordType * recordType,
			       const std::string& memberName,
			       const std::string& recordName)
  :
  mMemberName(memberName),
  mBasePointer(NULL),
  mRecordType(recordType)
{
  mBasePointer = llvm::unwrap(ctxt->lookupValue(recordName.c_str(), NULL)->getValue());
}

IQLToLLVMField::IQLToLLVMField(const RecordType * recordType,
			       const std::string& memberName,
			       llvm::Value * basePointer)
  :
  mMemberName(memberName),
  mBasePointer(basePointer),
  mRecordType(recordType)
{
}

IQLToLLVMField::~IQLToLLVMField() 
{
}

void IQLToLLVMField::setNull(CodeGenerationContext *ctxt, bool isNull) const
{
  mRecordType->LLVMMemberSetNull(mMemberName, ctxt, mBasePointer, isNull);
}

IQLToLLVMValueRef IQLToLLVMField::getValuePointer(CodeGenerationContext * ctxt) const
{
  llvm::Value * outputVal = mRecordType->LLVMMemberGetPointer(mMemberName, 
							      ctxt, 
							      mBasePointer,
							      false);
  // Don't worry about Nullability, it is dealt separately  
  IQLToLLVMValueRef val = 
    IQLToLLVMValue::get(ctxt, 
			llvm::wrap(outputVal), 
			NULL, 
			IQLToLLVMValue::eGlobal);  
  return val;
}

const IQLToLLVMValue * 
IQLToLLVMField::getEntirePointer(CodeGenerationContext * ctxt) const
{
  llvm::Value * outputVal = mRecordType->LLVMMemberGetPointer(mMemberName, 
							      ctxt, 
							      mBasePointer,
							      false);
  llvm::Value * nullVal = NULL;
  if (isNullable()) {
    nullVal = mRecordType->LLVMMemberGetNull(mMemberName,
					     ctxt,
					     mBasePointer);
  }
  IQLToLLVMValueRef val = 
    IQLToLLVMValue::get(ctxt, 
			llvm::wrap(outputVal), 
			nullVal,
			IQLToLLVMValue::eGlobal);  
  return unwrap(val);
}

bool IQLToLLVMField::isNullable() const
{
  const FieldType * outputTy = mRecordType->getMember(mMemberName).GetType();
  return outputTy->isNullable();
}

IQLToLLVMLocal::IQLToLLVMLocal(const IQLToLLVMValue * val,
			       llvm::Value * nullBit)
  :
  mValue(val),
  mNullBit(nullBit)
{
}

IQLToLLVMLocal::~IQLToLLVMLocal()
{
}

IQLToLLVMValueRef IQLToLLVMLocal::getValuePointer(CodeGenerationContext * ctxt) const
{
  return wrap(mValue);
}

const IQLToLLVMValue * 
IQLToLLVMLocal::getEntirePointer(CodeGenerationContext * ctxt) const
{
  if(NULL == mNullBit) {
    return mValue;
  } else {
    llvm::IRBuilder<> * b = llvm::unwrap(ctxt->LLVMBuilder);
    return unwrap(IQLToLLVMValue::get(ctxt, mValue->getValue(),
				      b->CreateLoad(mNullBit),
				      mValue->getValueType()));
  }
}

llvm::Value * IQLToLLVMLocal::getNullBitPointer() const
{
  return mNullBit;
}

void IQLToLLVMLocal::setNull(CodeGenerationContext * ctxt, bool isNull) const
{
  // Unwrap to C++
  llvm::IRBuilder<> * b = llvm::unwrap(ctxt->LLVMBuilder);
  b->CreateStore(isNull ? b->getTrue() : b->getFalse(), mNullBit);
}

bool IQLToLLVMLocal::isNullable() const
{
  return mNullBit != NULL;
}

SymbolTable::SymbolTable()
{
}

SymbolTable::~SymbolTable()
{
  for(table_type::iterator it = mSymbols.begin();
      it != mSymbols.end();
      ++it) {
    delete it->second;
  }
}

IQLToLLVMLValue * SymbolTable::lookup(const char * nm) const
{
  table_type::const_iterator it = mSymbols.find(nm);
  if (it == mSymbols.end() )
    return NULL;
  else
    return it->second;
}

void SymbolTable::add(const char * nm, IQLToLLVMLValue * value)
{
  // Don't bother worrying about overwriting a symbol table entry
  // this should be safe by virtue of type check. 
  // TODO: We shouldn't even be managing a symbol table during
  // code generation all names should be resolved during type
  // checking.
  // table_type::const_iterator it = mSymbols.find(nm);
  // if (it != mSymbols.end() )
  //   throw std::runtime_error((boost::format("Variable %1% already defined")
  // 			      % nm).str());
  mSymbols[nm] = value;
}

void SymbolTable::clear()
{
  mSymbols.clear();
}

void SymbolTable::dump() const
{
  // for(table_type::const_iterator it = tab.begin();
  //     it != tab.end();
  //     ++it) {
  //   std::cerr << it->first.c_str() << ":";
  //   llvm::unwrap(unwrap(it->second)->getValue())->dump();
  // }
}

CodeGenerationFunctionContext::CodeGenerationFunctionContext()
  :
  Builder(NULL),
  mSymbolTable(NULL),
  Function(NULL),
  RecordArguments(NULL),
  OutputRecord(NULL),
  AllocaCache(NULL)
{
}

CodeGenerationContext::CodeGenerationContext()
  :
  mOwnsModule(true),
  mSymbolTable(NULL),
  LLVMContext(NULL),
  LLVMModule(NULL),
  LLVMBuilder(NULL),
  LLVMDecContextPtrType(NULL),
  LLVMDecimal128Type(NULL),
  LLVMVarcharType(NULL),
  LLVMDatetimeType(NULL),
  LLVMFunction(NULL),
  IQLRecordArguments(NULL),
  IQLOutputRecord(NULL),
  LLVMMemcpyIntrinsic(NULL),
  LLVMMemsetIntrinsic(NULL),
  LLVMMemcmpIntrinsic(NULL),
  IQLMoveSemantics(0),
  IsIdentity(true),
  AggFn(0),
  AllocaCache(NULL)
{
  LLVMContext = ::LLVMContextCreate();
  LLVMModule = ::LLVMModuleCreateWithNameInContext("my cool JIT", 
						   LLVMContext);
}

CodeGenerationContext::~CodeGenerationContext()
{
  typedef std::vector<IQLToLLVMValue *> factory;
  for(factory::iterator it = ValueFactory.begin();
      it != ValueFactory.end();
      ++it) {
    delete *it;
  }

  if (LLVMBuilder) {
    LLVMDisposeBuilder(LLVMBuilder);
    LLVMBuilder = NULL;
  }
  if (mSymbolTable) {
    delete mSymbolTable;
    mSymbolTable = NULL;
  }  
  delete unwrap(IQLRecordArguments);
  if (mOwnsModule && LLVMModule) {
    LLVMDisposeModule(LLVMModule);
    LLVMModule = NULL;
  }
  if (LLVMContext) {
    LLVMContextDispose(LLVMContext);
    LLVMContext = NULL;
  }
  while(IQLCase.size()) {
    delete IQLCase.top();
    IQLCase.pop();
  }
}

void CodeGenerationContext::disownModule()
{
  mOwnsModule = false;
}

void CodeGenerationContext::defineVariable(const char * name,
					   llvm::Value * val,
					   llvm::Value * nullVal,
					   IQLToLLVMValue::ValueType globalOrLocal)
{
  IQLToLLVMValueRef tmp = IQLToLLVMValue::get(this, wrap(val), 
					      NULL, globalOrLocal);
  IQLToLLVMLocal * local = new IQLToLLVMLocal(unwrap(tmp),
					      nullVal);
  mSymbolTable->add(name, NULL, local);
}

void CodeGenerationContext::defineFieldVariable(llvm::Value * basePointer,
						const char * prefix,
						const char * memberName,
						const RecordType * recordType)
{
  IQLToLLVMField * field = new IQLToLLVMField(recordType,
					      memberName,
					      basePointer);
  mSymbolTable->add(prefix, memberName, field);
}

const IQLToLLVMLValue * 
CodeGenerationContext::lookup(const char * name, const char * name2)
{
  TreculSymbolTableEntry * lval = mSymbolTable->lookup(name, name2); 
  return lval->getValue();
}

const IQLToLLVMValue * 
CodeGenerationContext::lookupValue(const char * name, const char * name2)
{
  TreculSymbolTableEntry * lval = mSymbolTable->lookup(name, name2);
  return lval->getValue()->getEntirePointer(this);
}

const IQLToLLVMValue * 
CodeGenerationContext::lookupBasePointer(const char * name)
{
  TreculSymbolTableEntry * lval = mSymbolTable->lookup(name, NULL);
  return lval->getValue()->getEntirePointer(this);
}

LLVMValueRef CodeGenerationContext::getContextArgumentRef()
{
  return lookupValue("__DecimalContext__", NULL)->getValue();
}

void CodeGenerationContext::reinitializeForTransfer()
{
  delete (local_cache *) AllocaCache;
  delete mSymbolTable;
  mSymbolTable = new TreculSymbolTable();
  AllocaCache = new local_cache();
}

void CodeGenerationContext::reinitialize()
{
  // Reinitialize and create transfer
  mSymbolTable->clear();
  LLVMFunction = NULL;
  unwrap(IQLRecordArguments)->clear();
}

void CodeGenerationContext::createFunctionContext()
{
  LLVMBuilder = LLVMCreateBuilderInContext(LLVMContext);
  mSymbolTable = new TreculSymbolTable();
  LLVMFunction = NULL;
  IQLRecordArguments = wrap(new std::map<std::string, std::pair<std::string, const RecordType*> >());
  IQLOutputRecord = NULL;
  AllocaCache = new local_cache();
}

void CodeGenerationContext::dumpSymbolTable()
{
  // mSymbolTable->dump();
}

void CodeGenerationContext::restoreAggregateContext(CodeGenerationFunctionContext * fCtxt)
{
  this->LLVMBuilder = fCtxt->Builder;
  this->mSymbolTable = fCtxt->mSymbolTable;
  this->LLVMFunction = fCtxt->Function;
  this->IQLRecordArguments = fCtxt->RecordArguments;
  this->IQLOutputRecord = fCtxt->OutputRecord;
  this->AllocaCache = (local_cache *) fCtxt->AllocaCache;
}

void CodeGenerationContext::saveAggregateContext(CodeGenerationFunctionContext * fCtxt)
{
  fCtxt->Builder = this->LLVMBuilder;
  fCtxt->mSymbolTable = this->mSymbolTable;
  fCtxt->Function = this->LLVMFunction;
  fCtxt->RecordArguments = this->IQLRecordArguments;
  fCtxt->OutputRecord = this->IQLOutputRecord;
  fCtxt->AllocaCache = this->AllocaCache;
}

void CodeGenerationContext::addInputRecordType(const char * name, 
					       const char * argumentName, 
					       const RecordType * rec)
{
  boost::dynamic_bitset<> mask;
  mask.resize(rec->size(), true);
  addInputRecordType(name, argumentName, rec, mask);
}

void CodeGenerationContext::addInputRecordType(const char * name, 
					       const char * argumentName, 
					       const RecordType * rec,
					       const boost::dynamic_bitset<>& mask)
{
  llvm::Value * basePointer = llvm::unwrap(lookupValue(argumentName, NULL)->getValue());
  for(RecordType::const_member_iterator it = rec->begin_members();
      it != rec->end_members();
      ++it) {
    std::size_t idx = (std::size_t) std::distance(rec->begin_members(), it);
    if (!mask.test(idx)) continue;
    rec->LLVMMemberGetPointer(it->GetName(), 
			      this, 
			      basePointer,
			      true, // Put the member into the symbol table
			      name);
  }
  std::map<std::string, std::pair<std::string, const RecordType *> >& recordTypes(*unwrap(IQLRecordArguments));
  recordTypes[name] = std::make_pair(argumentName, rec);
}

llvm::Value * CodeGenerationContext::addExternalFunction(const char * treculName,
							 const char * implName,
							 llvm::Type * funTy)
{
  mTreculNameToSymbol[treculName] = implName;
  return llvm::Function::Create(llvm::dyn_cast<llvm::FunctionType>(funTy), 
				llvm::GlobalValue::ExternalLinkage,
				implName, llvm::unwrap(LLVMModule));
  
}

void CodeGenerationContext::whileBegin()
{
  // Unwrap to C++
  llvm::LLVMContext * c = llvm::unwrap(LLVMContext);
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  // The function we are working on.
  llvm::Function *TheFunction = b->GetInsertBlock()->getParent();
  // Create blocks for the condition, loop body and continue.
  std::stack<class IQLToLLVMStackRecord* > & stk(IQLStack);
  stk.push(new IQLToLLVMStackRecord());
  stk.top()->ThenBB = llvm::BasicBlock::Create(*c, "whileCond", TheFunction);
  stk.top()->ElseBB = llvm::BasicBlock::Create(*c, "whileBody");
  stk.top()->MergeBB = llvm::BasicBlock::Create(*c, "whileCont");

  // We do an unconditional branch to the condition block
  // so the loop has somewhere to branch to.
  b->CreateBr(stk.top()->ThenBB);
  b->SetInsertPoint(stk.top()->ThenBB);  
}

void CodeGenerationContext::whileStatementBlock(const IQLToLLVMValue * condVal,
						const FieldType * condTy)
{  
  // Test the condition and branch 
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  llvm::Function * f = b->GetInsertBlock()->getParent();
  std::stack<class IQLToLLVMStackRecord* > & stk(IQLStack);
  f->getBasicBlockList().push_back(stk.top()->ElseBB);
  conditionalBranch(condVal, condTy, stk.top()->ElseBB, stk.top()->MergeBB);
  b->SetInsertPoint(stk.top()->ElseBB);
}

void CodeGenerationContext::whileFinish()
{
  // Unwrap to C++
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  llvm::Function * f = b->GetInsertBlock()->getParent();
  std::stack<class IQLToLLVMStackRecord* > & stk(IQLStack);

  // Branch to reevaluate loop predicate
  b->CreateBr(stk.top()->ThenBB);
  f->getBasicBlockList().push_back(stk.top()->MergeBB);
  b->SetInsertPoint(stk.top()->MergeBB);

  // Done with this entry
  delete stk.top();
  stk.pop();
}

void CodeGenerationContext::conditionalBranch(const IQLToLLVMValue * condVal,
					      const FieldType * condTy,
					      llvm::BasicBlock * trueBranch,
					      llvm::BasicBlock * falseBranch)
{
  llvm::LLVMContext * c = llvm::unwrap(LLVMContext);
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  llvm::Function * f = b->GetInsertBlock()->getParent();
  
  // Handle ternary logic here
  llvm::Value * nv = condVal->getNull();
  if (nv) {
    llvm::BasicBlock * notNullBB = llvm::BasicBlock::Create(*c, "notNull", f);
    b->CreateCondBr(b->CreateNot(nv), notNullBB, falseBranch);
    b->SetInsertPoint(notNullBB);
  }
  // Cast back to i1 by comparing to zero.
  llvm::Value * boolVal = b->CreateICmpNE(llvm::unwrap(condVal->getValue()),
					  b->getInt32(0),
					  "boolCast");
  // Branch and set block
  b->CreateCondBr(boolVal, trueBranch, falseBranch);
}

const IQLToLLVMValue * 
CodeGenerationContext::buildArray(std::vector<const IQLToLLVMValue *>& vals,
				  FieldType * arrayTy)
{
  // Detect if this is an array of constants
  // TODO: We need analysis or attributes that tell us whether the
  // array is const before we can make it static.  Right now we are just
  // making an generally invalid assumption that an array of numeric
  // constants is in fact const.
  bool isConstArray=true;
  for(std::vector<const IQLToLLVMValue *>::iterator v = vals.begin(),
	e = vals.end(); v != e; ++v) {
    if (!llvm::isa<llvm::Constant>(llvm::unwrap((*v)->getValue()))) {
      isConstArray = false;
      break;
    }
  }

  if (isConstArray) {
    return buildGlobalConstArray(vals, arrayTy);
  }

  // TODO: This is potentially inefficient.  Will LLVM remove the extra copy?
  // Even if it does, how much are we adding to the compilation time while
  // it cleans up our mess.
  llvm::LLVMContext * c = llvm::unwrap(LLVMContext);
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  llvm::Type * retTy = arrayTy->LLVMGetType(this);
  LLVMValueRef result = LLVMCreateEntryBlockAlloca(this, 
						   llvm::wrap(retTy),
						   "nullableBinOp");    
  llvm::Type * ptrToElmntTy = llvm::cast<llvm::SequentialType>(retTy)->getElementType();
  ptrToElmntTy = llvm::PointerType::get(ptrToElmntTy, 0);
  llvm::Value * ptrToElmnt = b->CreateBitCast(llvm::unwrap(result), ptrToElmntTy);
  // TODO: We are not allowing nullable element types for arrays at this point.
  int32_t sz = arrayTy->GetSize();
  for (int32_t i=0; i<sz; ++i) {
    // Make an LValue out of a slot in the array so we can
    // set the value into it.

    // GEP to get pointer to the correct offset.
    llvm::Value * gepIndexes[1] = { b->getInt64(i) };
    llvm::Value * lval = b->CreateInBoundsGEP(ptrToElmnt, 
					      llvm::ArrayRef<llvm::Value*>(&gepIndexes[0], 
									   &gepIndexes[1]));
    const IQLToLLVMValue * slot = unwrap(IQLToLLVMValue::get(this, 
							     llvm::wrap(lval),
							     IQLToLLVMValue::eLocal));
    IQLToLLVMLocal localLVal(slot, NULL);
    IQLToLLVMBuildSetNullableValue(this, &localLVal, wrap(vals[i]));
  }

  // return pointer to array
  return unwrap(IQLToLLVMValue::get(this, result, IQLToLLVMValue::eLocal));
}

const IQLToLLVMValue * 
CodeGenerationContext::buildGlobalConstArray(std::vector<const IQLToLLVMValue *>& vals,
					     FieldType * arrayTy)
{
  llvm::Module * m = llvm::unwrap(LLVMModule);
  llvm::LLVMContext * c = llvm::unwrap(LLVMContext);
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  llvm::ArrayType * arrayType = 
    llvm::dyn_cast<llvm::ArrayType>(arrayTy->LLVMGetType(this));
  BOOST_ASSERT(arrayType != NULL);
  llvm::GlobalVariable * globalArray = 
    new llvm::GlobalVariable(*m, arrayType, true, llvm::GlobalValue::InternalLinkage,
			  0, "constArray");
  globalArray->setAlignment(16);

  // Make initializer for the global.
  std::vector<llvm::Constant *> initializerArgs;
  for(std::vector<const IQLToLLVMValue *>::const_iterator v = vals.begin(),
	e = vals.end(); v != e; ++v) {
    initializerArgs.push_back(llvm::cast<llvm::Constant>(llvm::unwrap((*v)->getValue())));
  }
  llvm::Constant * constArrayInitializer = 
    llvm::ConstantArray::get(arrayType, initializerArgs);
  globalArray->setInitializer(constArrayInitializer);

  
  return unwrap(IQLToLLVMValue::get(this, llvm::wrap(globalArray), IQLToLLVMValue::eLocal));
}

IQLToLLVMValue::ValueType 
CodeGenerationContext::buildCall(const char * treculName,
				 const std::vector<const IQLToLLVMValue *> & args,
				 llvm::Value * retTmp,
				 const FieldType * retType)
{
  // Get the implementation name of the function.
  std::map<std::string,std::string>::const_iterator it = mTreculNameToSymbol.find(treculName);
  if (mTreculNameToSymbol.end() == it) {
    throw std::runtime_error((boost::format("Unable to find implementation for "
					    "function %1%") % treculName).str());
  }
  llvm::LLVMContext & c(*llvm::unwrap(LLVMContext));
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);

  std::vector<LLVMValueRef> callArgs;
  LLVMValueRef fn = LLVMGetNamedFunction(LLVMModule, it->second.c_str());
  if (fn == NULL) {
    throw std::runtime_error((boost::format("Call to function %1% passed type checking "
					    "but implementation function %2% does not exist") %
			      treculName % it->second).str());
  }
  
  for(std::size_t i=0; i<args.size(); i++) {
    if (IQLToLLVMTypePredicate::isChar(args[i]->getValue())) {
      LLVMValueRef e = args[i]->getValue();
      // CHAR(N) arg must pass by reference
      // Pass as a pointer to int8.  pointer to char(N) is too specific
      // for a type signature.
      LLVMTypeRef int8Ptr = LLVMPointerType(LLVMInt8TypeInContext(LLVMContext), 0);
      LLVMValueRef ptr = LLVMBuildBitCast(LLVMBuilder, e, int8Ptr, "charcnvcasttmp1");
      callArgs.push_back(ptr);
    } else {
      callArgs.push_back(args[i]->getValue());
    }
  }
  
  const llvm::Type * unwrapped = llvm::unwrap(LLVMTypeOf(fn));
  const llvm::PointerType * ptrTy = llvm::dyn_cast<llvm::PointerType>(unwrapped);
  const llvm::FunctionType * fnTy = llvm::dyn_cast<llvm::FunctionType>(ptrTy->getElementType());
  if (fnTy->getReturnType() == llvm::Type::getVoidTy(c)) {
    // Validate the calling convention.  If returning void must
    // also take RuntimeContext as last argument and take pointer
    // to return as next to last argument.
    if (callArgs.size() + 2 != fnTy->getNumParams() ||
	fnTy->getParamType(fnTy->getNumParams()-1) != 
	llvm::unwrap(LLVMDecContextPtrType) ||
	!fnTy->getParamType(fnTy->getNumParams()-2)->isPointerTy())
      throw std::runtime_error("Internal Error");

    const llvm::Type * retTy = retType->LLVMGetType(this);
    // The return type is determined by next to last argument.
    llvm::Type * retArgTy = llvm::cast<llvm::PointerType>(fnTy->getParamType(fnTy->getNumParams()-2))->getElementType();

    // Must alloca a value for the return value and pass as an arg.
    // No guarantee that the type of the formal of the function is exactly
    // the same as the LLVM ret type (in particular, CHAR(N) return
    // values will have a int8* formal) so we do a bitcast.
    LLVMValueRef retVal = llvm::wrap(retTmp);
    if (retTy != retArgTy) {
      const llvm::ArrayType * arrTy = llvm::dyn_cast<llvm::ArrayType>(retTy);
      if (retArgTy != b->getInt8Ty() ||
	  NULL == arrTy ||
	  arrTy->getElementType() != b->getInt8Ty()) {
	throw std::runtime_error("INTERNAL ERROR: mismatch between IQL function "
				 "return type and LLVM formal argument type.");
      }
      retVal = LLVMBuildBitCast(LLVMBuilder, 
				retVal,
				llvm::wrap(llvm::PointerType::get(retArgTy, 0)),
				"callReturnTempCast");
    }
    callArgs.push_back(retVal);					
    // Also must pass the context for allocating the string memory.
    callArgs.push_back(LLVMBuildLoad(LLVMBuilder, 
				     getContextArgumentRef(),
				     "ctxttmp"));    
    LLVMBuildCall(LLVMBuilder, 
		  fn, 
		  &callArgs[0], 
		  callArgs.size(), 
		  "");
    // Return was the second to last entry in the arg list.
    return IQLToLLVMValue::eLocal;
  } else {
    LLVMValueRef r = LLVMBuildCall(LLVMBuilder, 
				   fn, 
				   &callArgs[0], 
				   callArgs.size(), 
				   "call");
    b->CreateStore(llvm::unwrap(r), retTmp);
    return IQLToLLVMValue::eLocal;
  }
}

IQLToLLVMValue::ValueType 
CodeGenerationContext::buildCastInt32(const IQLToLLVMValue * e, 
				      const FieldType * argAttrs, 
				      llvm::Value * ret, 
				      const FieldType * retAttrs)
{
  llvm::Value * e1 = llvm::unwrap(e->getValue());
  llvm::LLVMContext * c = llvm::unwrap(LLVMContext);
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  std::vector<const IQLToLLVMValue *> args;
  args.push_back(e);

  switch(argAttrs->GetEnum()) {
  case FieldType::INT32:
    b->CreateStore(e1, ret);
    return IQLToLLVMValue::eLocal;
  case FieldType::INT64:
    {
      llvm::Value * r = b->CreateTrunc(e1, 
				       b->getInt32Ty(),
				       "castInt64ToInt32");
      b->CreateStore(r, ret);
      return IQLToLLVMValue::eLocal;
    }
  case FieldType::DOUBLE:
    {
      llvm::Value * r = b->CreateFPToSI(e1, 
					b->getInt32Ty(),
					"castDoubleToInt32");
      b->CreateStore(r, ret);
      return IQLToLLVMValue::eLocal;
    }
  case FieldType::CHAR:
    {
      return buildCall("InternalInt32FromChar", args, ret, retAttrs);
    }
  case FieldType::VARCHAR:
    {
      return buildCall("InternalInt32FromVarchar", args, ret, retAttrs);
    }
  case FieldType::BIGDECIMAL:
    {
      return buildCall("InternalInt32FromDecimal", args, ret, retAttrs);
    }
  case FieldType::DATE:
    {
      return buildCall("InternalInt32FromDate", args, ret, retAttrs);
    }
  case FieldType::DATETIME:
    {
      return buildCall("InternalInt32FromDatetime", args, ret, retAttrs);
    }
  default:
    // TODO: Cast INTEGER to DECIMAL
    throw std::runtime_error ((boost::format("Cast to INTEGER from %1% not "
					     "implemented.") % 
			       retAttrs->toString()).str());
  }
}

IQLToLLVMValue::ValueType 
CodeGenerationContext::buildCastInt64(const IQLToLLVMValue * e, 
				      const FieldType * argAttrs, 
				      llvm::Value * ret, 
				      const FieldType * retAttrs)
{
  llvm::Value * e1 = llvm::unwrap(e->getValue());
  llvm::LLVMContext * c = llvm::unwrap(LLVMContext);
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  std::vector<const IQLToLLVMValue *> args;
  args.push_back(e);

  switch(argAttrs->GetEnum()) {
  case FieldType::INT32:
    {
      llvm::Value * r = b->CreateSExt(e1, 
				      b->getInt64Ty(),
				      "castInt64ToInt32");
      b->CreateStore(r, ret);
      return IQLToLLVMValue::eLocal;
    }
  case FieldType::INT64:
    b->CreateStore(e1, ret);
    return IQLToLLVMValue::eLocal;
  case FieldType::DOUBLE:
    {
      llvm::Value * r = b->CreateFPToSI(e1, 
					b->getInt64Ty(),
					"castDoubleToInt64");
      b->CreateStore(r, ret);
      return IQLToLLVMValue::eLocal;
    }
  case FieldType::CHAR:
    {
      return buildCall("InternalInt64FromChar", args, ret, retAttrs);
    }
  case FieldType::VARCHAR:
    {
      return buildCall("InternalInt64FromVarchar", args, ret, retAttrs);
    }
  case FieldType::BIGDECIMAL:
    {
      return buildCall("InternalInt64FromDecimal", args, ret, retAttrs);
    }
  case FieldType::DATE:
    {
      return buildCall("InternalInt64FromDate", args, ret, retAttrs);
    }
  case FieldType::DATETIME:
    {
      return buildCall("InternalInt64FromDatetime", args, ret, retAttrs);
    }
  default:
    // TODO: 
    throw std::runtime_error ((boost::format("Cast to BIGINT from %1% not "
					     "implemented.") % 
			       retAttrs->toString()).str());
  }
}

IQLToLLVMValue::ValueType 
CodeGenerationContext::buildCastDouble(const IQLToLLVMValue * e, 
				       const FieldType * argAttrs, 
				       llvm::Value * ret, 
				       const FieldType * retAttrs)
{
  llvm::Value * e1 = llvm::unwrap(e->getValue());
  llvm::LLVMContext * c = llvm::unwrap(LLVMContext);
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  std::vector<const IQLToLLVMValue *> args;
  args.push_back(e);

  switch(argAttrs->GetEnum()) {
  case FieldType::INT32:
  case FieldType::INT64:
    {
      llvm::Value * r = b->CreateSIToFP(e1, 
					b->getDoubleTy(),
					"castIntToDouble");
      b->CreateStore(r, ret);
      return IQLToLLVMValue::eLocal;
    }
  case FieldType::DOUBLE:
    b->CreateStore(e1, ret);
    return IQLToLLVMValue::eLocal;
  case FieldType::CHAR:
    {
      return buildCall("InternalDoubleFromChar", args, ret, retAttrs);
    }
  case FieldType::VARCHAR:
    {
      return buildCall("InternalDoubleFromVarchar", args, ret, retAttrs);
    }
  case FieldType::BIGDECIMAL:
    {
      return buildCall("InternalDoubleFromDecimal", args, ret, retAttrs);
    }
  case FieldType::DATE:
    {
      return buildCall("InternalDoubleFromDate", args, ret, retAttrs);
    }
  case FieldType::DATETIME:
    {
      return buildCall("InternalDoubleFromDatetime", args, ret, retAttrs);
    }
  default:
    throw std::runtime_error ((boost::format("Cast to DOUBLE PRECISION from %1% not "
					     "implemented.") % 
			       retAttrs->toString()).str());
  }
}

IQLToLLVMValue::ValueType 
CodeGenerationContext::buildCastDecimal(const IQLToLLVMValue * e, 
				       const FieldType * argAttrs, 
				       llvm::Value * ret, 
				       const FieldType * retAttrs)
{
  llvm::Value * e1 = llvm::unwrap(e->getValue());
  llvm::LLVMContext * c = llvm::unwrap(LLVMContext);
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  std::vector<const IQLToLLVMValue *> args;
  args.push_back(e);

  switch(argAttrs->GetEnum()) {
  case FieldType::INT32:
    {
      return buildCall("InternalDecimalFromInt32", args, ret, retAttrs);
    }
  case FieldType::INT64:
    {
      return buildCall("InternalDecimalFromInt64", args, ret, retAttrs);
    }
  case FieldType::DOUBLE:
    {
      return buildCall("InternalDecimalFromDouble", args, ret, retAttrs);
    }
  case FieldType::CHAR:
    {
      return buildCall("InternalDecimalFromChar", args, ret, retAttrs);
    }
  case FieldType::VARCHAR:
    {
      return buildCall("InternalDecimalFromVarchar", args, ret, retAttrs);
    }
  case FieldType::BIGDECIMAL:
    b->CreateStore(b->CreateLoad(e1), ret);
    return e->getValueType();
  case FieldType::DATE:
    {
      return buildCall("InternalDecimalFromDate", args, ret, retAttrs);
    }
  case FieldType::DATETIME:
    {
      return buildCall("InternalDecimalFromDatetime", args, ret, retAttrs);
    }
  default:
    throw std::runtime_error ((boost::format("Cast to DECIMAL from %1% not "
					     "implemented.") % 
			       retAttrs->toString()).str());
  }
}

IQLToLLVMValue::ValueType 
CodeGenerationContext::buildSub(const IQLToLLVMValue * lhs, 
				const FieldType * lhsType, 
				const IQLToLLVMValue * rhs, 
				const FieldType * rhsType, 
				llvm::Value * ret, 
				const FieldType * retType)
{
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  if (lhsType->GetEnum() == FieldType::DATE ||
      lhsType->GetEnum() == FieldType::DATETIME) {
    // Negate the interval value (which is integral)
    // then call add
    LLVMValueRef neg = 
      LLVMBuildNeg(LLVMBuilder, rhs->getValue(), "negintervaltmp");
    rhs = unwrap(IQLToLLVMValue::get(this, neg, IQLToLLVMValue::eLocal));
    return buildDateAdd(lhs, lhsType, rhs, rhsType, ret, retType);
  } else {  
    IQLToLLVMBinaryConversion cvt(this, wrap(lhs), wrap(rhs));
    lhs = unwrap(cvt.getLHS());
    rhs = unwrap(cvt.getRHS());
    llvm::Value * e1 = llvm::unwrap(lhs->getValue());
    llvm::Value * e2 = llvm::unwrap(rhs->getValue());
    if (retType->isIntegral()) {
      b->CreateStore(b->CreateSub(e1, e2), ret);
      return IQLToLLVMValue::eLocal;
    } else if (retType->isFloatingPoint()) {
      b->CreateStore(b->CreateFSub(e1, e2), ret);
      return IQLToLLVMValue::eLocal;
    } else if (retType->GetEnum() == FieldType::BIGDECIMAL) {
      /* call the decimal add library function */
      /* for decimal types we are getting alloca pointers in our expressions */
      return IQLToLLVMCreateBinaryDecimalCall(this, wrap(lhs), wrap(rhs), llvm::wrap(ret), iqlOpDecMinus);     
    } else {
      throw std::runtime_error("INTERNAL ERROR: unexpected type in subtract");
    }
  }
}

IQLToLLVMValue::ValueType 
CodeGenerationContext::buildDateAdd(const IQLToLLVMValue * lhs, 
				    const FieldType * lhsType, 
				    const IQLToLLVMValue * rhs, 
				    const FieldType * rhsType, 
				    llvm::Value * retVal, 
				    const FieldType * retType)
{
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  if (lhsType != NULL && lhsType->GetEnum() == FieldType::INTERVAL) {
    std::swap(lhs, rhs);
    std::swap(lhsType, rhsType);
  }
  const IntervalType * intervalType = dynamic_cast<const IntervalType *>(rhsType);
  IntervalType::IntervalUnit unit = intervalType->getIntervalUnit();
  LLVMValueRef callArgs[2];
  static const char * types [] = {"datetime", "date"};
  const char * ty = 
    lhsType->GetEnum() == FieldType::DATETIME ? types[0] : types[1];
  std::string 
    fnName((boost::format(
			  unit == IntervalType::DAY ? "%1%_add_day" : 
			  unit == IntervalType::HOUR ? "%1%_add_hour" :
			  unit == IntervalType::MINUTE ? "%1%_add_minute" :
			  unit == IntervalType::MONTH ? "%1%_add_month" :
			  unit == IntervalType::SECOND ? "%1%_add_second" :
			  "%1%_add_year") % ty).str());
  LLVMValueRef fn = 
    LLVMGetNamedFunction(LLVMModule, fnName.c_str());
  callArgs[0] = lhs->getValue();
  callArgs[1] = rhs->getValue();
  LLVMValueRef ret = LLVMBuildCall(LLVMBuilder, fn, &callArgs[0], 2, "");
  b->CreateStore(llvm::unwrap(ret), retVal);
  return IQLToLLVMValue::eLocal;  
}

llvm::Value * 
CodeGenerationContext::buildVarcharIsSmall(llvm::Value * varcharPtr)
{
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  // Access first bit of the structure to see if large or small.
  llvm::Value * firstByte = 
    b->CreateLoad(b->CreateBitCast(varcharPtr, b->getInt8PtrTy()));
  return 
    b->CreateICmpEQ(b->CreateAnd(b->getInt8(1U),
				 firstByte),
		    b->getInt8(0U));
}

llvm::Value * 
CodeGenerationContext::buildVarcharGetSize(llvm::Value * varcharPtr)
{
  llvm::LLVMContext * c = llvm::unwrap(LLVMContext);
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  llvm::Function * f = b->GetInsertBlock()->getParent();

  llvm::Value * ret = b->CreateAlloca(b->getInt32Ty());
  
  llvm::BasicBlock * smallBB = llvm::BasicBlock::Create(*c, "small", f);
  llvm::BasicBlock * largeBB = llvm::BasicBlock::Create(*c, "large", f);
  llvm::BasicBlock * contBB = llvm::BasicBlock::Create(*c, "cont", f);
  
  b->CreateCondBr(buildVarcharIsSmall(varcharPtr), smallBB, largeBB);
  b->SetInsertPoint(smallBB);
  llvm::Value * firstByte = 
    b->CreateLoad(b->CreateBitCast(varcharPtr, b->getInt8PtrTy()));
  b->CreateStore(b->CreateSExt(b->CreateAShr(b->CreateAnd(b->getInt8(0xfe),
							  firstByte),  
					     b->getInt8(1U)),
			       b->getInt32Ty()),
		 ret);
  b->CreateBr(contBB);
  b->SetInsertPoint(largeBB);
  llvm::Value * firstDWord = b->CreateLoad(b->CreateStructGEP(varcharPtr, 0));
  b->CreateStore(b->CreateAShr(b->CreateAnd(b->getInt32(0xfffffffe),
					    firstDWord),  
			       b->getInt32(1U)),
		 ret);
  b->CreateBr(contBB);
  b->SetInsertPoint(contBB);
  return b->CreateLoad(ret);
}

llvm::Value * 
CodeGenerationContext::buildVarcharGetPtr(llvm::Value * varcharPtr)
{
  llvm::LLVMContext * c = llvm::unwrap(LLVMContext);
  llvm::IRBuilder<> * b = llvm::unwrap(LLVMBuilder);
  llvm::Function * f = b->GetInsertBlock()->getParent();

  llvm::Value * ret = b->CreateAlloca(b->getInt8PtrTy());
  
  llvm::BasicBlock * smallBB = llvm::BasicBlock::Create(*c, "small", f);
  llvm::BasicBlock * largeBB = llvm::BasicBlock::Create(*c, "large", f);
  llvm::BasicBlock * contBB = llvm::BasicBlock::Create(*c, "cont", f);
  
  b->CreateCondBr(buildVarcharIsSmall(varcharPtr), smallBB, largeBB);
  b->SetInsertPoint(smallBB);
  b->CreateStore(b->CreateConstGEP1_64(b->CreateBitCast(varcharPtr, 
							b->getInt8PtrTy()), 
				       1),
		 ret);
  b->CreateBr(contBB);
  b->SetInsertPoint(largeBB);
  b->CreateStore(b->CreateLoad(b->CreateStructGEP(varcharPtr, 2)), ret);
  b->CreateBr(contBB);
  b->SetInsertPoint(contBB);
  return b->CreateLoad(ret);
}


LLVMSymbolTableRef LLVMSymbolTableCreate()
{
  return reinterpret_cast<LLVMSymbolTableRef>(new std::map<std::string, IQLToLLVMValueRef>());
}

void LLVMSymbolTableFree(LLVMSymbolTableRef symTable)
{
  delete reinterpret_cast<std::map<std::string, IQLToLLVMValueRef> *>(symTable);
}

IQLToLLVMValueRef LLVMSymbolTableLookup(LLVMSymbolTableRef symTable, const char * name)
{
  std::map<std::string, IQLToLLVMValueRef> * tab = reinterpret_cast<std::map<std::string, IQLToLLVMValueRef> *>(symTable);
  std::map<std::string, IQLToLLVMValueRef>::const_iterator it = tab->find(name);
  if (tab->end() == it) {
    throw std::runtime_error((boost::format("Undefined variable: %1%") % name).str());
    //return NULL;
  }
  return it->second;
}

void LLVMSymbolTableAdd(LLVMSymbolTableRef symTable, const char * name, IQLToLLVMValueRef value)
{
  std::map<std::string, IQLToLLVMValueRef> * tab = reinterpret_cast<std::map<std::string, IQLToLLVMValueRef> *>(symTable);
  std::map<std::string, IQLToLLVMValueRef>::const_iterator it = tab->find(name);
  (*tab)[name] = value;
}

void LLVMSymbolTableClear(LLVMSymbolTableRef symTable)
{
  reinterpret_cast<std::map<std::string, IQLToLLVMValueRef> *>(symTable)->clear();
}

void LLVMSymbolTableDump(LLVMSymbolTableRef symTable)
{
  const std::map<std::string, IQLToLLVMValueRef>& tab(*reinterpret_cast<std::map<std::string, IQLToLLVMValueRef> *>(symTable));
  for(std::map<std::string, IQLToLLVMValueRef>::const_iterator it = tab.begin();
      it != tab.end();
      ++it) {
    std::cerr << it->first.c_str() << ":";
    llvm::unwrap(unwrap(it->second)->getValue())->dump();
  }
}

bool IQLToLLVMTypePredicate::isChar(LLVMTypeRef ty)
{
  // This is safe for now since we don't have int8 as an 
  // IQL type.  Ultimately this should be removed and
  // we should be using FieldType for this info.
  return LLVMPointerTypeKind == LLVMGetTypeKind(ty) &&
    LLVMArrayTypeKind == LLVMGetTypeKind(LLVMGetElementType(ty)) &&
    llvm::unwrap(LLVMGetElementType(LLVMGetElementType(ty)))->isIntegerTy(8);
}
bool IQLToLLVMTypePredicate::isChar(LLVMValueRef val)
{
  return isChar(LLVMTypeOf(val));
}
bool IQLToLLVMTypePredicate::isArrayType(LLVMTypeRef ty)
{
  // This is safe for now since we don't have int8 as an 
  // IQL type.  Ultimately this should be removed and
  // we should be using FieldType for this info.
  return LLVMPointerTypeKind == LLVMGetTypeKind(ty) &&
    LLVMArrayTypeKind == LLVMGetTypeKind(LLVMGetElementType(ty)) &&
    !llvm::unwrap(LLVMGetElementType(LLVMGetElementType(ty)))->isIntegerTy(8);
}
bool IQLToLLVMTypePredicate::isArrayType(LLVMValueRef val)
{
  return isArrayType(LLVMTypeOf(val));
}

IQLToLLVMValueRef IQLToLLVMBinaryConversion::convertIntToDec(CodeGenerationContext * ctxt,
							     LLVMValueRef llvmVal,
							     bool isInt64)
{
  const char * convertFn = isInt64 ? "InternalDecimalFromInt64" : "InternalDecimalFromInt32";
  const char * retValName = isInt64 ? "64ToDecimal" : "32ToDecimal";
  LLVMValueRef callArgs[3];
  LLVMValueRef fn = LLVMGetNamedFunction(ctxt->LLVMModule, convertFn);
  callArgs[0] = llvmVal;
  callArgs[1] = LLVMCreateEntryBlockAlloca(ctxt, 
					   ctxt->LLVMDecimal128Type, 
					   retValName);
  callArgs[2] = LLVMBuildLoad(ctxt->LLVMBuilder, 
			      ctxt->getContextArgumentRef(),
			      "ctxttmp");
  LLVMBuildCall(ctxt->LLVMBuilder, fn, &callArgs[0], 3, "");
  return IQLToLLVMValue::get(ctxt, 
			     callArgs[1],
			     IQLToLLVMValue::eLocal);
}

IQLToLLVMValueRef IQLToLLVMBinaryConversion::convertDecToDouble(CodeGenerationContext * ctxt,
								LLVMValueRef llvmVal)
{
  LLVMValueRef callArgs[3];
  LLVMValueRef fn = LLVMGetNamedFunction(ctxt->LLVMModule, "InternalDoubleFromDecimal");
  callArgs[0] = llvmVal;
  callArgs[1] = LLVMCreateEntryBlockAlloca(ctxt, 
					   LLVMDoubleTypeInContext(ctxt->LLVMContext), 
					   "retDecToDouble");
  callArgs[2] = LLVMBuildLoad(ctxt->LLVMBuilder, 
			      ctxt->getContextArgumentRef(),
			      "ctxttmp");
  LLVMBuildCall(ctxt->LLVMBuilder, fn, &callArgs[0], 3, "");
  return IQLToLLVMValue::get(ctxt, 
			     LLVMBuildLoad(ctxt->LLVMBuilder,
					   callArgs[1], "retDecToDoubleResult"),
			     IQLToLLVMValue::eLocal);
}

/**
 * Can e1 be cast to e2?
 */
LLVMTypeRef IQLToLLVMBinaryConversion::castTo(CodeGenerationContext * ctxt, LLVMTypeRef e1, LLVMTypeRef e2)
{
  if (e1 == LLVMInt32TypeInContext(ctxt->LLVMContext)) {
    if (e2 == LLVMInt32TypeInContext(ctxt->LLVMContext))
      return e1;
    else if (e2 == LLVMInt64TypeInContext(ctxt->LLVMContext))
      return e2;
    else if (e2 == LLVMDoubleTypeInContext(ctxt->LLVMContext))
      return e2;
    else if (e2 == LLVMPointerType(ctxt->LLVMDecimal128Type, 0))
      return e2;
    else 
      return NULL;
  } else if (e1 == LLVMInt64TypeInContext(ctxt->LLVMContext)) {
    if (e2 == LLVMInt64TypeInContext(ctxt->LLVMContext))
      return e2;
    else if (e2 == LLVMDoubleTypeInContext(ctxt->LLVMContext))
      return e2;
    else if (e2 == LLVMPointerType(ctxt->LLVMDecimal128Type, 0))
      return e2;
    else 
      return NULL;    
  } else if (e1 == LLVMPointerType(ctxt->LLVMDecimal128Type, 0)) {
    if (e2 == LLVMPointerType(ctxt->LLVMDecimal128Type, 0))
      return e2;
    else if (e2 == LLVMDoubleTypeInContext(ctxt->LLVMContext))
      return e2;
    else 
      return NULL;    
  } else if (IQLToLLVMTypePredicate::isChar(e1)) { // Char types
    // TODO: What about CHAR(M) and CHAR(N) where M!=N?
    if (IQLToLLVMTypePredicate::isChar(e2) &&
	IQLToLLVMTypeInspector::getCharArrayLength(e1) == 
	IQLToLLVMTypeInspector::getCharArrayLength(e2))
      return e2;
    // Type promotion of CHAR(N) to VARCHAR(max)
    else if (LLVMPointerTypeKind == LLVMGetTypeKind(e2) &&
	     LLVMGetElementType(e2) == ctxt->LLVMVarcharType)
      return e2;
    else 
      return NULL;    
  } else {
    if (e1 == e2) 
      return e1;
    else
      return NULL;
  }
}
/**
 * Can e1 be cast to e2 or vice versa?
 */
LLVMTypeRef IQLToLLVMBinaryConversion::leastCommonType(CodeGenerationContext * ctxt, LLVMTypeRef e1, LLVMTypeRef e2)
{
  LLVMTypeRef ty = castTo(ctxt, e1, e2);
  if (ty != NULL) return ty;
  return castTo(ctxt, e2, e1);
}
/**
 * Convert a value to the target type.
 */
IQLToLLVMValueRef IQLToLLVMBinaryConversion::convertTo(CodeGenerationContext * ctxt, 
						       IQLToLLVMValueRef v, 
						       LLVMTypeRef e2)
{
  LLVMValueRef llvmVal = unwrap(v)->getValue();
  // NULL literal
  if (llvmVal == NULL)
    return v;
  LLVMTypeRef e1 = LLVMTypeOf(llvmVal);
  // No conversion 
  if (e1 == e2) 
    return v;
    
  // Supported conversions
  if (e1 == LLVMInt32TypeInContext(ctxt->LLVMContext)) {
    if (e2 == LLVMInt64TypeInContext(ctxt->LLVMContext)) {
      return IQLToLLVMValue::get(ctxt, 
				 LLVMBuildSExt(ctxt->LLVMBuilder, 
					       llvmVal, 
					       LLVMInt64TypeInContext(ctxt->LLVMContext),
					       "32to64"),
				 IQLToLLVMValue::eLocal);
    } else if (e2 == LLVMDoubleTypeInContext(ctxt->LLVMContext)) {
      return IQLToLLVMValue::get(ctxt, 
				 LLVMBuildSIToFP(ctxt->LLVMBuilder, 
						 llvmVal, 
						 LLVMDoubleTypeInContext(ctxt->LLVMContext),
						 "32toDouble"),
				 IQLToLLVMValue::eLocal);
    } else if (e2 == LLVMPointerType(ctxt->LLVMDecimal128Type, 0)) {
      return convertIntToDec(ctxt, llvmVal, false);
    } else {
      return NULL;
    }
  } else if (e1 == LLVMInt64TypeInContext(ctxt->LLVMContext)) {
    if (e2 == LLVMDoubleTypeInContext(ctxt->LLVMContext)) {
      return IQLToLLVMValue::get(ctxt, 
				 LLVMBuildSIToFP(ctxt->LLVMBuilder, 
						 llvmVal, 
						 LLVMDoubleTypeInContext(ctxt->LLVMContext),
						 "64toDouble"),
				 IQLToLLVMValue::eLocal);
    } else if (e2 == LLVMPointerType(ctxt->LLVMDecimal128Type, 0)) {
      return convertIntToDec(ctxt, llvmVal, true);
    } else {
      return NULL; 
    }   
  } else if (e1 == LLVMPointerType(ctxt->LLVMDecimal128Type, 0)) {
    if (e2 == LLVMDoubleTypeInContext(ctxt->LLVMContext)) {
      return convertDecToDouble(ctxt, llvmVal);
    } else {
      return NULL; 
    }   
  } else if (IQLToLLVMTypePredicate::isChar(e1)) { // Char types
    // TODO: What about CHAR(M) and CHAR(N) where M!=N?
    // Type promotion of CHAR(N) to VARCHAR(max).  Must allocate
    // the varchar.
    if (LLVMPointerTypeKind == LLVMGetTypeKind(e2) &&
	LLVMGetElementType(e2) == ctxt->LLVMVarcharType) {
      LLVMValueRef callArgs[3];
      LLVMValueRef fn = LLVMGetNamedFunction(ctxt->LLVMModule, "InternalVarcharFromChar");
      // Cast char array to a pointer to int8_t.
      LLVMTypeRef int8Ptr = LLVMPointerType(LLVMInt8TypeInContext(ctxt->LLVMContext), 0);
      callArgs[0] = LLVMBuildBitCast(ctxt->LLVMBuilder, llvmVal, int8Ptr, "charcnvcasttmp1");
      // This is the varchar we are converting to.
      callArgs[1] = LLVMCreateEntryBlockAlloca(ctxt, ctxt->LLVMVarcharType, "varcharliteral");
      callArgs[2] = LLVMBuildLoad(ctxt->LLVMBuilder, 
				  ctxt->getContextArgumentRef(),
				  "ctxttmp");
      LLVMBuildCall(ctxt->LLVMBuilder, fn, &callArgs[0], 3, "");
      return IQLToLLVMValue::get(ctxt, callArgs[1], IQLToLLVMValue::eLocal);
    }
  } else {
    return NULL;
  }
  return NULL;
}					     
/**
 * Convert a value to the target type.
 */
IQLToLLVMValueRef IQLToLLVMBinaryConversion::convertTo(CodeGenerationContext * ctxt, 
						       IQLToLLVMValueRef v, 
						       const FieldType * ty)
{
  LLVMTypeRef cvtTy = IQLToLLVMValue::getVariableType(ctxt, ty);
  return convertTo(ctxt, v, cvtTy);
}

IQLToLLVMBinaryConversion::IQLToLLVMBinaryConversion(CodeGenerationContext * ctxt, IQLToLLVMValueRef lhs, IQLToLLVMValueRef rhs)
  :
  mLHS(NULL),
  mRHS(NULL),
  mResultType(NULL)
{
  // Figure out the conversion to perform.
  mResultType = leastCommonType(ctxt,
				LLVMTypeOf(unwrap(lhs)->getValue()), 
				LLVMTypeOf(unwrap(rhs)->getValue()));
  mLHS = convertTo(ctxt, lhs, mResultType);
  mRHS = convertTo(ctxt, rhs, mResultType);
}

int32_t IQLToLLVMTypeInspector::getCharArrayLength(LLVMTypeRef ty)
{
  return LLVMGetArrayLength(LLVMGetElementType(ty));
}

int32_t IQLToLLVMTypeInspector::getCharArrayLength(LLVMValueRef val)
{
  return getCharArrayLength(LLVMTypeOf(val));
}
