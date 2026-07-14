#include "calculator_tool.h"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace oop_agent::tools {
namespace {

class ExpressionParser {
  public:
    explicit ExpressionParser(std::string_view input) : input_(input) {}

    double parse() {
        const double value = parseExpression();
        skipSpaces();
        if (!isAtEnd()) {
            throw std::runtime_error("unexpected token at position " +
                                     std::to_string(position_));
        }
        return value;
    }

  private:
    // Grammar:
    // expression = term (("+" | "-") term)*
    // term       = factor (("*" | "/") factor)*
    // factor     = number | "(" expression ")" | ("+" | "-") factor
    double parseExpression() {
        double value = parseTerm();
        while (true) {
            skipSpaces();
            if (match('+')) {
                value += parseTerm();
            } else if (match('-')) {
                value -= parseTerm();
            } else {
                return value;
            }
        }
    }

    double parseTerm() {
        double value = parseFactor();
        while (true) {
            skipSpaces();
            if (match('*')) {
                value *= parseFactor();
            } else if (match('/')) {
                const double divisor = parseFactor();
                if (std::abs(divisor) < 1e-12) {
                    throw std::runtime_error("division by zero");
                }
                value /= divisor;
            } else {
                return value;
            }
        }
    }

    double parseFactor() {
        skipSpaces();
        if (match('+')) {
            return parseFactor();
        }
        if (match('-')) {
            return -parseFactor();
        }
        if (match('(')) {
            const double value = parseExpression();
            skipSpaces();
            if (!match(')')) {
                throw std::runtime_error("missing closing parenthesis");
            }
            return value;
        }
        return parseNumber();
    }

    double parseNumber() {
        skipSpaces();
        const std::size_t start = position_;
        bool has_digit = false;

        while (!isAtEnd() &&
               std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            has_digit = true;
            ++position_;
        }

        if (!isAtEnd() && input_[position_] == '.') {
            ++position_;
            while (!isAtEnd() &&
                   std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                has_digit = true;
                ++position_;
            }
        }

        if (!has_digit) {
            throw std::runtime_error("expected number at position " +
                                     std::to_string(start));
        }

        return std::stod(std::string(input_.substr(start, position_ - start)));
    }

    bool match(char expected) {
        if (!isAtEnd() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void skipSpaces() {
        while (!isAtEnd() &&
               std::isspace(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    bool isAtEnd() const noexcept {
        return position_ >= input_.size();
    }

    std::string_view input_;
    std::size_t position_{0};
};

std::string formatNumber(double value) {
    if (!std::isfinite(value)) {
        throw std::runtime_error("calculation result is not finite");
    }

    std::ostringstream output;
    output << std::setprecision(15) << value;
    return output.str();
}

} // namespace

std::string_view CalculatorTool::name() const noexcept {
    return "calculator";
}

std::string_view CalculatorTool::description() const noexcept {
    return "Evaluate a basic arithmetic expression. Argument: expression.";
}

ToolResult CalculatorTool::execute(const ToolArguments &arguments) {
    const auto expression_argument = arguments.find("expression");
    if (expression_argument == arguments.end() ||
        expression_argument->second.empty()) {
        return ToolResult::failed(
            "calculator requires a non-empty 'expression' argument");
    }

    try {
        ExpressionParser parser(expression_argument->second);
        return ToolResult::ok(formatNumber(parser.parse()));
    } catch (const std::exception &error) {
        return ToolResult::failed(std::string("calculator failed: ") +
                                  error.what());
    }
}

} // namespace oop_agent::tools
