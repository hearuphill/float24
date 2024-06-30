#include <iostream>
#include <cmath>
#include <bitset>
#include <string>
#include <sstream>

/** https://evanw.github.io/float-toy/
保证精度都是float32的子集，因此float24可以安全转换为float32

     1  符号位     sign
     7   阶码  exponent
    16   尾数  mantissa

    精度: log_10⁡{(2^(16+1)} := 5.1175        (1-63=-62) 17位尾数精度为5.4
    范围: -Infinity | -2*2^63 | -1*2^-62 | 0 | 1*2^-62 | 2*2^63 | +Infinity
*/
class Float24
{
private:
    using uint7_t = uint8_t; // 最高位不用，但运行时需保证运算时正确
    uint8_t sign_exponent;   // 不能直接设置
    uint16_t mantissa;       // 可以直接设置

    // 7bits 常量
    static const uint7_t exponent_max = static_cast<uint8_t>(0b1111111);  // 127
    static const uint7_t exponent_min = static_cast<uint8_t>(0b0000000);  // 0
    static const uint7_t exponent_bias = static_cast<uint8_t>(0b0111111); // 63

public:
    // 无参构造器，返回的是：正零
    explicit Float24() : sign_exponent(0), mantissa(0) {}
    // 给定二进制构造
    explicit Float24(bool sign, uint7_t exponent, uint16_t mantissa)
    {
        setSign(sign);
        setExponent(exponent);
        setMantissa(mantissa);
    }
    inline Float24 clone() const
    {
        Float24 f;
        f.sign_exponent = sign_exponent;
        f.mantissa = mantissa;
        return f;
    }

    /** 0 为正，1 为负 */
    bool inline getSign() const { return sign_exponent >> 7; }
    uint16_t inline getMantissa() const { return mantissa; }
    uint7_t inline getExponent() const { return sign_exponent & 0b01111111; } // 消除符号位即可

    inline std::string toBinaryString() const
    {
        uint32_t n = mantissa;
        n |= sign_exponent << 16;
        return std::bitset<24>(n).to_string();
    }
    /** 返回二进制可读表示格式 */
    inline std::string toPrettyString() const
    {
        std::string sign = getSign() ? "-" : "+";
        if (isInfinity())
            return sign + "Infinity";
        if (isNaN())
            return sign + "NaN";
        std::stringstream ss;
        ss << "(-1)^" << (int)(getSign()) << " * " << "2^(";
        if (isDenormalized())
        {
            ss << "1-" << (int)(exponent_bias) << ")";
            ss << " * " << "0.";
            ss << std::bitset<16>(getMantissa()).to_string();
        }
        else
        {
            ss << (int)(getExponent()) << "-" << (int)exponent_bias << ")";
            ss << " * " << "1.";
            ss << std::bitset<16>(getMantissa()).to_string();
        }
        ss << " = " << toFloat();
        return ss.str();
    }

    void inline setSign(bool sign)
    {
        if (getSign() == sign)
            return;                  // 符号位相同，不改变
        sign_exponent ^= 0b10000000; // 符号位取反
    }
    void inline setMantissa(uint16_t mantissa) { this->mantissa = mantissa; }
    static void inline checkExponent(uint7_t exponent)
    {
        if (exponent & 0b10000000)
            throw std::invalid_argument("exponent should be 7bits");
    }
    void inline setExponent(uint7_t exponent)
    {
        checkExponent(exponent);
        sign_exponent &= 0b10000000; // 保留符号位
        sign_exponent |= exponent;   // 设置阶码
    }
    /** @return 是否是 NaN */
    bool inline isNaN() const
    {
        return getExponent() == exponent_max // 阶码全 1
               && getMantissa() != 0;        // 尾数不为 0
    }
    /** @return 是否是静默 NaN */
    bool inline isQNaN() const
    {
        return isNaN() && // 尾数最高位为 1
               getMantissa() & 0b1000000000000000;
    }
    /** @return 是否是正负无穷大 */
    bool inline isInfinity() const
    {
        return getExponent() == exponent_max // 阶码全 1
               && getMantissa() == 0;        // 尾数为 0
    }
    /** @return 是否是正负零 */
    bool inline isZero() const
    {
        return getExponent() == 0     // 阶码为 0
               && getMantissa() == 0; // 尾数为 0
    }
    /** @return 是否是非规格化数 */
    bool inline isDenormalized() const
    {
        return getExponent() == 0     // 阶码为 0
               && getMantissa() != 0; // 尾数不为 0
    }

    /** @return Float24 正无限大，注意符号位为0 */
    static Float24 inline infinity()
    {
        Float24 f;
        f.setExponent(Float24::exponent_max);
        return f;
    }
    /** @return 静默NaN */
    static Float24 inline qNaN()
    {
        Float24 f;
        f.setExponent(Float24::exponent_max);
        f.setMantissa(0b1000000000000000); // 1 << 16
        return f;
    }

    // 单精度浮点数转换为 Float24，注意会可能丢失精度
    explicit Float24(float value)
    {
        uint32_t bits = *reinterpret_cast<uint32_t *>(&value);
        bool sign = bits >> 31;
        uint8_t exponent = (bits >> 23) & 0xFF; // 8 位阶码
        uint32_t mantissa = bits & 0x7FFFFF;    // 23 位尾数

        setSign(sign);

        if (exponent == 0xFF)
        { //  NaN or Infinity
            setExponent(exponent_max);
            setMantissa(mantissa ? 0xFFFF : 0);
        }
        else if (exponent == 0)
        { // 单精度非规格化数，无法表示为 f24，故转化为 0
            setExponent(0);
            setMantissa(0);
        }
        else
        {
            // 127 为 f32 bias
            int new_exponent = (int)exponent - 127 + (int)exponent_bias;
            if (new_exponent >= (int)exponent_max)
            { // 超过最大值，无法表示，故为 Infinity
                setExponent(exponent_max);
                setMantissa(0);
            }
            else if (new_exponent <= 0)
            { // 超过最小值，无法表示，故为 0
                setExponent(0);
                setMantissa(0);
            }
            else
            {
                setExponent(new_exponent);
                setMantissa(mantissa >> (23 - 16));
            }
        }
    }

    // 不会丢失精度
    float toFloat() const
    {
        uint32_t sign = getSign() ? 1 : 0; // 1
        uint32_t exponent = getExponent(); // 7
        uint32_t mantissa = getMantissa(); // 16

        if (exponent == exponent_max)
        { // 转换正负 Infinity 或 NaN
            exponent = 0xFF;
            mantissa = mantissa ? 0x7FFFFF : 0;
        }
        else if (exponent == 0)
        {
            if (mantissa == 0)
            { // 正负 0
                exponent = 0;
                mantissa = 0;
            }
            else
            { // 非规格化数
                while ((mantissa & (1 << 16)) == 0)
                {
                    mantissa <<= 1;
                    exponent--;
                }
                mantissa &= ~(1 << 16); // 移除前导  1
                exponent = 1;           // 非规格化数指数位为 1
            }
        }
        else
        {
            exponent = exponent - exponent_bias + 127;
            mantissa = mantissa << 7;
        }
        uint32_t bits = (sign << 31) | (exponent << 23) | mantissa;
        return *reinterpret_cast<float *>(&bits);
    }

    Float24 operator+(const Float24 &other) const; // { return Float24(this->toFloat() + other.toFloat()); }
    Float24 operator-(const Float24 &other) const; // { return Float24(this->toFloat() - other.toFloat()); }
    Float24 operator*(const Float24 &other) const { return Float24(this->toFloat() * other.toFloat()); }
    Float24 operator/(const Float24 &other) const { return Float24(this->toFloat() / other.toFloat()); }
};

Float24 Float24::operator+(const Float24 &other) const
{
    // NaN + any = NaN
    if (this->isNaN() || other.isNaN())
        return qNaN();

    // 0 + any = any
    if (this->isZero())
        return other.clone();
    if (other.isZero())
        return this->clone();

    // Infinity + what = ?
    static auto infPlusWhat = [&](Float24 inf, Float24 what) -> Float24
    {
        if (what.isInfinity())
        {
            if (inf.getSign() == what.getSign())
                return Float24(inf);
            return qNaN(); // 正无穷 + 负无穷 = NaN
        }
        else
        {
            return Float24(inf); // 否则，返回符号位不变的无穷
        }
    };

    if (this->isInfinity())
        return infPlusWhat(*this, other);
    if (other.isInfinity())
        return infPlusWhat(other, *this);

    uint32_t f1_sign = this->getSign();
    uint32_t f2_sign = other.getSign();
    uint16_t f1_exp = this->getExponent();
    uint16_t f2_exp = other.getExponent();
    uint16_t r_exp;
    uint32_t f1_mant = this->getMantissa();
    uint32_t f2_mant = other.getMantissa();

    // 对于规格化数，添加隐含的前导 1
    if (f1_exp != 0)
        f1_mant |= (1 << 16);
    if (f2_exp != 0)
        f2_mant |= (1 << 16);

    if (f1_exp > f2_exp)
    { // 指数较小的那个浮点数的尾数向右移动
        f2_mant >>= f1_exp - f2_exp;
        r_exp = f1_exp;
    }
    else
    {
        f1_mant >>= f2_exp - f1_exp;
        r_exp = f2_exp;
    }

    uint32_t r_mant;
    uint32_t r_sign = f1_sign;

    if (f1_sign ^ f2_sign)
    { // 符号位不同，减法
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
        else // 抵消
            return Float24(r_sign, 0, 0);

        while (r_mant < (1 << 16) && r_exp > 0)
        { // 对阶
            r_mant <<= 1;
            r_exp--;
        }
    }
    else
    { // 符号位相同，加法
        r_mant = f1_mant + f2_mant;
        if (r_mant & (1 << 17))
        {
            r_mant >>= 1;
            r_exp++;
        }
    }

    // 对于规格化数，移除前导 1
    if (r_exp != 0)
        r_mant &= ~(1 << 16);

    return Float24(r_sign, r_exp, r_mant);
}

Float24 Float24::operator-(const Float24 &other) const
{
    Float24 neg_other = other.clone();
    neg_other.setSign(!other.getSign());
    return *this + neg_other;
}
