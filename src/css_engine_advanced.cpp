/**
 * @file css_engine_advanced.cpp
 * @brief Advanced CSS Engine with Combinator Support
 * 
 * Enhanced CSS engine supporting CSS combinators (>, +, ~),
 * pseudo-selectors, attribute selectors, and complex selectors.
 */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// 1. ADVANCED CSS SELECTOR STRUCTURES
// ==========================================

enum class SelectorType {
    ELEMENT,
    ID,
    CLASS,
    ATTRIBUTE,
    PSEUDO_CLASS,
    PSEUDO_ELEMENT,
    UNIVERSAL
};

enum class CombinatorType {
    DESCENDANT,      // space
    CHILD,           // >
    ADJACENT_SIBLING, // +
    GENERAL_SIBLING  // ~
};

struct SimpleSelectorSequence {
    SelectorType type;
    std::string value;
    std::string operator_; // For attributes: =, ~=, |=, ^=, $=, *=
    std::string operand;   // For attributes: value to match
};

struct ComplexSelector {
    std::vector<SimpleSelectorSequence> selectors;
    std::vector<CombinatorType> combinators; // Between consecutive selectors
    int specificity;
};

struct AdvancedCSSRule {
    std::string id;
    ComplexSelector selector;
    std::vector<std::pair<std::string, std::string>> properties; // {name, value}
    int lineNumber;
    bool isMediaQuery;
    std::string mediaQuery;
};

// ==========================================
// 2. ADVANCED SELECTOR PARSER
// ==========================================

class AdvancedSelectorParser {
public:
    static ComplexSelector parseSelector(const std::string& selectorStr) {
        ComplexSelector selector = {{}, {}, 0};
        std::string current = trim(selectorStr);
        std::vector<std::string> parts;

        // Split by combinators while preserving them
        size_t i = 0;
        std::string currentPart = "";
        while (i < current.length()) {
            char c = current[i];

            // Check for combinators
            if ((c == '>' || c == '+' || c == '~') && !currentPart.empty()) {
                parts.push_back(trim(currentPart));
                parts.push_back(std::string(1, c));
                currentPart = "";
                i++;
            } else if (c == ' ' && !currentPart.empty()) {
                // Descendant combinator
                parts.push_back(trim(currentPart));
                currentPart = "";
                i++;
                // Skip multiple spaces
                while (i < current.length() && current[i] == ' ') i++;
                if (i < current.length()) parts.push_back(" ");
            } else {
                currentPart += c;
                i++;
            }
        }
        if (!currentPart.empty()) {
            parts.push_back(trim(currentPart));
        }

        // Parse each part
        for (size_t j = 0; j < parts.size(); j++) {
            const auto& part = parts[j];
            if (part == ">" || part == "+" || part == "~" || part == " ") {
                if (part == ">") selector.combinators.push_back(CombinatorType::CHILD);
                else if (part == "+") selector.combinators.push_back(CombinatorType::ADJACENT_SIBLING);
                else if (part == "~") selector.combinators.push_back(CombinatorType::GENERAL_SIBLING);
                else selector.combinators.push_back(CombinatorType::DESCENDANT);
            } else {
                auto simpleSelectors = parseSimpleSelectors(part);
                selector.selectors.insert(selector.selectors.end(),
                                         simpleSelectors.begin(),
                                         simpleSelectors.end());
            }
        }

        selector.specificity = calculateSpecificity(selector);
        return selector;
    }

private:
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    static std::vector<SimpleSelectorSequence> parseSimpleSelectors(const std::string& part) {
        std::vector<SimpleSelectorSequence> selectors;
        std::string current = part;
        size_t i = 0;

        while (i < current.length()) {
            if (current[i] == '#') {
                // ID selector
                i++;
                std::string id = "";
                while (i < current.length() && (std::isalnum(current[i]) || current[i] == '-' || current[i] == '_')) {
                    id += current[i++];
                }
                selectors.push_back({SelectorType::ID, id, "", ""});
            } else if (current[i] == '.') {
                // Class selector
                i++;
                std::string cls = "";
                while (i < current.length() && (std::isalnum(current[i]) || current[i] == '-' || current[i] == '_')) {
                    cls += current[i++];
                }
                selectors.push_back({SelectorType::CLASS, cls, "", ""});
            } else if (current[i] == '[') {
                // Attribute selector
                i++;
                std::string attr = "";
                std::string op = "";
                std::string val = "";
                
                while (i < current.length() && current[i] != ']') {
                    if (current[i] == '=' || (i + 1 < current.length() && (current.substr(i, 2) == "~=" ||
                        current.substr(i, 2) == "|=" || current.substr(i, 2) == "^=" ||
                        current.substr(i, 2) == "$=" || current.substr(i, 2) == "*="))) {
                        
                        if (i + 1 < current.length() && current[i] != '=') {
                            op = current.substr(i, 2);
                            i += 2;
                        } else {
                            op = "=";
                            i++;
                        }
                        
                        // Skip whitespace
                        while (i < current.length() && std::isspace(current[i])) i++;
                        
                        // Get value (may be quoted)
                        if (i < current.length() && (current[i] == '"' || current[i] == '\'')) {
                            char quote = current[i++];
                            while (i < current.length() && current[i] != quote) {
                                val += current[i++];
                            }
                            i++;
                        } else {
                            while (i < current.length() && current[i] != ']') {
                                val += current[i++];
                            }
                        }
                        break;
                    } else {
                        attr += current[i++];
                    }
                }
                i++; // skip ]
                selectors.push_back({SelectorType::ATTRIBUTE, trim(attr), op, val});
            } else if (current[i] == ':') {
                // Pseudo-class or pseudo-element
                i++;
                bool isElement = (i < current.length() && current[i] == ':');
                if (isElement) i++;
                
                std::string pseudo = "";
                while (i < current.length() && (std::isalnum(current[i]) || current[i] == '-')) {
                    pseudo += current[i++];
                }
                
                selectors.push_back({
                    isElement ? SelectorType::PSEUDO_ELEMENT : SelectorType::PSEUDO_CLASS,
                    pseudo, "", ""
                });
            } else if (current[i] == '*') {
                // Universal selector
                selectors.push_back({SelectorType::UNIVERSAL, "*", "", ""});
                i++;
            } else if (std::isalpha(current[i]) || current[i] == '_') {
                // Element selector
                std::string element = "";
                while (i < current.length() && (std::isalnum(current[i]) || current[i] == '-')) {
                    element += current[i++];
                }
                selectors.push_back({SelectorType::ELEMENT, element, "", ""});
            } else {
                i++;
            }
        }

        return selectors;
    }

    static int calculateSpecificity(const ComplexSelector& selector) {
        int score = 0;
        // a = IDs, b = classes/attributes/pseudos, c = elements
        int a = 0, b = 0, c = 0;

        for (const auto& s : selector.selectors) {
            switch (s.type) {
                case SelectorType::ID:
                    a++;
                    break;
                case SelectorType::CLASS:
                case SelectorType::ATTRIBUTE:
                case SelectorType::PSEUDO_CLASS:
                    b++;
                    break;
                case SelectorType::ELEMENT:
                    c++;
                    break;
                default:
                    break;
            }
        }

        score = (a * 100) + (b * 10) + c;
        return score;
    }
};

// ==========================================
// 3. ADVANCED CSS ENGINE
// ==========================================

class CSSEngineAdvanced {
private:
    std::vector<AdvancedCSSRule> rules;
    int ruleIdCounter = 0;

public:
    CSSEngineAdvanced() {
        rules.reserve(10000);
    }

    std::string addRule(
        const std::string& selectorStr,
        const std::vector<std::pair<std::string, std::string>>& properties,
        int lineNumber = -1,
        bool isMediaQuery = false,
        const std::string& mediaQuery = ""
    ) {
        std::string id = "rule_" + std::to_string(ruleIdCounter++);
        ComplexSelector selector = AdvancedSelectorParser::parseSelector(selectorStr);

        rules.push_back({
            id,
            selector,
            properties,
            lineNumber,
            isMediaQuery,
            mediaQuery
        });

        return id;
    }

    // Find rules by various criteria
    std::vector<AdvancedCSSRule> getRulesByElementType(const std::string& element) const {
        std::vector<AdvancedCSSRule> matching;
        std::string lowerElement = element;
        std::transform(lowerElement.begin(), lowerElement.end(), lowerElement.begin(), ::tolower);

        for (const auto& rule : rules) {
            for (const auto& sel : rule.selector.selectors) {
                std::string lowerValue = sel.value;
                std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
                if (sel.type == SelectorType::ELEMENT && lowerValue == lowerElement) {
                    matching.push_back(rule);
                    break;
                }
            }
        }
        return matching;
    }

    std::vector<AdvancedCSSRule> getRulesByClass(const std::string& className) const {
        std::vector<AdvancedCSSRule> matching;
        for (const auto& rule : rules) {
            for (const auto& sel : rule.selector.selectors) {
                if (sel.type == SelectorType::CLASS && sel.value == className) {
                    matching.push_back(rule);
                    break;
                }
            }
        }
        return matching;
    }

    const std::vector<AdvancedCSSRule>& getAllRules() const {
        return rules;
    }

    bool updateRule(
        const std::string& ruleId,
        const std::string& selectorStr,
        const std::vector<std::pair<std::string, std::string>>& properties
    ) {
        for (auto& rule : rules) {
            if (rule.id == ruleId) {
                rule.selector = AdvancedSelectorParser::parseSelector(selectorStr);
                rule.properties = properties;
                return true;
            }
        }
        return false;
    }

    bool removeRule(const std::string& ruleId) {
        for (auto it = rules.begin(); it != rules.end(); ++it) {
            if (it->id == ruleId) {
                rules.erase(it);
                return true;
            }
        }
        return false;
    }

    void clear() {
        rules.clear();
        ruleIdCounter = 0;
    }
};

// ==========================================
// 4. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(css_engine_advanced_module) {
    class_<AdvancedCSSRule>("AdvancedCSSRule")
        .property("id", &AdvancedCSSRule::id)
        .property("lineNumber", &AdvancedCSSRule::lineNumber)
        .property("isMediaQuery", &AdvancedCSSRule::isMediaQuery)
        .property("mediaQuery", &AdvancedCSSRule::mediaQuery);

    register_vector<AdvancedCSSRule>("VectorAdvancedCSSRule");

    class_<CSSEngineAdvanced>("CSSEngineAdvanced")
        .constructor()
        .function("addRule", select_overload<std::string(
            const std::string&,
            const std::vector<std::pair<std::string, std::string>>&,
            int,
            bool,
            const std::string&
        )>(&CSSEngineAdvanced::addRule))
        .function("updateRule", &CSSEngineAdvanced::updateRule)
        .function("removeRule", &CSSEngineAdvanced::removeRule)
        .function("getRulesByElementType", &CSSEngineAdvanced::getRulesByElementType)
        .function("getRulesByClass", &CSSEngineAdvanced::getRulesByClass)
        .function("getAllRules", &CSSEngineAdvanced::getAllRules)
        .function("clear", &CSSEngineAdvanced::clear);
}
#endif
