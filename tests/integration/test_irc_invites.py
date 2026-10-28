import unittest

from integration_support import IrcIntegrationCase
from wraith_harness import LinkedWraithHarness


class InviteGroupChangeTest(IrcIntegrationCase):

    def test_group_change_distributes_invites(self):
        with LinkedWraithHarness(
            leaves=("b0nzai", "m0re", "oldbot", "t1", "t2", "t3"),
            bot_groups={
                "b0nzai": "drei,main",
                "m0re": "drei,main",
                "oldbot": "drei",
                "t1": "main",
                "t2": "main",
                "t3": "main",
            },
            initially_joined=("b0nzai", "m0re", "oldbot"),
            channel_options="+fastop groups drei",
        ) as harness:
            harness.start()
            harness.console(
                "b0nzai", ".chanset %s chanmode +i" % harness.CHANNEL)
            harness.fake_irc.wait_for_line(
                "MODE %s +i" % harness.CHANNEL, timeout=15)
            harness.wait_ready("b0nzai")
            harness.wait_ready("m0re")
            harness.wait_ready("oldbot")

            mark = len(harness.fake_irc.sent())
            harness.hub.write(
                ".chanset %s groups main\n" % harness.CHANNEL)

            for target in ("t1", "t2", "t3"):
                harness.fake_irc.wait_for_line(
                    "JOIN %s" % harness.CHANNEL, timeout=30, nick=target)
                harness.wait_ready(target)

            invites = []
            for line in harness.fake_irc.sent()[mark:]:
                fields = line.split()
                if len(fields) >= 4 and fields[1] == "INVITE":
                    chan = fields[3].lstrip(":")
                    if chan == harness.CHANNEL:
                        invites.append(
                            (fields[0].lstrip(":").split("!", 1)[0],
                             fields[2]))

            inviters = {inviter for inviter, _ in invites}
            invited_targets = [target for _, target in invites]
            self.assertGreaterEqual(len(inviters), 3)
            self.assertIn("b0nzai", inviters)
            self.assertIn("m0re", inviters)
            self.assertIn("oldbot", inviters)
            for target in ("t1", "t2", "t3"):
                self.assertIn(target, invited_targets)
            self.assertIn("b0nzai", harness.fake_irc.channels[harness.CHANNEL])
            self.assertIn("m0re", harness.fake_irc.channels[harness.CHANNEL])


if __name__ == "__main__":
    unittest.main()
