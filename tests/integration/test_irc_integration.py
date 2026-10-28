import unittest

from integration_support import IrcIntegrationCase
from wraith_harness import WraithHarness


class IrcIntegrationTest(IrcIntegrationCase):

    def test_smoke_register_and_join(self):
        with WraithHarness() as harness:
            harness.start()
            received = harness.fake_irc.received()
            self.assertIn("NICK %s" % harness.BOT, received)
            self.assertTrue(any(line.startswith("USER ") for line in received))

            start = harness.add_channel()
            harness.wait_channel_active(start)
            received = harness.fake_irc.received()
            self.assertTrue(any(line.startswith("JOIN %s" % harness.CHANNEL) for line in received))
            self.assertTrue(any(line.startswith("WHO %s" % harness.CHANNEL) for line in received))

            mark = harness.fake_irc.line_count()
            harness.fake_irc.send(":fake.irc PING :integration")
            pong = harness.fake_irc.wait_for_new(
                lambda line: line.startswith("PONG "), mark, timeout=5
            )
            self.assertIn("integration", pong)

    def test_initial_join_ops_all_pending_bots(self):
        peers = ("peer1", "peer2")
        with WraithHarness(extra_bots=peers) as harness:
            harness.start()
            for peer in peers:
                harness.add_bot_hostmask(peer, "peer", "%shost" % peer)
            harness.fake_irc.defer_who.add(harness.CHANNEL)
            start = harness.fake_irc.line_count()
            harness.console(".+chan %s +fastop" % harness.CHANNEL)
            harness.fake_irc.wait_for_new_line("JOIN %s" % harness.CHANNEL, start, timeout=15)
            harness.fake_irc.wait_for_new_line("WHO %s" % harness.CHANNEL, start, timeout=15)

            for peer in peers:
                harness.fake_irc.add_member(
                    harness.CHANNEL, peer, "peer", "%shost" % peer)
            harness.wait_ready()
            harness.fake_irc.finish_deferred_who(end=False)
            harness.fake_irc.send_mode(harness.CHANNEL, "+o", harness.BOT)
            harness.wait_ready()
            harness.fake_irc.send_who_end(harness.CHANNEL)

            for peer in peers:
                harness.fake_irc.wait_for(
                    lambda line, peer=peer: (
                        line.startswith("MODE %s " % harness.CHANNEL)
                        and peer in line
                        and "o" in line.split()[2].lstrip("+-")
                    ),
                    timeout=45,
                    description="op for pending bot %s" % peer,
                )

    def test_initial_join_ignores_preexisting_bot(self):
        with WraithHarness(extra_bots=("peerbot",)) as harness:
            harness.start()
            harness.add_bot_hostmask("peerbot", "peer", "peerhost")
            harness.fake_irc.defer_who.add(harness.CHANNEL)
            start = harness.fake_irc.line_count()
            harness.console(".+chan %s +fastop" % harness.CHANNEL)
            harness.fake_irc.wait_for_new_line(
                "JOIN %s" % harness.CHANNEL, start, timeout=15)
            harness.fake_irc.wait_for_new_line(
                "WHO %s" % harness.CHANNEL, start, timeout=15)

            harness.fake_irc.add_member(
                harness.CHANNEL, "peerbot", "peer", "peerhost", send_join=False)
            harness.fake_irc.finish_deferred_who(end=False)
            harness.fake_irc.send_mode(harness.CHANNEL, "+o", harness.BOT)
            harness.wait_ready()
            harness.fake_irc.send_who_end(harness.CHANNEL)
            harness.wait_ready()

            self.assertFalse(
                any(line.startswith("MODE %s " % harness.CHANNEL)
                    and "peerbot" in line
                    for line in harness.fake_irc.received()[start:]),
                "pre-existing bot was treated as a pending join",
            )

    def test_group_change_waits_for_opped_replacement(self):
        with WraithHarness() as harness:
            harness.start()
            start = harness.add_channel()
            harness.wait_channel_active(start)

            mark = harness.fake_irc.line_count()
            harness.console(".chanset %s groups othergroup" % harness.CHANNEL)
            harness.wait_output("waiting for an opped replacement bot", timeout=15)
            self.assert_no_new_line(
                harness.fake_irc, "PART %s" % harness.CHANNEL, mark,
                description="premature group-change part",
            )

    def test_missing_cookie_is_detected(self):
        with WraithHarness(extra_bots=("opperbot", "peerbot")) as harness:
            harness.start()
            harness.add_bot_hostmask("opperbot", "opp", "opphost")
            harness.add_bot_hostmask("peerbot", "peer", "peerhost")
            start = harness.add_channel(options="")
            harness.wait_channel_active(start)
            harness.fake_irc.send_mode(harness.CHANNEL, "+o", harness.BOT)
            harness.wait_ready()
            harness.fake_irc.add_member(harness.CHANNEL, "opperbot", "opp", "opphost")
            harness.fake_irc.add_member(harness.CHANNEL, "peerbot", "peer", "peerhost")
            harness.wait_ready()

            harness.fake_irc.send_opper_mode(
                harness.CHANNEL, "opperbot", "peerbot"
            )
            harness.wait_output("Missing cookie", timeout=15)

    def test_bad_hash_cookie_is_deferred_and_retried(self):
        with WraithHarness(extra_bots=("opperbot", "peerbot")) as harness:
            harness.start()
            harness.add_bot_hostmask("opperbot", "opp", "opphost")
            harness.add_bot_hostmask("peerbot", "peer", "peerhost")
            start = harness.add_channel(options="")
            harness.wait_channel_active(start)
            harness.fake_irc.send_mode(harness.CHANNEL, "+o", harness.BOT)
            harness.wait_ready()
            harness.fake_irc.add_member(harness.CHANNEL, "opperbot", "opp", "opphost")
            harness.fake_irc.add_member(harness.CHANNEL, "peerbot", "peer", "peerhost")
            harness.wait_ready()

            mark = harness.fake_irc.line_count()
            harness.fake_irc.send_opper_mode(
                harness.CHANNEL, "opperbot", "peerbot", cookie="abc!Ab3d@"
            )
            who = harness.fake_irc.wait_for_new(
                lambda line: line.startswith("WHO peerbot"), mark, timeout=15,
                description="pending cookie WHO",
            )
            self.assertEqual("WHO peerbot", who)
            harness.wait_output("Cookie retry failed", timeout=15)


if __name__ == "__main__":
    unittest.main()
