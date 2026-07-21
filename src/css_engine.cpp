/**
 * @file css_engine.cpp
 * @brief CSS Stylesheet Inspector for Mobile DevTools (WASM)
 * 
 * Parse and inspect CSS stylesheets with rule extraction, property analysis,
 * and selector matching.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// 1. CSS STRUCTURES
// ==========================================

struct CSSProperty {
    std::string name;
    std::string value;
    bool important;
};

struct CSSRule {
    std::string id;
    std::string selector;
    std::vector<CSSProperty> properties;
    int specificity; // Simple specificity score
    bool isMediaQuery;
    std::string mediaQuery;
    int lineNumber;
};

struct CSSStylesheet {
    std::string id;
    std::string href;
    std::string content;
    bool isInline;
    bool isDisabled;
    size_t ruleCount;
    std::vector<CSSRule> rules;
};

// ==========================================
// 2. CSS PARSER
// ==========================================

class CSSParser {
public:
    static std::vector<CSSRule> parseRules(const std::string& css) {
        std::vector<CSSRule> rules;
        std::string current = css;
        size_t pos = 0;
        int lineNumber = 1;
        int ruleId = 0;

        while (pos < current.length()) {
            // Skip whitespace and comments
            pos = skipWhitespaceAndComments(current, pos, lineNumber);
            if (pos >= current.length()) break;

            // Find selector (everything before {)
            size_t selectorEnd = current.find('{', pos);
            if (selectorEnd == std::string::npos) break;

            std::string selector = current.substr(pos, selectorEnd - pos);
            selector = trim(selector);
            pos = selectorEnd + 1;

            // Find properties (everything until })
            size_t propsEnd = findMatchingBrace(current, pos);
            if (propsEnd == std::string::npos) break;

            std::string propsStr = current.substr(pos, propsEnd - pos);
            std::vector<CSSProperty> properties = parseProperties(propsStr);

            bool isMediaQuery = (selector.find("@media") != std::string::npos);
            int specificity = calculateSpecificity(selector);

            rules.push_back({
                "rule_" + std::to_string(ruleId++),
                selector,
                properties,
                specificity,
                isMediaQuery,
                isMediaQuery ? selector : "",
                lineNumber
            });

            pos = propsEnd + 1;
        }

        return rules;
    }

private:
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    static size_t skipWhitespaceAndComments(const std::string& str, size_t pos, int& lineNumber) {
        while (pos < str.length()) {
            if (std::isspace(str[pos])) {
                if (str[pos] == '\n') lineNumber++;
                pos++;
            } else if (str[pos] == '/' && pos + 1 < str.length() && str[pos + 1] == '*') {
                // Skip comment
                pos += 2;
                while (pos + 1 < str.length()) {
                    if (str[pos] == '*' && str[pos + 1] == '/') {
                        pos += 2;
                        break;
                    }
                    if (str[pos] == '\n') lineNumber++;
                    pos++;
                }
            } else {
                break;
            }
        }
        return pos;
    }

    static size_t findMatchingBrace(const std::string& str, size_t start) {
        int depth = 1;
        size_t pos = start;
        while (pos < str.length() && depth > 0) {
            if (str[pos] == '{') depth++;
            else if (str[pos] == '}') depth--;
            if (depth == 0) return pos;
            pos++;
        }
        return std::string::npos;
    }

    static std::vector<CSSProperty> parseProperties(const std::string& propsStr) {
        std::vector<CSSProperty> properties;
        std::string current = propsStr;
        size_t pos = 0;

        while (pos < current.length()) {
            // Find property name (before :)
            size_t colonPos = current.find(':', pos);
            if (colonPos == std::string::npos) break;

            std::string name = trim(current.substr(pos, colonPos - pos));
            if (name.empty()) break;

            // Find property value (before ; or end)
            size_t semiPos = current.find(';', colonPos);
            if (semiPos == std::string::npos) semiPos = current.length();

            std::string value = trim(current.substr(colonPos + 1, semiPos - colonPos - 1));

            bool important = (value.find("!important") != std::string::npos);
            if (important) {
                value = trim(value.substr(0, value.find("!important")));
            }

            properties.push_back({name, value, important});
            pos = semiPos + 1;
        }

        return properties;
    }

    static int calculateSpecificity(const std::string& selector) {
        int score = 0;
        // Rough specificity: ID(100) > class(10) > element(1)
        for (char c : selector) {
            if (c == '#') score += 100;
            else if (c == '.') score += 10;
        }
        score += 1; // Base for element selector
        return score;
    }
};

// ==========================================
// 3. CSS ENGINE
// ==========================================

class CSSEngine {
private:
    std::vector<CSSStylesheet> stylesheets;
    static const size_t MAX_STYLESHEETS = 100;
    size_t stylesheetIdCounter = 0;

    std::string generateId() {
        return "css_" + std::to_string(stylesheetIdCounter++);
    }

public:
    CSSEngine() {
        stylesheets.reserve(MAX_STYLESHEETS);
    }

    // Add stylesheet from external source
    std::string addStylesheet(const std::string& href, const std::string& content, bool isInline = false) {
        if (stylesheets.size() >= MAX_STYLESHEETS) {
            stylesheets.erase(stylesheets.begin());
        }

        std::string id = generateId();
        std::vector<CSSRule> rules = CSSParser::parseRules(content);

        stylesheets.push_back({
            id,
            href,
            content,
            isInline,
            false,
            rules.size(),
            rules
        });

        return id;
    }

    bool updateStylesheet(const std::string& stylesheetId, const std::string& newContent) {
        for (auto& sheet : stylesheets) {
            if (sheet.id == stylesheetId) {
                sheet.content = newContent;
                sheet.rules = CSSParser::parseRules(newContent);
                sheet.ruleCount = sheet.rules.size();
                return true;
            }
        }
        return false;
    }

    bool removeStylesheet(const std::string& stylesheetId) {
        for (auto it = stylesheets.begin(); it != stylesheets.end(); ++it) {
            if (it->id == stylesheetId) {
                stylesheets.erase(it);
                return true;
            }
        }
        return false;
    }

    // Get all stylesheets
    const std::vector<CSSStylesheet>& getStylesheets() const {
        return stylesheets;
    }

    // Get rules from specific stylesheet
    std::vector<CSSRule> getRulesFromStylesheet(const std::string& stylesheetId) const {
        for (const auto& sheet : stylesheets) {
            if (sheet.id == stylesheetId) {
                return sheet.rules;
            }
        }
        return {};
    }

    // Get all rules
    std::vector<CSSRule> getAllRules() const {
        std::vector<CSSRule> allRules;
        for (const auto& sheet : stylesheets) {
            allRules.insert(allRules.end(), sheet.rules.begin(), sheet.rules.end());
        }
        return allRules;
    }

    // Find rules that match a selector pattern
    std::vector<CSSRule> findRulesBySelector(const std::string& selectorPattern) const {
        std::vector<CSSRule> matching;
        std::string lowerPattern = selectorPattern;
        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);

        for (const auto& sheet : stylesheets) {
            for (const auto& rule : sheet.rules) {
                std::string lowerSelector = rule.selector;
                std::transform(lowerSelector.begin(), lowerSelector.end(), lowerSelector.begin(), ::tolower);
                if (lowerSelector.find(lowerPattern) != std::string::npos) {
                    matching.push_back(rule);
                }
            }
        }
        return matching;
    }

    // Find rules with specific property
    std::vector<CSSRule> findRulesByProperty(const std::string& propName) const {
        std::vector<CSSRule> matching;
        std::string lowerName = propName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        for (const auto& sheet : stylesheets) {
            for (const auto& rule : sheet.rules) {
                for (const auto& prop : rule.properties) {
                    std::string lowerPropName = prop.name;
                    std::transform(lowerPropName.begin(), lowerPropName.end(), lowerPropName.begin(), ::tolower);
                    if (lowerPropName.find(lowerName) != std::string::npos) {
                        matching.push_back(rule);
                        break;
                    }
                }
            }
        }
        return matching;
    }

    // Get stylesheet statistics
    std::map<std::string, size_t> getStats() const {
        std::map<std::string, size_t> stats;
        stats["totalSheets"] = stylesheets.size();
        stats["totalRules"] = 0;
        stats["totalProperties"] = 0;

        for (const auto& sheet : stylesheets) {
            stats["totalRules"] += sheet.ruleCount;
            for (const auto& rule : sheet.rules) {
                stats["totalProperties"] += rule.properties.size();
            }
        }
        return stats;
    }

    void clear() {
        stylesheets.clear();
        stylesheetIdCounter = 0;
    }
};

// ==========================================
// 3. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(css_engine_module) {
    class_<CSSProperty>("CSSProperty")
        .property("name", &CSSProperty::name)
        .property("value", &CSSProperty::value)
        .property("important", &CSSProperty::important);

    register_vector<CSSProperty>("VectorCSSProperty");

    class_<CSSRule>("CSSRule")
        .property("id", &CSSRule::id)
        .property("selector", &CSSRule::selector)
        .property("properties", &CSSRule::properties)
        .property("specificity", &CSSRule::specificity)
        .property("isMediaQuery", &CSSRule::isMediaQuery)
        .property("mediaQuery", &CSSRule::mediaQuery)
        .property("lineNumber", &CSSRule::lineNumber);

    register_vector<CSSRule>("VectorCSSRule");

    class_<CSSStylesheet>("CSSStylesheet")
        .property("id", &CSSStylesheet::id)
        .property("href", &CSSStylesheet::href)
        .property("content", &CSSStylesheet::content)
        .property("isInline", &CSSStylesheet::isInline)
        .property("isDisabled", &CSSStylesheet::isDisabled)
        .property("ruleCount", &CSSStylesheet::ruleCount)
        .property("rules", &CSSStylesheet::rules);

    register_vector<CSSStylesheet>("VectorCSSStylesheet");

    class_<CSSEngine>("CSSEngine")
        .constructor()
        .function("addStylesheet", select_overload<std::string(
            const std::string&,
            const std::string&,
            bool
        )>(&CSSEngine::addStylesheet))
        .function("updateStylesheet", &CSSEngine::updateStylesheet)
        .function("removeStylesheet", &CSSEngine::removeStylesheet)
        .function("getStylesheets", &CSSEngine::getStylesheets)
        .function("getRulesFromStylesheet", &CSSEngine::getRulesFromStylesheet)
        .function("getAllRules", &CSSEngine::getAllRules)
        .function("findRulesBySelector", &CSSEngine::findRulesBySelector)
        .function("findRulesByProperty", &CSSEngine::findRulesByProperty)
        .function("getStats", &CSSEngine::getStats)
        .function("clear", &CSSEngine::clear);
}
#endif
