#include <iostream>
#include <sstream>
#include <stack>
#include <string>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <cmath>
#include "float24.hpp"

// 操作符优先级
int precedence(char op)
{
    if (op == '+' || op == '-')
        return 1;
    if (op == '*' || op == '/')
        return 2;
    return 0;
}

// 应用操作符
Float24 applyOp(Float24 a, Float24 b, char op)
{
    switch (op)
    {
    case '+':
        return a + b;
    case '-':
        return a - b;
    case '*':
        return a * b;
    case '/':
        if (b.isZero())
        {
            throw std::invalid_argument("Division by zero");
        }
        return a / b;
    default:
        throw std::invalid_argument("Invalid operator");
    }
}

// 解析并计算表达式
Float24 evaluate(const std::string &tokens)
{
    std::stack<Float24> values;
    std::stack<char> ops;
    std::istringstream iss(tokens);
    char token;

    while (iss >> token)
    {
        if (std::isdigit(token) || token == '.')
        {
            iss.putback(token);
            float value;
            iss >> value;
            values.push(Float24(value));
        }
        else if (token == '(')
        {
            ops.push(token);
        }
        else if (token == ')')
        {
            while (!ops.empty() && ops.top() != '(')
            {
                if (values.size() < 2)
                {
                    throw std::invalid_argument("Mismatched parentheses or missing operand");
                }
                Float24 val2 = values.top();
                values.pop();
                Float24 val1 = values.top();
                values.pop();
                char op = ops.top();
                ops.pop();
                values.push(applyOp(val1, val2, op));
            }
            if (ops.empty())
            {
                throw std::invalid_argument("Mismatched parentheses");
            }
            ops.pop();
        }
        else if (token == '+' || token == '-' || token == '*' || token == '/')
        {
            while (!ops.empty() && precedence(ops.top()) >= precedence(token))
            {
                if (values.size() < 2)
                {
                    throw std::invalid_argument("Missing operand for operator");
                }
                Float24 val2 = values.top();
                values.pop();
                Float24 val1 = values.top();
                values.pop();
                char op = ops.top();
                ops.pop();
                values.push(applyOp(val1, val2, op));
            }
            ops.push(token);
        }
        else if (!std::isspace(token))
        {
            std::string error = "Invalid character '";
            error += token;
            error += "' in expression";
            throw std::invalid_argument(error);
        }
    }

    while (!ops.empty())
    {
        if (values.size() < 2)
        {
            throw std::invalid_argument("Missing operand for operator");
        }
        Float24 val2 = values.top();
        values.pop();
        Float24 val1 = values.top();
        values.pop();
        char op = ops.top();
        ops.pop();
        values.push(applyOp(val1, val2, op));
    }

    if (values.size() != 1)
    {
        throw std::invalid_argument("Invalid expression");
    }

    return values.top();
}

// REPL 主函数
int main()
{
    std::string line;
    std::cout << "Enter expressions to evaluate or 'exit' to quit:" << std::endl;

    while (true)
    {
        std::cout << "> ";
        std::getline(std::cin, line);

        if (line == "exit")
            break;

        try
        {
            Float24 result = evaluate(line);
            std::cout << "Result: " << result.toFloat() << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    return 0;
}
