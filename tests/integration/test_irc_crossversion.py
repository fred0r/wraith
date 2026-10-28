import os
import unittest
from pathlib import Path

from integration_support import IrcIntegrationCase
from wraith_harness import LinkedWraithHarness


class CrossVersionIrcTest(IrcIntegrationCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.old_binary = Path(os.environ.get("WRAITH_OLD_BINARY", ""))
        if not cls.old_binary.is_file():
            raise unittest.SkipTest(
                "set WRAITH_OLD_BINARY to a pre-refactor wraith binary")

    def test_old_and_new_cookie_ops_both_directions(self):
        with LinkedWraithHarness(
            leaves=("oldbot", "newbot"),
            binaries={"oldbot": self.old_binary},
            channel_options="groups oldgroup",
        ) as harness:
            harness.start()

            harness.fake_irc.send(
                ":fake.irc MODE %s -o newbot" % harness.CHANNEL)
            harness.wait_ready("oldbot")
            harness.wait_ready("newbot")

            mark = len(harness.fake_irc.sent("newbot"))
            harness.console("oldbot", ".op newbot %s" % harness.CHANNEL)
            old_to_new = harness.fake_irc.wait_for_sent(
                lambda line: line.startswith(":oldbot")
                    and "MODE %s +o-b newbot" % harness.CHANNEL in line,
                mark, timeout=20, description="old-to-new cookie op",
                nick="newbot",
            )
            self.assertIn("newbot", old_to_new)
            harness.wait_ready("newbot")
            self.assert_cookie_output_clean(harness, "newbot")

            harness.fake_irc.send(
                ":fake.irc MODE %s -o oldbot" % harness.CHANNEL)
            harness.wait_ready("oldbot")
            harness.wait_ready("newbot")

            mark = len(harness.fake_irc.sent("oldbot"))
            harness.console("newbot", ".op oldbot %s" % harness.CHANNEL)
            new_to_old = harness.fake_irc.wait_for_sent(
                lambda line: line.startswith(":newbot")
                    and "MODE %s +o-b oldbot" % harness.CHANNEL in line,
                mark, timeout=20, description="new-to-old cookie op",
                nick="oldbot",
            )
            self.assertIn("oldbot", new_to_old)
            harness.wait_ready("oldbot")
            self.assert_cookie_output_clean(harness, "oldbot")


if __name__ == "__main__":
    unittest.main()
