import unittest

from integration_support import IrcIntegrationCase
from wraith_harness import LinkedWraithHarness


class LinkedIrcTest(IrcIntegrationCase):

    def test_userfile_group_change_waits_then_leaves(self):
        with LinkedWraithHarness() as harness:
            harness.start()
            harness.fake_irc.add_member(
                harness.CHANNEL, "peerbot", "peer", "peerhost")
            harness.wait_ready()

            mark = harness.fake_irc.line_count()
            harness.leaf_console(
                ".chanset %s groups main" % harness.CHANNEL)
            harness.wait_output(
                "waiting for an opped replacement bot", timeout=15)
            self.assert_no_new_line(
                harness.fake_irc, "PART %s" % harness.CHANNEL, mark,
                description="premature linked part",
            )

            harness.fake_irc.send_mode(harness.CHANNEL, "+o", "peerbot")
            harness.fake_irc.wait_for(
                lambda line: line.startswith("PART %s" % harness.CHANNEL),
                timeout=15,
                description="linked group-change part",
            )

    def test_server_disconnect_reconnects(self):
        with LinkedWraithHarness() as harness:
            harness.start()
            mark = harness.fake_irc.line_count()
            harness.fake_irc.drop_client()

            harness.fake_irc.wait_for_new_line(
                "NICK %s" % harness.LEAF, mark, timeout=20)
            harness.fake_irc.wait_connected(timeout=20)
            harness.fake_irc.wait_for_new_line(
                "JOIN %s" % harness.CHANNEL, mark, timeout=20)
            harness.wait_ready()


if __name__ == "__main__":
    unittest.main()
