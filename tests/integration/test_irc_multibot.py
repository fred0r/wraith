import re
import unittest

from integration_support import IrcIntegrationCase
from wraith_harness import LinkedWraithHarness


class MultiBotIrcTest(IrcIntegrationCase):

    @staticmethod
    def _role_members(output, role):
        clean = re.sub(r"\x1b\[[0-9;]*m", "", output)
        for line in clean.splitlines():
            match = re.match(r"^\s*%s\s*:\s*(.*)$" % re.escape(role), line)
            if match:
                return match.group(1).split()
        return []

    def test_two_leaves_join_and_roles_calculated(self):
        with LinkedWraithHarness(leaves=("leaf1", "leaf2")) as harness:
            harness.start()
            output = harness.wait_roles(harness.CHANNEL, nick="leaf1")
            self.assertIn("Roles for %s" % harness.CHANNEL, output)
            self.assertNotIn("not yet calculated", output)
            self.assertIn("leaf1", harness.fake_irc.channels[harness.CHANNEL])
            self.assertIn("leaf2", harness.fake_irc.channels[harness.CHANNEL])
            self.assertTrue(self._role_members(output, "voice"))
            self.assertTrue(self._role_members(output, "ban"))

    def test_voice_role_voices_joiner(self):
        with LinkedWraithHarness(leaves=("leaf1", "leaf2")) as harness:
            harness.start()
            output = harness.wait_roles(harness.CHANNEL, nick="leaf1")
            holders = self._role_members(output, "voice")
            self.assertTrue(holders, "no voice role holder:\n%s" % output)
            leaf = holders[0]

            harness.console(leaf, ".chanset %s +voice" % harness.CHANNEL)
            harness.console(leaf, ".chanset %s auto-delay 0" % harness.CHANNEL)
            harness.wait_ready(leaf)
            mark = harness.fake_irc.line_count(leaf)
            harness.fake_irc.add_member(
                harness.CHANNEL, "voiceme", "voice", "voicehost")
            harness.fake_irc.wait_for_new(
                lambda line: line.startswith(
                    "MODE %s +v voiceme" % harness.CHANNEL),
                mark, timeout=15, description="voice mode", nick=leaf,
            )

    def test_ban_role_enforces_join(self):
        with LinkedWraithHarness(leaves=("leaf1", "leaf2")) as harness:
            harness.start()
            output = harness.wait_roles(harness.CHANNEL, nick="leaf1")
            holders = self._role_members(output, "ban")
            self.assertTrue(holders, "no ban role holder:\n%s" % output)
            leaf = holders[0]

            harness.console(leaf, ".+ban *!*@badhost %s" % harness.CHANNEL)
            harness.wait_ready(leaf)
            mark = harness.fake_irc.line_count(leaf)
            harness.fake_irc.add_member(
                harness.CHANNEL, "baduser", "bad", "badhost")
            harness.fake_irc.wait_for_new(
                lambda line: line.startswith(
                    "KICK %s baduser" % harness.CHANNEL),
                mark, timeout=15, description="banned user kick", nick=leaf,
            )

            harness.console(leaf, ".+exempt *!*@goodhost %s" % harness.CHANNEL)
            harness.wait_ready(leaf)
            mark = harness.fake_irc.line_count(leaf)
            harness.fake_irc.add_member(
                harness.CHANNEL, "gooduser", "good", "goodhost")
            harness.wait_ready(leaf)
            self.assert_no_new_line(
                harness.fake_irc, "KICK %s gooduser" % harness.CHANNEL,
                mark, timeout=2, nick=leaf,
                description="exempt user kick",
            )


if __name__ == "__main__":
    unittest.main()
