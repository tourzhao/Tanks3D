#ifndef TANKS3D_TEST_SUPPORT_H
#define TANKS3D_TEST_SUPPORT_H

#include <iostream>
#include <string>

namespace tanks3d_test
{
class Reporter
{
public:
    void reset()
    {
        currentSuite_.clear();
        currentChecks_ = 0;
        totalChecks_ = 0;
        completedSuites_ = 0;
    }

    void beginSuite(const char *name)
    {
        finishCurrentSuite();
        currentSuite_ = name;
        currentChecks_ = 0;
    }

    bool check(bool condition, const std::string &message)
    {
        ++currentChecks_;
        ++totalChecks_;
        if (!condition)
        {
            std::cerr << "SELF-TEST FAILED [" << currentSuite_ << ", check "
                      << currentChecks_ << "]: " << message << '\n';
        }
        return condition;
    }

    void finish()
    {
        finishCurrentSuite();
        std::cout << "Self-test suites passed: " << completedSuites_
                  << (completedSuites_ == 1 ? " suite, " : " suites, ")
                  << totalChecks_ << " runtime checks.\n";
    }

private:
    void finishCurrentSuite()
    {
        if (currentSuite_.empty())
            return;
        std::cout << "  PASS " << currentSuite_ << " (" << currentChecks_
                  << " checks)\n";
        ++completedSuites_;
        currentSuite_.clear();
        currentChecks_ = 0;
    }

    std::string currentSuite_;
    int currentChecks_ = 0;
    int totalChecks_ = 0;
    int completedSuites_ = 0;
};

template <typename Runner>
int runSuite(Reporter &reporter, const char *name, Runner runner)
{
    reporter.beginSuite(name);
    return runner();
}
} // namespace tanks3d_test

#endif
