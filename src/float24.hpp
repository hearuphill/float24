#ifndef FLOAT24_HPP
#define FLOAT24_HPP

// 本实现必须在小端CPU上运行，低字节存储在低地址
#define IS_LITTLE_ENDIAN (*(uint16_t *)"\0\xff" < 0x100)

#define CHOPMASK ((1UL << 24) - 1)
#define F32_SIGN 0b10000000000000000000000000000000
#define F32_EXP 0b01111111100000000000000000000000
#define F32_MANT 0b00000000011111111111111111111111

#define F24_SIGN 0b100000000000000000000000
#define F24_EXP 0b011111100000000000000000
#define F24_MANT 0b000000011111111111111111
#define F24_NINF 0b111111100000000000000000
#define F24_PINF 0b011111100000000000000000
#define F24_NAN 0b111111111111111111111111
#define F24_MANT_PREP 0b000000100000000000000000
#define F24_EXP_CARRY 0b000001000000000000000000

#define F32TOF24_SIGN 0b10000000000000000000000000000000
#define F32TOF24_EXP 0b01111110000000000000000000000000
#define F32TOF24_MANT 0b00000000011111111111111111000000
#define F32TOF24_MANT_OFFSET 2

#include <cmath>
#include <cstring>
#include <iostream>

class Float24
{
private:
    uint8_t value[3];
    // 内存布局：0 1 2 三个字节按地址从低到高存储
    // 字节对齐：一旦对齐 4 个字节，3 号字节存储在 2 号字节之后（最高地址）
    // 小端字节序：低字节存储在低地址，因此当 value 指针强转为 uint32_t* 时，保证能够正确解释

    uint32_t *value_as_uint32_ptr()
    {
        return (uint32_t *)value;
    }
    uint32_t value_as_uint32_safe(const uint8_t value[3])
    { // 安全转换法
        uint32_t result = 0;
        if (IS_LITTLE_ENDIAN)
        { // 小端
            result |= value[0];
            result |= value[1] << 8;
            result |= value[2] << 16;
        }
        else
        { // 大端
            result |= value[2];
            result |= value[1] << 8;
            result |= value[0] << 16;
        }
        return result;
    }

    int8_t getExponent()
    {
        int8_t copy = (value[2] & 0b01111110);
        copy >>= 1;
        return copy - 31;
    }

    /** 牛顿迭代法 */
    Float24 newtonDivision(Float24 guess, Float24 divider);

public:
    Float24 operator+(Float24 f2);
    Float24 operator-(Float24 f2);
    Float24 operator-(); // 取反
    Float24 operator/(Float24 f2);
    Float24 operator*(Float24 f2);
    bool operator==(Float24 f2);
    void operator+=(Float24 f2);
    void operator-=(Float24 f2);
    void operator*=(Float24 f2);
    void operator/=(Float24 f2);
    bool equals(Float24 f2, int precision);
    float toFloat32();
    Float24(float number); // fromFloat32
    static inline Float24 fromFloat32(float number) { return Float24(number); }
    Float24() { std::memset(value, 0, 3); };
    friend std::ostream &operator<<(std::ostream &cout, Float24 &obj);
};

Float24 Float24::operator+(Float24 f2)
{
    uint32_t f1i = *(uint32_t *)value;
    uint32_t f2i = *(uint32_t *)&f2;
    uint32_t f_ret = 0;
    f1i &= CHOPMASK;
    f2i &= CHOPMASK;

    if (f1i == 0)
        return (*(Float24 *)&f2i);
    else if (f2i == 0)
        return (*(Float24 *)&f1i);

    uint32_t f1_sign = (f1i & F24_SIGN);
    uint32_t f2_sign = (f2i & F24_SIGN);
    int8_t f1_exp = getExponent();
    int8_t f2_exp = f2.getExponent();
    int8_t r_exp;
    uint32_t f1_mant = (f1i & F24_MANT) | F24_MANT_PREP;
    uint32_t f2_mant = (f2i & F24_MANT) | F24_MANT_PREP;

    if (f1_exp > f2_exp)
    {
        f2_mant >>= std::abs(f1_exp - f2_exp);
        r_exp = f1_exp;
    }
    else
    {
        r_exp = f2_exp;
        f1_mant >>= std::abs(f2_exp - f1_exp);
    }

    uint32_t r_mant;
    uint32_t r_sign = f1_sign;

    if (f1_sign ^ f2_sign)
    {
        if (f1_mant > f2_mant)
        {
            r_sign = f1_sign;
            r_mant = f1_mant - f2_mant;
        }
        else if (f2_mant > f1_mant)
        {
            r_mant = f2_mant - f1_mant;
            r_sign = f2_sign;
        }
        else
            return 0;
        while (r_mant < 131072)
        {
            r_mant <<= 1;
            r_exp--;
        }
    }
    else
    {
        r_mant = f1_mant + f2_mant;
        if (r_mant & F24_EXP_CARRY)
        {
            r_mant >>= 1;
            r_exp++;
        }
    }

    f_ret |= (r_mant & F24_MANT);
    f_ret |= (r_exp + 31) << 17;
    f_ret |= r_sign;
    return (*(Float24 *)&f_ret);
}

Float24 Float24::operator-(Float24 f2)
{
    *(uint32_t *)&f2 ^= 1UL << 23;
    return *this + f2;
}

Float24 Float24::operator-()
{
    *(uint32_t *)this ^= F24_SIGN;
    return *this;
}

Float24 Float24::operator/(Float24 f2)
{
    if (*(uint32_t *)&f2 & F24_SIGN)
        return *this * -newtonDivision(0.1f, -f2);
    return *this * newtonDivision(0.1f, f2);
}

Float24 Float24::operator*(Float24 f2)
{
    uint32_t f1i = *(uint32_t *)value;
    uint32_t f2i = *(uint32_t *)&f2;
    uint32_t f_ret = 0;
    f1i &= CHOPMASK;
    f2i &= CHOPMASK;

    if (f1i == 0 || f2i == 0)
        return 0;

    uint32_t f1_sign = (f1i & F24_SIGN);
    uint32_t f2_sign = (f2i & F24_SIGN);
    uint32_t r_sign = f1_sign ^ f2_sign;
    int8_t f1_exp = getExponent();
    int8_t f2_exp = f2.getExponent();
    uint32_t f1_mant = (f1i & F24_MANT);
    uint32_t f2_mant = (f2i & F24_MANT);
    uint64_t r_mant;
    int8_t r_exp = f1_exp + f2_exp;
    if (f2_mant == 0)
        r_mant = f1_mant;
    else
    {
        uint64_t t = f1_mant | F24_MANT_PREP;
        uint64_t t2 = f2_mant | F24_MANT_PREP;
        r_mant = t * t2;
        r_mant >>= 17;
        if (r_mant & F24_EXP_CARRY)
        {
            r_mant >>= 1;
            r_exp++;
        }
    }
    if (r_exp > 31)
        return F24_PINF;
    if (r_exp < -32)
        return 0;
    f_ret |= (r_mant & F24_MANT);
    f_ret |= (((uint32_t)r_exp + 31) << 17) & F24_EXP;
    f_ret |= r_sign;
    return (*(Float24 *)&f_ret);
}

bool Float24::operator==(Float24 f2) { return *(uint32_t *)this == *(uint32_t *)&f2; }

void Float24::operator+=(Float24 f2) { *this = *this + f2; }

void Float24::operator-=(Float24 f2) { *this = *this - f2; }

void Float24::operator/=(Float24 f2) { *this = *this / f2; }

void Float24::operator*=(Float24 f2) { *this = *this * f2; }

bool Float24::equals(Float24 f2, int precision)
{
    Float24 copy = *this;
    Float24 delta = copy - f2;
    return delta.getExponent() <= -(precision * 4);
}

Float24::Float24(float number)
{
    std::memset(value, 0, 3);
    uint32_t *f32 = (uint32_t *)&number;
    uint32_t *f24i = (uint32_t *)value;
    uint32_t sign = *f32 & F32_SIGN;
    int32_t exp = ((*f32 & F32_EXP) >> 23) - 127 + 31; // 128 half of int8, 32 half of int6
    if (exp == -96)                                    // handle float point zero on exponent 00000000 -> -128
        exp = 0;
    uint32_t mant = *f32 & F32_MANT;
    *f24i |= sign >> 8;
    *f24i |= exp << 17;
    *f24i |= ((mant & F32TOF24_MANT) << F32TOF24_MANT_OFFSET) >> 8;
}

Float24 Float24::newtonDivision(Float24 guess, Float24 divider)
{
    for (int i = 0; i < 10; ++i)
    {
        guess = guess * (Float24(2) - divider * guess);
    }
    return guess;
}

float Float24::toFloat32()
{
    float result = 0;
    uint32_t *f32 = (uint32_t *)&result;
    uint32_t f24i = *(uint32_t *)value;
    f24i &= CHOPMASK;
    if (f24i == 0)
        return 0;
    *f32 |= (f24i & F24_SIGN) >> 8;
    int8_t ex = getExponent();
    *f32 |= (ex + 127) << 7;
    *f32 <<= 16;
    *f32 |= (f24i & F24_MANT) << 6;
    return result;
}

std::ostream &operator<<(std::ostream &cout, Float24 &obj)
{
    cout << obj.toFloat32();
    return cout;
}

#endif