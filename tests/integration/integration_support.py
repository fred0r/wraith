import os
import unittest

from wraith_harness import ROOT


class IrcIntegrationCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.geteuid() == 0:
            raise unittest.SkipTest("wraith refuses to run as root")
        if not (ROOT / "wraith").is_file():
            raise unittest.SkipTest("wraith is not built; run make dynamic")

    def assert_no_new_line(self, fake_irc, prefix, start, timeout=1.5,
                           nick=None, description=None):
        try:
            line = fake_irc.wait_for_new_line(
                prefix, start, timeout=timeout, nick=nick)
        except TimeoutError:
            return
        self.fail("unexpected %s: %r" % (
            description or prefix, line))

    def assert_cookie_output_clean(self, harness, nick=None):
        output = (harness.leaf_process(nick).output()
                  if nick is not None else harness.output())
        self.assertNotIn("Missing cookie", output)
        self.assertNotIn("Invalid cookie", output)
        self.assertNotIn("Cookie retry failed", output)
