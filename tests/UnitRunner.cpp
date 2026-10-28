#include <cstdlib>
#include <iostream>

#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

int main(int argc, char **argv)
{
  CPPUNIT_NS::TestResult result;
  CPPUNIT_NS::TestResultCollector collected;
  result.addListener(&collected);

  CPPUNIT_NS::BriefTestProgressListener progress;
  if (argc > 1 || getenv("V"))
    result.addListener(&progress);

  CPPUNIT_NS::TestRunner runner;
  runner.addTest(CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest());
  runner.run(result);

  CPPUNIT_NS::CompilerOutputter output(&collected, std::cerr);
  output.write();

  return collected.wasSuccessful() ? 0 : 1;
}
