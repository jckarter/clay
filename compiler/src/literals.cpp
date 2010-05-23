#include "clay.hpp"

ValueHolderPtr parseIntLiteral(IntLiteral *x)
{
    char *ptr = const_cast<char *>(x->value.c_str());
    char *end = ptr;
    ValueHolderPtr vh;
    if (x->suffix == "i8") {
        long y = strtol(ptr, &end, 0);
        if (*end != 0)
            error("invalid int8 literal");
        if ((errno == ERANGE) || (y < SCHAR_MIN) || (y > SCHAR_MAX))
            error("int8 literal out of range");
        vh = new ValueHolder(int8Type);
        *((char *)vh->buf) = (char)y;
    }
    else if (x->suffix == "i16") {
        long y = strtol(ptr, &end, 0);
        if (*end != 0)
            error("invalid int16 literal");
        if ((errno == ERANGE) || (y < SHRT_MIN) || (y > SHRT_MAX))
            error("int16 literal out of range");
        vh = new ValueHolder(int16Type);
        *((short *)vh->buf) = (short)y;
    }
    else if ((x->suffix == "i32") || x->suffix.empty()) {
        long y = strtol(ptr, &end, 0);
        if (*end != 0)
            error("invalid int32 literal");
        if (errno == ERANGE)
            error("int32 literal out of range");
        vh = new ValueHolder(int32Type);
        *((int *)vh->buf) = (int)y;
    }
    else if (x->suffix == "i64") {
        long long y = strtoll(ptr, &end, 0);
        if (*end != 0)
            error("invalid int64 literal");
        if (errno == ERANGE)
            error("int64 literal out of range");
        vh = new ValueHolder(int64Type);
        *((long long *)vh->buf) = y;
    }
    else if (x->suffix == "u8") {
        unsigned long y = strtoul(ptr, &end, 0);
        if (*end != 0)
            error("invalid uint8 literal");
        if ((errno == ERANGE) || (y > UCHAR_MAX))
            error("uint8 literal out of range");
        vh = new ValueHolder(uint8Type);
        *((unsigned char *)vh->buf) = (unsigned char)y;
    }
    else if (x->suffix == "u16") {
        unsigned long y = strtoul(ptr, &end, 0);
        if (*end != 0)
            error("invalid uint16 literal");
        if ((errno == ERANGE) || (y > USHRT_MAX))
            error("uint16 literal out of range");
        vh = new ValueHolder(uint16Type);
        *((unsigned short *)vh->buf) = (unsigned short)y;
    }
    else if (x->suffix == "u32") {
        unsigned long y = strtoul(ptr, &end, 0);
        if (*end != 0)
            error("invalid uint32 literal");
        if (errno == ERANGE)
            error("uint32 literal out of range");
        vh = new ValueHolder(uint32Type);
        *((unsigned int *)vh->buf) = (unsigned int)y;
    }
    else if (x->suffix == "u64") {
        unsigned long long y = strtoull(ptr, &end, 0);
        if (*end != 0)
            error("invalid uint64 literal");
        if (errno == ERANGE)
            error("uint64 literal out of range");
        vh = new ValueHolder(uint64Type);
        *((unsigned long long *)vh->buf) = y;
    }
    else if (x->suffix == "f32") {
        float y = (float)strtod(ptr, &end);
        if (*end != 0)
            error("invalid float32 literal");
        if (errno == ERANGE)
            error("float32 literal out of range");
        vh = new ValueHolder(float32Type);
        *((float *)vh->buf) = y;
    }
    else if (x->suffix == "f64") {
        double y = strtod(ptr, &end);
        if (*end != 0)
            error("invalid float64 literal");
        if (errno == ERANGE)
            error("float64 literal out of range");
        vh = new ValueHolder(float64Type);
        *((double *)vh->buf) = y;
    }
    else {
        error("invalid literal suffix: " + x->suffix);
    }
    return vh;
}

ValueHolderPtr parseFloatLiteral(FloatLiteral *x)
{
    char *ptr = const_cast<char *>(x->value.c_str());
    char *end = ptr;
    ValueHolderPtr vh;
    if (x->suffix == "f32") {
        float y = (float)strtod(ptr, &end);
        if (*end != 0)
            error("invalid float32 literal");
        if (errno == ERANGE)
            error("float32 literal out of range");
        vh = new ValueHolder(float32Type);
        *((float *)vh->buf) = y;
    }
    else if ((x->suffix == "f64") || x->suffix.empty()) {
        double y = strtod(ptr, &end);
        if (*end != 0)
            error("invalid float64 literal");
        if (errno == ERANGE)
            error("float64 literal out of range");
        vh = new ValueHolder(float64Type);
        *((double *)vh->buf) = y;
    }
    else {
        error("invalid float literal suffix: " + x->suffix);
    }
    return vh;
}
