#include "float24.hpp"

Float24 Float24::operator*(const Float24 &other) const
{
    // NaN * any = NaN
    if (this->isNaN() || other.isNaN())
        return qNaN();

    // 0 * any = 0
    if (this->isZero() || other.isZero())
        return Float24();

    // Infinity * 0 = NaN
    if ((this->isInfinity() && other.isZero()) || (this->isZero() && other.isInfinity()))
        return qNaN();

    // Infinity * any = Infinity
    if (this->isInfinity() || other.isInfinity())
    {
        bool sign = this->getSign() ^ other.getSign();
        return Float24(sign, exponent_max, 0);
    }

    // Get the sign, exponent, and mantissa of both numbers
    bool sign1 = this->getSign();
    bool sign2 = other.getSign();
    uint8_t exp1 = this->getExponent();
    uint8_t exp2 = other.getExponent();
    uint32_t mant1 = this->getMantissa();
    uint32_t mant2 = other.getMantissa();

    // Add implicit leading 1 for normalized numbers
    if (exp1 != 0)
        mant1 |= (1 << 16);
    if (exp2 != 0)
        mant2 |= (1 << 16);

    // Calculate the new sign
    bool result_sign = sign1 ^ sign2;

    // Calculate the new exponent
    int result_exp = exp1 + exp2 - exponent_bias;

    // Calculate the new mantissa
    uint64_t result_mant = (uint64_t)mant1 * (uint64_t)mant2;

    // Normalize the result
    if (result_mant & (1ULL << 33))
    {
        result_mant >>= 17;
        result_exp++;
    }
    else
    {
        result_mant >>= 16;
    }

    // Handle overflow and underflow
    if (result_exp >= exponent_max)
    {
        return Float24(result_sign, exponent_max, 0); // Infinity
    }
    else if (result_exp <= 0)
    {
        return Float24(result_sign, 0, 0); // Zero
    }

    // Remove the implicit leading 1
    result_mant &= ~(1 << 16);

    return Float24(result_sign, result_exp, (uint16_t)result_mant);
}

Float24 Float24::operator/(const Float24 &other) const
{
    // NaN / any = NaN
    if (this->isNaN() || other.isNaN())
        return qNaN();

    // any / 0 = Infinity
    if (other.isZero())
    {
        if (this->isZero())
            return qNaN();                                                  // 0 / 0 = NaN
        return Float24(this->getSign() ^ other.getSign(), exponent_max, 0); // Infinity
    }

    // 0 / any = 0
    if (this->isZero())
        return Float24();

    // Infinity / any = Infinity
    if (this->isInfinity())
    {
        if (other.isInfinity())
            return qNaN();                                                  // Infinity / Infinity = NaN
        return Float24(this->getSign() ^ other.getSign(), exponent_max, 0); // Infinity
    }

    // Get the sign, exponent, and mantissa of both numbers
    bool sign1 = this->getSign();
    bool sign2 = other.getSign();
    uint8_t exp1 = this->getExponent();
    uint8_t exp2 = other.getExponent();
    uint32_t mant1 = this->getMantissa();
    uint32_t mant2 = other.getMantissa();

    // Add implicit leading 1 for normalized numbers
    if (exp1 != 0)
        mant1 |= (1 << 16);
    if (exp2 != 0)
        mant2 |= (1 << 16);

    // Calculate the new sign
    bool result_sign = sign1 ^ sign2;

    // Calculate the new exponent
    int result_exp = exp1 - exp2 + exponent_bias;

    // Calculate the new mantissa
    uint64_t result_mant = ((uint64_t)mant1 << 32) / mant2;

    // Normalize the result
    if (result_mant & (1ULL << 17))
    {
        result_mant >>= 1;
        result_exp++;
    }

    // Handle overflow and underflow
    if (result_exp >= exponent_max)
    {
        return Float24(result_sign, exponent_max, 0); // Infinity
    }
    else if (result_exp <= 0)
    {
        return Float24(result_sign, 0, 0); // Zero
    }

    // Remove the implicit leading 1
    result_mant &= ~(1 << 16);

    return Float24(result_sign, result_exp, (uint16_t)result_mant);
}
