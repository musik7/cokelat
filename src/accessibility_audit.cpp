/**
 * @file accessibility_audit.cpp
 * @brief Accessibility Audit Engine for Mobile DevTools
 * 
 * Detect and report accessibility issues for WCAG compliance.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// 1. ACCESSIBILITY AUDIT STRUCTURES
// ==========================================

enum class A11yIssueSeverity {
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

enum class A11yIssueType {
    MISSING_ALT_TEXT,
    LOW_CONTRAST,
    MISSING_LABEL,
    MISSING_ARIA_LABEL,
    KEYBOARD_NOT_ACCESSIBLE,
    NO_HEADING_ORDER,
    MISSING_LANDMARK,
    MISSING_FORM_LABEL,
    INVALID_ARIA_ROLE,
    IMPROPER_HEADING_HIERARCHY,
    MISSING_SKIP_LINK,
    CUSTOM
};

struct A11yIssue {
    std::string id;
    A11yIssueType type;
    A11yIssueSeverity severity;
    std::string elementSelector;
    std::string elementTag;
    std::string message;
    std::string suggestion;
    int wcagLevel; // 1 = A, 2 = AA, 3 = AAA
    bool isFixed; // Whether issue has been addressed
};

struct A11yAuditReport {
    int totalIssues;
    int criticalCount;
    int errorCount;
    int warningCount;
    int infoCount;
    double wcagAaCompliance;  // Percentage
    double wcagAaaCompliance; // Percentage
    std::vector<A11yIssue> issues;
    std::vector<std::string> passedChecks;
};

struct ColorContrast {
    std::string foregroundColor;
    std::string backgroundColor;
    double ratio;
    bool isWCAG_AA; // >= 4.5:1 for normal text
    bool isWCAG_AAA; // >= 7:1 for normal text
};

// ==========================================
// 2. ACCESSIBILITY AUDIT ENGINE
// ==========================================

class AccessibilityAudit {
private:
    std::vector<A11yIssue> issues;
    size_t issueIdCounter = 0;
    static const size_t MAX_ISSUES = 1000;

    std::string generateIssueId() {
        return "a11y_" + std::to_string(issueIdCounter++);
    }

public:
    AccessibilityAudit() {
        issues.reserve(MAX_ISSUES);
    }

    // Record accessibility issue
    std::string reportIssue(
        A11yIssueType type,
        A11yIssueSeverity severity,
        const std::string& elementSelector,
        const std::string& elementTag,
        const std::string& message,
        const std::string& suggestion,
        int wcagLevel = 2
    ) {
        if (issues.size() >= MAX_ISSUES) {
            issues.erase(issues.begin());
        }

        std::string id = generateIssueId();
        issues.push_back({
            id,
            type,
            severity,
            elementSelector,
            elementTag,
            message,
            suggestion,
            wcagLevel,
            false
        });

        return id;
    }

    // Fix issue
    bool fixIssue(const std::string& issueId) {
        for (auto& issue : issues) {
            if (issue.id == issueId) {
                issue.isFixed = true;
                return true;
            }
        }
        return false;
    }

    // Common checks
    void checkImageAltText(const std::string& selector, bool hasAlt, bool hasAriaLabel) {
        if (!hasAlt && !hasAriaLabel) {
            reportIssue(
                A11yIssueType::MISSING_ALT_TEXT,
                A11yIssueSeverity::CRITICAL,
                selector,
                "img",
                "Image missing alt text and aria-label",
                "Add descriptive alt text or aria-label to image",
                2
            );
        }
    }

    void checkColorContrast(
        const std::string& selector,
        const ColorContrast& contrast
    ) {
        if (!contrast.isWCAG_AA) {
            reportIssue(
                A11yIssueType::LOW_CONTRAST,
                A11yIssueSeverity::ERROR,
                selector,
                "*",
                "Color contrast ratio " + std::to_string(contrast.ratio) + ":1 below WCAG AA (4.5:1)",
                "Adjust colors to meet minimum contrast ratio",
                2
            );
        }
    }

    void checkHeadingOrder(const std::vector<int>& headingLevels) {
        for (size_t i = 1; i < headingLevels.size(); i++) {
            if (headingLevels[i] - headingLevels[i - 1] > 1) {
                reportIssue(
                    A11yIssueType::NO_HEADING_ORDER,
                    A11yIssueSeverity::WARNING,
                    "h" + std::to_string(headingLevels[i]),
                    "heading",
                    "Heading hierarchy skipped level",
                    "Maintain proper heading order (H1 -> H2 -> H3)",
                    2
                );
            }
        }
    }

    void checkFormLabel(const std::string& selector, bool hasLabel, bool hasAriaLabel) {
        if (!hasLabel && !hasAriaLabel) {
            reportIssue(
                A11yIssueType::MISSING_FORM_LABEL,
                A11yIssueSeverity::ERROR,
                selector,
                "input",
                "Form field missing associated label",
                "Add label element or aria-label to form input",
                2
            );
        }
    }

    void checkKeyboardAccess(const std::string& selector, bool isKeyboardAccessible) {
        if (!isKeyboardAccessible) {
            reportIssue(
                A11yIssueType::KEYBOARD_NOT_ACCESSIBLE,
                A11yIssueSeverity::CRITICAL,
                selector,
                "*",
                "Interactive element not keyboard accessible",
                "Add tabindex or make element focusable",
                2
            );
        }
    }

    // Get all issues
    const std::vector<A11yIssue>& getIssues() const {
        return issues;
    }

    // Get issues by severity
    std::vector<A11yIssue> getIssuesBySeverity(A11yIssueSeverity severity) const {
        std::vector<A11yIssue> filtered;
        for (const auto& issue : issues) {
            if (issue.severity == severity) {
                filtered.push_back(issue);
            }
        }
        return filtered;
    }

    // Get issues by type
    std::vector<A11yIssue> getIssuesByType(A11yIssueType type) const {
        std::vector<A11yIssue> filtered;
        for (const auto& issue : issues) {
            if (issue.type == type) {
                filtered.push_back(issue);
            }
        }
        return filtered;
    }

    // Generate audit report
    A11yAuditReport generateReport() const {
        A11yAuditReport report = {0, 0, 0, 0, 0, 0, 0, {}, {}};
        report.totalIssues = issues.size();

        for (const auto& issue : issues) {
            if (!issue.isFixed) {
                if (issue.severity == A11yIssueSeverity::CRITICAL) report.criticalCount++;
                else if (issue.severity == A11yIssueSeverity::ERROR) report.errorCount++;
                else if (issue.severity == A11yIssueSeverity::WARNING) report.warningCount++;
                else report.infoCount++;
                report.issues.push_back(issue);
            }
        }

        // Calculate WCAG compliance
        size_t wcagAAPass = 0;
        size_t wcagAAAPass = 0;
        for (const auto& issue : issues) {
            if (issue.isFixed) {
                if (issue.wcagLevel >= 2) wcagAAPass++;
                if (issue.wcagLevel >= 3) wcagAAAPass++;
            }
        }

        if (!issues.empty()) {
            report.wcagAaCompliance = (double)wcagAAPass / (double)issues.size() * 100.0;
            report.wcagAaaCompliance = (double)wcagAAAPass / (double)issues.size() * 100.0;
        }

        return report;
    }

    void clear() {
        issues.clear();
        issueIdCounter = 0;
    }
};

// ==========================================
// 3. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(accessibility_audit_module) {
    class_<A11yIssue>("A11yIssue")
        .property("id", &A11yIssue::id)
        .property("elementSelector", &A11yIssue::elementSelector)
        .property("message", &A11yIssue::message)
        .property("suggestion", &A11yIssue::suggestion)
        .property("wcagLevel", &A11yIssue::wcagLevel)
        .property("isFixed", &A11yIssue::isFixed);

    register_vector<A11yIssue>("VectorA11yIssue");

    class_<A11yAuditReport>("A11yAuditReport")
        .property("totalIssues", &A11yAuditReport::totalIssues)
        .property("criticalCount", &A11yAuditReport::criticalCount)
        .property("errorCount", &A11yAuditReport::errorCount)
        .property("warningCount", &A11yAuditReport::warningCount)
        .property("wcagAaCompliance", &A11yAuditReport::wcagAaCompliance)
        .property("wcagAaaCompliance", &A11yAuditReport::wcagAaaCompliance)
        .property("issues", &A11yAuditReport::issues);

    class_<ColorContrast>("ColorContrast")
        .property("ratio", &ColorContrast::ratio)
        .property("isWCAG_AA", &ColorContrast::isWCAG_AA)
        .property("isWCAG_AAA", &ColorContrast::isWCAG_AAA);

    class_<AccessibilityAudit>("AccessibilityAudit")
        .constructor()
        .function("reportIssue", select_overload<std::string(
            A11yIssueType,
            A11yIssueSeverity,
            const std::string&,
            const std::string&,
            const std::string&,
            const std::string&,
            int
        )>(&AccessibilityAudit::reportIssue))
        .function("fixIssue", &AccessibilityAudit::fixIssue)
        .function("checkImageAltText", &AccessibilityAudit::checkImageAltText)
        .function("checkColorContrast", select_overload<void(
            const std::string&,
            const ColorContrast&
        )>(&AccessibilityAudit::checkColorContrast))
        .function("checkFormLabel", &AccessibilityAudit::checkFormLabel)
        .function("checkKeyboardAccess", &AccessibilityAudit::checkKeyboardAccess)
        .function("getIssues", &AccessibilityAudit::getIssues)
        .function("getIssuesBySeverity", select_overload<std::vector<A11yIssue>(
            A11yIssueSeverity
        )const>(&AccessibilityAudit::getIssuesBySeverity))
        .function("getIssuesByType", select_overload<std::vector<A11yIssue>(
            A11yIssueType
        )const>(&AccessibilityAudit::getIssuesByType))
        .function("generateReport", &AccessibilityAudit::generateReport)
        .function("clear", &AccessibilityAudit::clear);

    enum_<A11yIssueSeverity>("A11yIssueSeverity")
        .value("INFO", A11yIssueSeverity::INFO)
        .value("WARNING", A11yIssueSeverity::WARNING)
        .value("ERROR", A11yIssueSeverity::ERROR)
        .value("CRITICAL", A11yIssueSeverity::CRITICAL);

    enum_<A11yIssueType>("A11yIssueType")
        .value("MISSING_ALT_TEXT", A11yIssueType::MISSING_ALT_TEXT)
        .value("LOW_CONTRAST", A11yIssueType::LOW_CONTRAST)
        .value("MISSING_LABEL", A11yIssueType::MISSING_LABEL)
        .value("KEYBOARD_NOT_ACCESSIBLE", A11yIssueType::KEYBOARD_NOT_ACCESSIBLE)
        .value("INVALID_ARIA_ROLE", A11yIssueType::INVALID_ARIA_ROLE);
}
#endif
