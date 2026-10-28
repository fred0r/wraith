import time
import unittest

from cookie_factory import make_cookie
from integration_support import IrcIntegrationCase
from wraith_harness import WraithHarness


class IrcBehaviorTest(IrcIntegrationCase):

    def test_group_filter_controls_join(self):
        with WraithHarness(groups="oldgroup") as harness:
            harness.start()
            mark = harness.fake_irc.line_count()
            harness.console(".+chan %s +fastop groups main" % harness.CHANNEL)
            self.assert_no_new_line(
                harness.fake_irc, "JOIN %s" % harness.CHANNEL, mark,
                description="join outside group",
            )

            harness.console(".chanset %s groups oldgroup" % harness.CHANNEL)
            harness.fake_irc.wait_for_new_line(
                "JOIN %s" % harness.CHANNEL, mark, timeout=15)

    def test_kick_triggers_rejoin(self):
        with WraithHarness() as harness:
            harness.start()
            start = harness.add_channel()
            harness.wait_channel_active(start)
            mark = harness.fake_irc.line_count()

            harness.fake_irc.kick_member(harness.CHANNEL, harness.BOT)
            harness.fake_irc.wait_for_new_line(
                "JOIN %s" % harness.CHANNEL, mark, timeout=15)
            harness.fake_irc.wait_for_new_line(
                "WHO %s" % harness.CHANNEL, mark, timeout=15)
            harness.wait_ready()

    @staticmethod
    def _cookie_setup(harness):
        harness.add_bot_hostmask("peerbot", "peer", "peerhost")
        start = harness.add_channel(options="")
        harness.wait_channel_active(start)
        harness.fake_irc.send_mode(harness.CHANNEL, "+o", harness.BOT)
        harness.wait_ready()

        mark = harness.fake_irc.line_count()
        harness.fake_irc.add_member(harness.CHANNEL, "peerbot", "peer", "peerhost")
        harness.wait_ready()
        cookie = harness.fake_irc.wait_for_cookie(
            harness.CHANNEL, "peerbot", mark, timeout=15)
        harness.fake_irc.part_member(harness.CHANNEL, "peerbot")
        harness.wait_ready()
        harness.fake_irc.add_member(harness.CHANNEL, "peerbot", "peer", "peerhost")
        harness.wait_ready()
        return cookie

    @staticmethod
    def _operator_setup(harness, options=""):
        for bot in ("opperbot", "peerbot"):
            harness.add_bot_hostmask(bot, "opp" if bot == "opperbot" else "peer",
                                     "opphost" if bot == "opperbot" else "peerhost")
        start = harness.add_channel(options=options)
        harness.wait_channel_active(start)
        harness.fake_irc.send_mode(harness.CHANNEL, "+o", harness.BOT)
        harness.wait_ready()
        harness.fake_irc.add_member(
            harness.CHANNEL, "opperbot", "opp", "opphost")
        harness.fake_irc.add_member(
            harness.CHANNEL, "peerbot", "peer", "peerhost")
        harness.wait_ready()

    @staticmethod
    def _generated_cookie(harness, counter=1):
        return make_cookie(
            harness.SALT1, harness.SALT2, harness.CHANNEL,
            "opperbot", "opp@opphost", "peerbot", "peer@peerhost",
            int(time.time()), counter=counter,
        )

    def test_valid_cookie_replay_accepted(self):
        with WraithHarness(extra_bots=("peerbot",)) as harness:
            harness.start()
            cookie = self._cookie_setup(harness)
            harness.fake_irc.send_opper_mode(
                harness.CHANNEL, harness.BOT, "peerbot", cookie=cookie)
            harness.wait_ready()
            self.assert_cookie_output_clean(harness)

    def test_pending_cookie_recovers_after_who(self):
        with WraithHarness(extra_bots=("peerbot",)) as harness:
            harness.start()
            cookie = self._cookie_setup(harness)

            harness.fake_irc.remove_member(harness.CHANNEL, "peerbot")
            harness.wait_ready()
            harness.fake_irc.add_member(
                harness.CHANNEL, "peerbot", "peer", "changedhost")
            harness.wait_ready()

            harness.fake_irc.defer_who.add("peerbot")
            mark = harness.fake_irc.line_count()
            harness.fake_irc.send_opper_mode(
                harness.CHANNEL, harness.BOT, "peerbot", cookie=cookie)
            who = harness.fake_irc.wait_for_new(
                lambda line: line.startswith("WHO peerbot"), mark, timeout=15,
                description="pending cookie WHO",
            )
            self.assertEqual("WHO peerbot", who)

            harness.fake_irc.set_member_host(harness.CHANNEL, "peerbot", "peerhost")
            harness.fake_irc.finish_deferred_who()
            harness.wait_ready()
            self.assert_cookie_output_clean(harness)

    def test_generated_cookie_accepted(self):
        with WraithHarness(extra_bots=("opperbot", "peerbot")) as harness:
            harness.start()
            self._operator_setup(harness)
            harness.fake_irc.send_opper_mode(
                harness.CHANNEL, "opperbot", "peerbot",
                cookie=self._generated_cookie(harness))
            harness.wait_ready()
            self.assert_cookie_output_clean(harness)

    def test_cookie_counter_replay_rejected(self):
        with WraithHarness(extra_bots=("opperbot", "peerbot")) as harness:
            harness.start()
            self._operator_setup(harness)
            cookie = self._generated_cookie(harness)
            harness.fake_irc.send_opper_mode(
                harness.CHANNEL, "opperbot", "peerbot", cookie=cookie)
            harness.wait_ready()
            harness.fake_irc.send_opper_mode(
                harness.CHANNEL, "opperbot", "peerbot", cookie=cookie)
            harness.wait_output("Invalid cookie", timeout=15)
            self.assertIn("bad count", harness.output())

    def test_fastop_skips_cookie_check(self):
        with WraithHarness(extra_bots=("opperbot", "peerbot")) as harness:
            harness.start()
            self._operator_setup(harness, options="+fastop")
            harness.fake_irc.send_opper_mode(
                harness.CHANNEL, "opperbot", "peerbot")
            harness.wait_ready()
            self.assert_cookie_output_clean(harness)

    def test_inactive_channel_parts_and_reactivates(self):
        with WraithHarness() as harness:
            harness.start()
            start = harness.add_channel()
            harness.wait_channel_active(start)

            mark = harness.fake_irc.line_count()
            harness.console(".chanset %s +inactive" % harness.CHANNEL)
            harness.fake_irc.wait_for_new_line(
                "PART %s" % harness.CHANNEL, mark, timeout=20)
            self.assert_no_new_line(
                harness.fake_irc, "JOIN %s" % harness.CHANNEL, mark,
                description="inactive channel rejoin",
            )

            mark = harness.fake_irc.line_count()
            harness.console(".chanset %s -inactive" % harness.CHANNEL)
            harness.fake_irc.wait_for_new_line(
                "JOIN %s" % harness.CHANNEL, mark, timeout=15)
            harness.fake_irc.wait_for_new_line(
                "WHO %s" % harness.CHANNEL, mark, timeout=15)
            harness.wait_ready()

    def test_unknown_numeric_is_ignored(self):
        with WraithHarness() as harness:
            harness.start()
            harness.fake_irc.send(":fake.irc 999 bot :unknown numeric")
            harness.fake_irc.send(":fake.irc NOTANUMERIC bot")
            harness.wait_ready()


if __name__ == "__main__":
    unittest.main()
