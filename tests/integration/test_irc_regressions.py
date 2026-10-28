import time
import unittest

from cookie_factory import make_cookie
from integration_support import IrcIntegrationCase
from wraith_harness import WraithHarness


class IrcRegressionTest(IrcIntegrationCase):

    @staticmethod
    def _op_bot(harness):
        harness.fake_irc.send_mode(harness.CHANNEL, "+o", harness.BOT)
        harness.wait_ready()

    def test_self_cookie_op_accepted(self):
        with WraithHarness(extra_bots=("opperbot",)) as harness:
            harness.start()
            harness.add_bot_hostmask("opperbot", "opp", "opphost")
            start = harness.add_channel(options="")
            harness.wait_channel_active(start)
            self._op_bot(harness)
            harness.fake_irc.add_member(
                harness.CHANNEL, "opperbot", "opp", "opphost")
            harness.wait_ready()

            bot_member = harness.fake_irc.channels[harness.CHANNEL][harness.BOT]
            target_userhost = "%s@%s" % (
                bot_member["user"], bot_member["host"])
            cookie = make_cookie(
                harness.SALT1, harness.SALT2, harness.CHANNEL,
                "opperbot", "opp@opphost",
                harness.BOT, target_userhost, int(time.time()),
            )
            harness.fake_irc.send_opper_mode(
                harness.CHANNEL, "opperbot", harness.BOT, cookie=cookie)
            harness.wait_ready()

            self.assert_cookie_output_clean(harness)

    def test_join_failures(self):
        cases = (
            (471, "Channel full--can't join"),
            (473, "Channel invite only--can't join"),
            (474, "Banned from channel--can't join"),
            (475, "Bad key--can't join"),
        )
        for numeric, expected in cases:
            with self.subTest(numeric=numeric):
                with WraithHarness() as harness:
                    harness.start()
                    harness.console(".console +j")
                    harness.fake_irc.fail_join(
                        harness.CHANNEL, numeric, "join rejected")
                    mark = harness.fake_irc.line_count()
                    harness.console(
                        ".+chan %s +fastop" % harness.CHANNEL)
                    harness.fake_irc.wait_for_new_line(
                        "JOIN %s" % harness.CHANNEL, mark, timeout=15)
                    harness.wait_output(expected, timeout=15)
                    harness.wait_ready()

                    after_join = harness.fake_irc.line_count()
                    self.assert_no_new_line(
                        harness.fake_irc, "JOIN %s" % harness.CHANNEL,
                        after_join,
                        description="retry after join failure",
                    )

    def test_standalone_reconnect_after_error(self):
        with WraithHarness() as harness:
            harness.start()
            start = harness.add_channel()
            harness.wait_channel_active(start)
            harness.wait_ready()

            mark = harness.fake_irc.line_count()
            harness.fake_irc.send("ERROR :Closing Link: test")
            harness.fake_irc.wait_for_new_line(
                "NICK %s" % harness.BOT, mark, timeout=45)
            harness.fake_irc.wait_connected(timeout=45)
            harness.fake_irc.wait_for_new_line(
                "JOIN %s" % harness.CHANNEL, mark, timeout=45)
            harness.fake_irc.wait_for_new_line(
                "WHO %s" % harness.CHANNEL, mark, timeout=45)
            harness.wait_ready()

    def test_malformed_lines_do_not_disconnect(self):
        with WraithHarness() as harness:
            harness.start()
            lines = (
                ":",
                ":prefix",
                "PING",
                "PING :",
                "MODE",
                "MODE #wraith-test +o",
                "KICK",
                "KICK #wraith-test",
                "JOIN",
                "PRIVMSG",
                "NOTICE",
                "352",
                "001",
                "notacommand",
            )
            for line in lines:
                harness.fake_irc.send(line)
            harness.wait_ready()

    def test_pending_cookie_cleanup_after_nick_and_quit(self):
        with WraithHarness(extra_bots=("opperbot", "peerbot")) as harness:
            harness.start()
            for nick, user, host in (
                ("opperbot", "opp", "opphost"),
                ("peerbot", "peer", "peerhost"),
            ):
                harness.add_bot_hostmask(nick, user, host)
            start = harness.add_channel(options="")
            harness.wait_channel_active(start)
            self._op_bot(harness)
            for nick, user, host in (
                ("opperbot", "opp", "opphost"),
                ("peerbot", "peer", "peerhost"),
            ):
                harness.fake_irc.add_member(
                    harness.CHANNEL, nick, user, host)
            harness.wait_ready()
            harness.fake_irc.defer_who.add("opperbot")
            harness.fake_irc.defer_who.add("peerbot")

            mark = harness.fake_irc.line_count()
            harness.fake_irc.send_opper_mode(
                harness.CHANNEL, "opperbot", "peerbot",
                cookie="abc!Ab3d@")
            harness.fake_irc.wait_for_new(
                lambda line: line.startswith("WHO peerbot"),
                mark, timeout=15, description="pending cookie WHO",
            )
            harness.fake_irc.nick_member(
                "peerbot", "peer2", user="peer", host="peerhost")
            harness.fake_irc.remove_member(harness.CHANNEL, "peer2")
            harness.wait_ready()
            time.sleep(6)

            received = harness.fake_irc.received()[mark:]
            self.assertFalse(
                any(line.startswith("KICK %s peer" % harness.CHANNEL)
                    for line in received),
                "stale pending cookie kicked a departed member",
            )
            self.assertNotIn("Cookie retry failed", harness.output())


if __name__ == "__main__":
    unittest.main()
