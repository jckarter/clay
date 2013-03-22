#include "error.hpp"
#include "objects.hpp"
#include "evaluator.hpp"
#include "constructors.hpp"
#include "loader.hpp"
#include "env.hpp"

#include "analyzer.hpp"

#include "analyzer_op.hpp"


namespace clay {



static bool staticToSizeTOrInt(ObjectPtr x, size_t &out)
{
    if (x->objKind != VALUE_HOLDER)
        return false;
    ValueHolderPtr vh = (ValueHolder *)x.ptr();
    if (vh->type == cSizeTType) {
        out = *((size_t *)vh->buf);
        return true;
    }
    else if (vh->type == cIntType) {
        int i = *((int *)vh->buf);
        out = size_t(i);
        return true;
    }
    else {
        return false;
    }
}

static TypePtr numericTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index].type;
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return t;
    default :
        argumentTypeError(index, "numeric type", t);
        return NULL;
    }
}

static IntegerTypePtr integerTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index].type;
    if (t->typeKind != INTEGER_TYPE)
        argumentTypeError(index, "integer type", t);
    return (IntegerType *)t.ptr();
}

static PointerTypePtr pointerTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index].type;
    if (t->typeKind != POINTER_TYPE)
        argumentTypeError(index, "pointer type", t);
    return (PointerType *)t.ptr();
}

static CCodePointerTypePtr cCodePointerTypeOfValue(MultiPValuePtr x,
                                                   unsigned index)
{
    TypePtr t = x->values[index].type;
    if (t->typeKind != CCODE_POINTER_TYPE)
        argumentTypeError(index, "external code pointer type", t);
    return (CCodePointerType *)t.ptr();
}

static ArrayTypePtr arrayTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index].type;
    if (t->typeKind != ARRAY_TYPE)
        argumentTypeError(index, "array type", t);
    return (ArrayType *)t.ptr();
}

static TupleTypePtr tupleTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index].type;
    if (t->typeKind != TUPLE_TYPE)
        argumentTypeError(index, "tuple type", t);
    return (TupleType *)t.ptr();
}

static RecordTypePtr recordTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index].type;
    if (t->typeKind != RECORD_TYPE)
        argumentTypeError(index, "record type", t);
    return (RecordType *)t.ptr();
}






static TypePtr valueToType(MultiPValuePtr x, unsigned index)
{
    ObjectPtr obj = unwrapStaticType(x->values[index].type);
    if (!obj)
        argumentError(index, "expecting a type");
    TypePtr t;
    if (!staticToType(obj, t))
        argumentError(index, "expecting a type");
    return t;
}

static TypePtr valueToNumericType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return t;
    default :
        argumentTypeError(index, "numeric type", t);
        return NULL;
    }
}

static IntegerTypePtr valueToIntegerType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    if (t->typeKind != INTEGER_TYPE)
        argumentTypeError(index, "integer type", t);
    return (IntegerType *)t.ptr();
}

static TypePtr valueToPointerLikeType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    if (!isPointerOrCodePointerType(t))
        argumentTypeError(index, "pointer or code-pointer type", t);
    return t;
}

static EnumTypePtr valueToEnumType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    if (t->typeKind != ENUM_TYPE)
        argumentTypeError(index, "enum type", t);
    return (EnumType*)t.ptr();
}


static std::pair<vector<TypePtr>, InvokeEntry*>
invokeEntryForCallableArguments(MultiPValuePtr args, unsigned callableIndex, unsigned firstArgTypeIndex)
{
    if (args->size() < firstArgTypeIndex)
        arityError2(firstArgTypeIndex, args->size());
    ObjectPtr callable = unwrapStaticType(args->values[callableIndex].type);
    if (!callable)
        argumentError(callableIndex, "static callable expected");
    switch (callable->objKind) {
    case TYPE :
    case RECORD_DECL :
    case VARIANT_DECL :
    case PROCEDURE :
    case GLOBAL_ALIAS :
        break;
    case PRIM_OP :
        if (!isOverloadablePrimOp(callable))
            argumentError(callableIndex, "invalid callable");
        break;
    default :
        argumentError(callableIndex, "invalid callable");
    }
    vector<TypePtr> argsKey;
    vector<ValueTempness> argsTempness;
    for (unsigned i = firstArgTypeIndex; i < args->size(); ++i) {
        TypePtr t = valueToType(args, i);
        argsKey.push_back(t);
        argsTempness.push_back(TEMPNESS_LVALUE);
    }

    CompileContextPusher pusher(callable, argsKey);

    return std::make_pair(
        argsKey,
        analyzeCallable(callable, argsKey, argsTempness));
}




MultiPValuePtr analyzePrimOp(PrimOpPtr x, MultiPValuePtr args)
{
    switch (x->primOpCode) {

    case PRIM_TypeP :
        return new MultiPValue(PVData(boolType, true));

    case PRIM_TypeSize :
        return new MultiPValue(PVData(cSizeTType, true));

    case PRIM_TypeAlignment :
        return new MultiPValue(PVData(cSizeTType, true));

    case PRIM_OperatorP :
    case PRIM_SymbolP :
    case PRIM_StaticCallDefinedP :
        return new MultiPValue(PVData(boolType, true));

    case PRIM_StaticCallOutputTypes : {
        std::pair<vector<TypePtr>, InvokeEntry*> entry =
            invokeEntryForCallableArguments(args, 0, 1);
        MultiPValuePtr values = new MultiPValue();
        for (size_t i = 0; i < entry.second->returnTypes.size(); ++i)
            values->add(staticPValue(entry.second->returnTypes[i].ptr()));
        return values;
    }

    case PRIM_StaticMonoP : {
        return new MultiPValue(PVData(boolType, true));
    }

    case PRIM_StaticMonoInputTypes : {
        ensureArity(args, 1);
        ObjectPtr callable = unwrapStaticType(args->values[0].type);
        if (!callable)
            argumentError(0, "static callable expected");

        switch (callable->objKind) {
        case PROCEDURE : {
            Procedure *proc = (Procedure*)callable.ptr();
            if (proc->mono.monoState != Procedure_MonoOverload) {
                argumentError(0, "not a static monomorphic callable");
            }

            MultiPValuePtr values = new MultiPValue();
            for (size_t i = 0; i < proc->mono.monoTypes.size(); ++i)
                values->add(staticPValue(proc->mono.monoTypes[i].ptr()));

            return values;
        }

        default :
            argumentError(0, "not a static monomorphic callable");
        }
    }

    case PRIM_bitcopy :
        return new MultiPValue();

    case PRIM_boolNot :
        return new MultiPValue(PVData(boolType, true));

    case PRIM_integerEqualsP :
    case PRIM_integerLesserP :
    case PRIM_floatOrderedEqualsP :
    case PRIM_floatOrderedLesserP :
    case PRIM_floatOrderedLesserEqualsP :
    case PRIM_floatOrderedGreaterP :
    case PRIM_floatOrderedGreaterEqualsP :
    case PRIM_floatOrderedNotEqualsP :
    case PRIM_floatOrderedP :
    case PRIM_floatUnorderedEqualsP :
    case PRIM_floatUnorderedLesserP :
    case PRIM_floatUnorderedLesserEqualsP :
    case PRIM_floatUnorderedGreaterP :
    case PRIM_floatUnorderedGreaterEqualsP :
    case PRIM_floatUnorderedNotEqualsP :
    case PRIM_floatUnorderedP :
        ensureArity(args, 2);
        return new MultiPValue(PVData(boolType, true));

    case PRIM_numericAdd :
    case PRIM_numericSubtract :
    case PRIM_numericMultiply :
    case PRIM_floatDivide : {
        ensureArity(args, 2);
        TypePtr t = numericTypeOfValue(args, 0);
        return new MultiPValue(PVData(t, true));
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t = numericTypeOfValue(args, 0);
        return new MultiPValue(PVData(t, true));
    }

    case PRIM_integerQuotient :
    case PRIM_integerRemainder :
    case PRIM_integerShiftLeft :
    case PRIM_integerShiftRight :
    case PRIM_integerBitwiseAnd :
    case PRIM_integerBitwiseOr :
    case PRIM_integerBitwiseXor :
    case PRIM_integerAddChecked :
    case PRIM_integerSubtractChecked :
    case PRIM_integerMultiplyChecked :
    case PRIM_integerQuotientChecked :
    case PRIM_integerRemainderChecked :
    case PRIM_integerShiftLeftChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t = integerTypeOfValue(args, 0);
        return new MultiPValue(PVData(t.ptr(), true));
    }

    case PRIM_integerBitwiseNot :
    case PRIM_integerNegateChecked : {
        ensureArity(args, 1);
        IntegerTypePtr t = integerTypeOfValue(args, 0);
        return new MultiPValue(PVData(t.ptr(), true));
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr t = valueToNumericType(args, 0);
        return new MultiPValue(PVData(t, true));
    }

    case PRIM_integerConvertChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t = valueToIntegerType(args, 0);
        return new MultiPValue(PVData(t.ptr(), true));
    }

    case PRIM_Pointer :
        error("no Pointer type constructor overload found");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        TypePtr t = pointerType(args->values[0].type);
        return new MultiPValue(PVData(t, true));
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr pt = pointerTypeOfValue(args, 0);
        return new MultiPValue(PVData(pt->pointeeType, false));
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr pt = pointerTypeOfValue(args, 0);
        return new MultiPValue(PVData(pt.ptr(), true));
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr t = valueToIntegerType(args, 0);
        return new MultiPValue(PVData(t.ptr(), true));
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr t = valueToPointerLikeType(args, 0);
        return new MultiPValue(PVData(t, true));
    }

    case PRIM_nullPointer : {
        ensureArity(args, 1);
        TypePtr t = valueToPointerLikeType(args, 0);
        return new MultiPValue(PVData(t, true));
    }

    case PRIM_CodePointer :
        error("no CodePointer type constructor overload found");

    case PRIM_makeCodePointer : {
        std::pair<vector<TypePtr>, InvokeEntry*> entry =
            invokeEntryForCallableArguments(args, 0, 1);
        if (entry.second->callByName)
            argumentError(0, "cannot create pointer to call-by-name code");
        if (!entry.second->analyzed)
            return NULL;
        TypePtr cpType = codePointerType(entry.first,
                                         entry.second->returnIsRef,
                                         entry.second->returnTypes);
        return new MultiPValue(PVData(cpType, true));
    }

    case PRIM_ExternalCodePointer :
        error("no ExternalCodePointer type constructor overload found");

    case PRIM_makeExternalCodePointer : {
        std::pair<vector<TypePtr>, InvokeEntry*> entry =
            invokeEntryForCallableArguments(args, 0, 3);
        if (entry.second->callByName)
            argumentError(0, "cannot create pointer to call-by-name code");
        if (!entry.second->analyzed)
            return NULL;

        ObjectPtr ccObj = unwrapStaticType(args->values[1].type);
        CallingConv cc;
        if (!staticToCallingConv(ccObj, cc))
            argumentError(1, "expecting a calling convention attribute");

        ObjectPtr isVarArgObj = unwrapStaticType(args->values[2].type);
        bool isVarArg;
        TypePtr type;
        if (!staticToBool(isVarArgObj, isVarArg, type))
            argumentTypeError(2, "static Bool", type);

        if (isVarArg)
            argumentError(2, "implementation of external variadic functions is not yet supported");

        TypePtr returnType;
        if (entry.second->returnTypes.empty()) {
            returnType = NULL;
        }
        else if (entry.second->returnTypes.size() == 1) {
            if (entry.second->returnIsRef[0]) {
                argumentError(0, "cannot create external code pointer to "
                              " return-by-reference code");
            }
            returnType = entry.second->returnTypes[0];
        }
        else {
            argumentError(0, "cannot create external code pointer to "
                          "multi-return code");
        }
        TypePtr ccpType = cCodePointerType(cc,
                                           entry.first,
                                           isVarArg,
                                           returnType);
        return new MultiPValue(PVData(ccpType, true));
    }

    case PRIM_callExternalCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        CCodePointerTypePtr y = cCodePointerTypeOfValue(args, 0);
        if (!y->returnType)
            return new MultiPValue();
        return new MultiPValue(PVData(y->returnType, true));
    }

    case PRIM_bitcast : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args, 0);
        return new MultiPValue(PVData(t, false));
    }

    case PRIM_Array :
        error("no Array type constructor overload found");

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ArrayTypePtr t = arrayTypeOfValue(args, 0);
        return new MultiPValue(PVData(t->elementType, false));
    }

    case PRIM_arrayElements: {
        ensureArity(args, 1);
        ArrayTypePtr t = arrayTypeOfValue(args, 0);
        MultiPValuePtr mpv = new MultiPValue();
        for (size_t i = 0; i < t->size; ++i)
            mpv->add(PVData(t->elementType, false));
        return mpv;
    }

    case PRIM_Vec :
        error("no Vec type constructor overload found");

    case PRIM_Tuple :
        error("no Tuple type constructor overload found");

    case PRIM_TupleElementCount :
        return new MultiPValue(PVData(cSizeTType, true));

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        TupleTypePtr t = tupleTypeOfValue(args, 0);
        ObjectPtr obj = unwrapStaticType(args->values[1].type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(1, "expecting static SizeT or Int value");
        if (i >= t->elementTypes.size())
            argumentIndexRangeError(1, "tuple element index",
                                    i, t->elementTypes.size());
        return new MultiPValue(PVData(t->elementTypes[i], false));
    }

    case PRIM_tupleElements : {
        ensureArity(args, 1);
        TupleTypePtr t = tupleTypeOfValue(args, 0);
        MultiPValuePtr mpv = new MultiPValue();
        for (size_t i = 0; i < t->elementTypes.size(); ++i)
            mpv->add(PVData(t->elementTypes[i], false));
        return mpv;
    }

    case PRIM_Union :
        error("no Union type constructor overload found");

    case PRIM_UnionMemberCount :
        return new MultiPValue(PVData(cSizeTType, true));

    case PRIM_RecordP :
        return new MultiPValue(PVData(boolType, true));

    case PRIM_RecordFieldCount :
        return new MultiPValue(PVData(cSizeTType, true));

    case PRIM_RecordFieldName : {
        ensureArity(args, 2);
        ObjectPtr first = unwrapStaticType(args->values[0].type);
        if (!first)
            argumentError(0, "expecting a record type");
        TypePtr t;
        if (!staticToType(first, t))
            argumentError(0, "expecting a record type");
        if (t->typeKind != RECORD_TYPE)
            argumentError(0, "expecting a record type");
        RecordType *rt = (RecordType *)t.ptr();
        ObjectPtr second = unwrapStaticType(args->values[1].type);
        size_t i = 0;
        if (!second || !staticToSizeTOrInt(second, i))
            argumentError(1, "expecting static SizeT or Int value");
        const vector<IdentifierPtr> fieldNames = recordFieldNames(rt);
        if (i >= fieldNames.size())
            argumentIndexRangeError(1, "record field index",
                                    i, fieldNames.size());
        return new MultiPValue(staticPValue(fieldNames[i].ptr()));
    }

    case PRIM_RecordWithFieldP :
        return new MultiPValue(PVData(boolType, true));

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        RecordTypePtr t = recordTypeOfValue(args, 0);
        ObjectPtr obj = unwrapStaticType(args->values[1].type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(1, "expecting static SizeT or Int value");
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(t);
        if (i >= t->fieldCount())
            argumentIndexRangeError(1, "record field index",
                                    i, t->fieldCount());

        MultiPValuePtr mpv = new MultiPValue();
        if (t->hasVarField && i >= t->varFieldPosition)
            if (i == t->varFieldPosition)
                for (size_t j=0;j < t->varFieldSize(); ++j)
                    mpv->add(PVData(fieldTypes[i+j], false));
            else
                mpv->add(PVData(fieldTypes[i+t->varFieldSize()-1], false));
        else
            mpv->add(PVData(fieldTypes[i], false));
        return mpv;
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        RecordTypePtr t = recordTypeOfValue(args, 0);
        ObjectPtr obj = unwrapStaticType(args->values[1].type);
        if (!obj || (obj->objKind != IDENTIFIER))
            argumentError(1, "expecting field name identifier");
        IdentifierPtr fname = (Identifier *)obj.ptr();
        const llvm::StringMap<size_t> &fieldIndexMap = recordFieldIndexMap(t);
        llvm::StringMap<size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end()) {
            string buf;
            llvm::raw_string_ostream sout(buf);
            sout << "field not found: " << fname->str;
            argumentError(1, sout.str());
        }
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(t);
        MultiPValuePtr mpv = new MultiPValue();
        size_t i = fi->second;
        if (t->hasVarField && i >= t->varFieldPosition)
            if (i == t->varFieldPosition)
                for (size_t j=0;j < t->varFieldSize(); ++j)
                    mpv->add(PVData(fieldTypes[i+j], false));
            else
                mpv->add(PVData(fieldTypes[i+t->varFieldSize()-1], false));
        else
            mpv->add(PVData(fieldTypes[i], false));
        return mpv;
    }

    case PRIM_recordFields : {
        ensureArity(args, 1);
        RecordTypePtr t = recordTypeOfValue(args, 0);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(t);
        MultiPValuePtr mpv = new MultiPValue();
        for (size_t i = 0; i < fieldTypes.size(); ++i)
            mpv->add(PVData(fieldTypes[i], false));
        return mpv;
    }

    case PRIM_recordVariadicField : {
        ensureArity(args, 1);
        RecordTypePtr t = recordTypeOfValue(args, 0);
        if (!t->hasVarField)
            argumentError(0, "expecting a record with a variadic field");
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(t);
        MultiPValuePtr mpv = new MultiPValue();
        size_t varEnd = t->varFieldPosition + t->varFieldSize();
        for (size_t i = t->varFieldPosition; i < varEnd; ++i)
            mpv->add(PVData(fieldTypes[i], false));
        return mpv;
    }

    case PRIM_VariantP :
        return new MultiPValue(PVData(boolType, true));

    case PRIM_VariantMemberIndex :
        return new MultiPValue(PVData(cSizeTType, true));

    case PRIM_VariantMemberCount :
        return new MultiPValue(PVData(cSizeTType, true));

    case PRIM_VariantMembers : {
        ensureArity(args, 1);
        ObjectPtr typeObj = unwrapStaticType(args->values[0].type);
        if (!typeObj)
            argumentError(0, "expecting a variant type");
        TypePtr t;
        if (!staticToType(typeObj, t))
            argumentError(0, "expecting a variant type");
        if (t->typeKind != VARIANT_TYPE)
            argumentError(0, "expecting a variant type");
        VariantType *vt = (VariantType *)t.ptr();
        llvm::ArrayRef<TypePtr> members = variantMemberTypes(vt);

        MultiPValuePtr mpv = new MultiPValue();
        for (TypePtr const *i = members.begin(), *end = members.end();
             i != end;
             ++i)
        {
            mpv->add(staticPValue(i->ptr()));
        }
        return mpv;
    }

    case PRIM_BaseType : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        if (!obj && obj->objKind != TYPE)
            argumentError(0, "static type expected");
        Type *type = (Type *)obj.ptr();
        MultiPValuePtr mpv = new MultiPValue();
        if(type->typeKind == NEW_TYPE) {
            NewType *nt = (NewType*)type;
            mpv->add(staticPValue(newtypeReprType(nt).ptr()));
        } else {
            mpv->add(staticPValue(type));
        }
        return mpv;
    }

    case PRIM_Static :
        error("no Static type constructor overload found");

    case PRIM_staticIntegers : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        if (!obj || (obj->objKind != VALUE_HOLDER))
            argumentError(0, "expecting a static SizeT or Int value");
        MultiPValuePtr mpv = new MultiPValue();
        ValueHolder *vh = (ValueHolder *)obj.ptr();
        if (vh->type == cIntType) {
            int count = *((int *)vh->buf);
            if (count < 0)
                argumentError(0, "negative values are not allowed");
            for (int i = 0; i < count; ++i) {
                ValueHolderPtr vhi = intToValueHolder(i);
                mpv->add(PVData(staticType(vhi.ptr()), true));
            }
        }
        else if (vh->type == cSizeTType) {
            size_t count = *((size_t *)vh->buf);
            for (size_t i = 0; i < count; ++i) {
                ValueHolderPtr vhi = sizeTToValueHolder(i);
                mpv->add(PVData(staticType(vhi.ptr()), true));
            }
        }
        else {
            argumentError(0, "expecting a static SizeT or Int value");
        }
        return mpv;
    }

    case PRIM_integers : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        if (!obj || (obj->objKind != VALUE_HOLDER))
            argumentError(0, "expecting a static SizeT or Int value");
        MultiPValuePtr mpv = new MultiPValue();
        ValueHolder *vh = (ValueHolder *)obj.ptr();
        if (vh->type == cIntType) {
            int count = *((int *)vh->buf);
            if (count < 0)
                argumentError(0, "negative values are not allowed");
            for (int i = 0; i < count; ++i)
                mpv->add(PVData(cIntType, true));
        }
        else if (vh->type == cSizeTType) {
            size_t count = *((size_t *)vh->buf);
            for (size_t i = 0; i < count; ++i)
                mpv->add(PVData(cSizeTType, true));
        }
        else {
            argumentError(0, "expecting a static SizeT or Int value");
        }
        return mpv;
    }

    case PRIM_staticFieldRef : {
        ensureArity(args, 2);
        ObjectPtr moduleObj = unwrapStaticType(args->values[0].type);
        if (!moduleObj || (moduleObj->objKind != MODULE))
            argumentError(0, "expecting a module");
        ObjectPtr identObj = unwrapStaticType(args->values[1].type);
        if (!identObj || (identObj->objKind != IDENTIFIER))
            argumentError(1, "expecting a string literal value");
        Module *module = (Module *)moduleObj.ptr();
        Identifier *ident = (Identifier *)identObj.ptr();
        ObjectPtr obj = safeLookupPublic(module, ident);
        return analyzeStaticObject(obj);
    }

    case PRIM_EnumP :
        return new MultiPValue(PVData(boolType, true));

    case PRIM_EnumMemberCount :
        return new MultiPValue(PVData(cSizeTType, true));

    case PRIM_EnumMemberName : {
        ensureArity(args, 2);
        EnumTypePtr et = valueToEnumType(args, 0);
        ObjectPtr obj = unwrapStaticType(args->values[1].type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(1, "expecting static SizeT or Int value");

        EnumDeclPtr e = et->enumeration;
        if (i >= e->members.size())
            argumentIndexRangeError(1, "enum member index",
                                    i, e->members.size());

        return analyzeStaticObject(e->members[i]->name.ptr());
    }

    case PRIM_enumToInt :
        return new MultiPValue(PVData(cIntType, true));

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        EnumTypePtr t = valueToEnumType(args, 0);
        initializeEnumType(t);
        return new MultiPValue(PVData(t.ptr(), true));
    }

    case PRIM_StringLiteralP :
        return new MultiPValue(PVData(boolType, true));

    case PRIM_stringLiteralByteIndex :
        return new MultiPValue(PVData(cIntType, true));

    case PRIM_stringLiteralBytes : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        if (!obj || (obj->objKind != IDENTIFIER))
            argumentError(0, "expecting a string literal value");
        Identifier *ident = (Identifier *)obj.ptr();
        MultiPValuePtr result = new MultiPValue();
        for (size_t i = 0, sz = ident->str.size(); i < sz; ++i)
            result->add(PVData(cIntType, true));
        return result;
    }

    case PRIM_stringLiteralByteSize :
        return new MultiPValue(PVData(cSizeTType, true));

    case PRIM_stringLiteralByteSlice : {
        ensureArity(args, 3);
        ObjectPtr identObj = unwrapStaticType(args->values[0].type);
        if (!identObj || (identObj->objKind != IDENTIFIER))
            argumentError(0, "expecting a string literal value");
        Identifier *ident = (Identifier *)identObj.ptr();
        ObjectPtr beginObj = unwrapStaticType(args->values[1].type);
        ObjectPtr endObj = unwrapStaticType(args->values[2].type);
        size_t begin = 0, end = 0;
        if (!beginObj || !staticToSizeTOrInt(beginObj, begin))
            argumentError(1, "expecting a static SizeT or Int value");
        if (!endObj || !staticToSizeTOrInt(endObj, end))
            argumentError(2, "expecting a static SizeT or Int value");
        if (end > ident->str.size()) {
            argumentIndexRangeError(2, "ending index",
                                    end, ident->str.size());
        }
        if (begin > end)
            argumentIndexRangeError(1, "starting index",
                                    begin, end);
        llvm::StringRef result(&ident->str[unsigned(begin)], end - begin);
        return analyzeStaticObject(Identifier::get(result));
    }

    case PRIM_stringLiteralConcat : {
        llvm::SmallString<32> result;
        for (size_t i = 0, sz = args->size(); i < sz; ++i) {
            ObjectPtr obj = unwrapStaticType(args->values[unsigned(i)].type);
            if (!obj || (obj->objKind != IDENTIFIER))
                argumentError(i, "expecting a string literal value");
            Identifier *ident = (Identifier *)obj.ptr();
            result.append(ident->str.begin(), ident->str.end());
        }
        return analyzeStaticObject(Identifier::get(result));
    }

    case PRIM_stringLiteralFromBytes : {
        std::string result;
        for (size_t i = 0, sz = args->size(); i < sz; ++i) {
            ObjectPtr obj = unwrapStaticType(args->values[unsigned(i)].type);
            size_t byte = 0;
            if (!obj || !staticToSizeTOrInt(obj, byte))
                argumentError(i, "expecting a static SizeT or Int value");
            if (byte > 255) {
                string buf;
                llvm::raw_string_ostream os(buf);
                os << byte << " is too large for a string literal byte";
                argumentError(i, os.str());
            }
            result.push_back((char)byte);
        }
        return analyzeStaticObject(Identifier::get(result));
    }

    case PRIM_stringTableConstant :
        return new MultiPValue(PVData(pointerType(cSizeTType), true));

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        if (!obj)
            argumentError(0, "expecting static object");
        llvm::SmallString<128> buf;
        llvm::raw_svector_ostream sout(buf);
        printStaticName(sout, obj);
        return analyzeStaticObject(Identifier::get(sout.str()));
    }

    case PRIM_MainModule : {
        ensureArity(args, 0);
        assert(globalMainModule != NULL);
        return analyzeStaticObject(globalMainModule.ptr());
    }

    case PRIM_StaticModule : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        if (!obj)
            argumentError(0, "expecting static object");
        ModulePtr m = staticModule(obj);
        if (!m)
            argumentError(0, "value has no associated module");
        return analyzeStaticObject(m.ptr());
    }

    case PRIM_ModuleName : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        if (!obj)
            argumentError(0, "expecting static object");
        ModulePtr m = staticModule(obj);
        if (!m)
            argumentError(0, "value has no associated module");
        return analyzeStaticObject(Identifier::get(m->moduleName));
    }

    case PRIM_ModuleMemberNames : {
        ensureArity(args, 1);
        ObjectPtr moduleObj = unwrapStaticType(args->values[0].type);
        if (!moduleObj || (moduleObj->objKind != MODULE))
            argumentError(0, "expecting a module");
        ModulePtr m = staticModule(moduleObj);
        assert(m != NULL);

        MultiPValuePtr result = new MultiPValue();

        for (llvm::StringMap<ObjectPtr>::const_iterator i = m->publicGlobals.begin(),
                end = m->publicGlobals.end();
            i != end;
            ++i)
        {
            result->add(staticPValue(Identifier::get(i->getKey())));
        }
        return result;
    }

    case PRIM_FlagP : {
        return new MultiPValue(PVData(boolType, true));
    }

    case PRIM_Flag : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        if (obj != NULL && obj->objKind == IDENTIFIER) {
            Identifier *ident = (Identifier*)obj.ptr();

            llvm::StringMap<string>::const_iterator flag = globalFlags.find(ident->str);
            string value;
            if (flag != globalFlags.end())
                value = flag->second;

            return analyzeStaticObject(Identifier::get(value));
        } else
            argumentTypeError(0, "identifier", args->values[0].type);
        return NULL;
    }

    case PRIM_atomicFence : {
        ensureArity(args, 1);
        return new MultiPValue();
    }

    case PRIM_atomicRMW : {
        // order op ptr val
        ensureArity(args, 4);
        pointerTypeOfValue(args, 2);
        return new MultiPValue(PVData(args->values[3].type, true));
    }

    case PRIM_atomicLoad : {
        // order ptr
        ensureArity(args, 2);
        PointerTypePtr pt = pointerTypeOfValue(args, 1);
        return new MultiPValue(PVData(pt->pointeeType, true));
    }

    case PRIM_atomicStore : {
        // order ptr val
        ensureArity(args, 3);
        pointerTypeOfValue(args, 1);
        return new MultiPValue();
    }

    case PRIM_atomicCompareExchange : {
        // order ptr old new
        ensureArity(args, 4);
        PointerTypePtr pt = pointerTypeOfValue(args, 1);

        return new MultiPValue(PVData(args->values[2].type, true));
    }

    case PRIM_activeException : {
        ensureArity(args, 0);
        return new MultiPValue(PVData(pointerType(uint8Type), true));
    }

    case PRIM_memcpy :
    case PRIM_memmove : {
        ensureArity(args, 3);
        PointerTypePtr toPt = pointerTypeOfValue(args, 0);
        PointerTypePtr fromPt = pointerTypeOfValue(args, 1);
        integerTypeOfValue(args, 2);
        return new MultiPValue();
    }

    case PRIM_countValues : {
        return new MultiPValue(PVData(cIntType, true));
    }

    case PRIM_nthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(0, "expecting static SizeT or Int value");
        if (i+1 >= args->size())
            argumentError(0, "nthValue argument out of bounds");
        return new MultiPValue(args->values[unsigned(i)+1]);
    }

    case PRIM_withoutNthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(0, "expecting static SizeT or Int value");
        if (i+1 >= args->size())
            argumentError(0, "withoutNthValue argument out of bounds");
        MultiPValuePtr mpv = new MultiPValue();
        for (unsigned n = 1; n < args->size(); ++n) {
            if (n == i+1)
                continue;
            mpv->add(args->values[n]);
        }
        return mpv;
    }

    case PRIM_takeValues : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(0, "expecting static SizeT or Int value");
        if (i+1 >= args->size())
            i = args->size() - 1;
        MultiPValuePtr mpv = new MultiPValue();
        for (unsigned n = 1; n < i+1; ++n)
            mpv->add(args->values[n]);
        return mpv;
    }

    case PRIM_dropValues : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr obj = unwrapStaticType(args->values[0].type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(0, "expecting static SizeT or Int value");
        if (i+1 >= args->size())
            i = args->size() - 1;
        MultiPValuePtr mpv = new MultiPValue();
        for (size_t n = i+1; n < args->size(); ++n)
            mpv->add(args->values[unsigned(n)]);
        return mpv;
    }

    case PRIM_LambdaRecordP : {
        ensureArity(args, 1);
        return new MultiPValue(PVData(boolType, true));
    }

    case PRIM_LambdaSymbolP : {
        ensureArity(args, 1);
        return new MultiPValue(PVData(boolType, true));
    }

    case PRIM_LambdaMonoP : {
        ensureArity(args, 1);
        return new MultiPValue(PVData(boolType, true));
    }

    case PRIM_LambdaMonoInputTypes : {
        ensureArity(args, 1);
        ObjectPtr callable = unwrapStaticType(args->values[0].type);
        if (!callable)
            argumentError(0, "lambda record type expected");

        if (callable != NULL && callable->objKind == TYPE) {
            Type *t = (Type*)callable.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *r = (RecordType*)t;
                if (r->record->lambda->mono.monoState != Procedure_MonoOverload)
                    argumentError(0, "not a monomorphic lambda record type");
                llvm::ArrayRef<TypePtr> monoTypes = r->record->lambda->mono.monoTypes;
                MultiPValuePtr values = new MultiPValue();
                for (size_t i = 0; i < monoTypes.size(); ++i)
                    values->add(staticPValue(monoTypes[i].ptr()));

                return values;
            }
        }

        argumentError(0, "not a monomorphic lambda record type");
        return new MultiPValue();
    }

    case PRIM_GetOverload : {
        std::pair<vector<TypePtr>, InvokeEntry*> entry =
            invokeEntryForCallableArguments(args, 0, 1);

        llvm::SmallString<128> buf;
        llvm::raw_svector_ostream nameout(buf);
        nameout << "GetOverload(";
        printStaticName(nameout, entry.second->callable);
        nameout << ", ";
        nameout << ")";

        ProcedurePtr proc = new Procedure(
            NULL, Identifier::get(nameout.str()),
            PRIVATE, false);

        proc->env = entry.second->env;

        CodePtr code = new Code(), origCode = entry.second->origCode;
        for (vector<FormalArgPtr>::const_iterator arg = origCode->formalArgs.begin(),
                end = origCode->formalArgs.end();
             arg != end;
             ++arg)
        {
            code->formalArgs.push_back(new FormalArg((*arg)->name, NULL));
        }

        code->hasVarArg = origCode->hasVarArg;

        if (origCode->hasNamedReturns()) {
            code->returnSpecsDeclared = true;
            code->returnSpecs = origCode->returnSpecs;
            code->varReturnSpec = origCode->varReturnSpec;
        }

        code->body = origCode->body;
        code->llvmBody = origCode->llvmBody;

        OverloadPtr overload = new Overload(
            NULL,
            new ObjectExpr(proc.ptr()),
            code,
            entry.second->callByName,
            entry.second->isInline);
        overload->env = entry.second->env;
        addProcedureOverload(proc, entry.second->env, overload);

        return new MultiPValue(staticPValue(proc.ptr()));
    }

    case PRIM_usuallyEquals : {
        ensureArity(args, 2);
        if (!isPrimitiveType(args->values[0].type))
            argumentTypeError(0, "primitive type", args->values[0].type);
        ObjectPtr expectedValue = unwrapStaticType(args->values[1].type);
        if (expectedValue == NULL
            || expectedValue->objKind != VALUE_HOLDER
            || ((ValueHolder*)expectedValue.ptr())->type != args->values[0].type)
            error("second argument to usuallyEquals must be a static value of the same type as the first argument");
        return new MultiPValue(PVData(args->values[0].type, true));
    }

    default :
        assert(false);
        return NULL;

    }
}


}
