import time
import unittest

from integration_support import IrcIntegrationCase
from wraith_harness import LinkedWraithHarness


class GroupChangeOpsTest(IrcIntegrationCase):

    @staticmethod
    def _op_senders(harness, targets, starts):
        senders = {target: [] for target in targets}

        for bot in harness.leaves:
            lines = harness.fake_irc.received(bot)[starts[bot]:]
            for line in lines:
                fields = line.split()
                if len(fields) < 4 or fields[0] != "MODE":
                    continue
                if fields[1] != harness.CHANNEL or "o" not in fields[2]:
                    continue
                for target in targets:
                    if target in fields[3:]:
                        senders[target].append(bot)
        return senders

    def test_group_change_ops_joiners_once(self):
        old_bots = ("op1", "op2", "op3")
        stay_bot = "stay"
        new_bots = ("new1", "new2")
        with LinkedWraithHarness(
            leaves=old_bots + (stay_bot,) + new_bots,
            bot_groups={bot: "oldgroup" for bot in old_bots}
                | {stay_bot: "oldgroup,main"}
                | {bot: "main" for bot in new_bots},
            initially_joined=old_bots + (stay_bot,),
            channel_options="groups oldgroup",
            add_hostmasks=True,
        ) as harness:
            harness.start()
            harness.wait_roles(harness.CHANNEL, nick=old_bots[0])
            starts = {
                bot: len(harness.fake_irc.received(bot))
                for bot in harness.leaves
            }
            harness.hub.write(
                ".chanset %s groups main\n" % harness.CHANNEL)

            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                members = harness.fake_irc.channels[harness.CHANNEL]
                if all("+o" in members.get(target, {}).get("modes", "")
                       for target in new_bots):
                    break
                time.sleep(0.1)
            else:
                self.fail("new-group bots did not join and get opped")

            time.sleep(0.5)
            senders = self._op_senders(harness, new_bots, starts)
            for target in new_bots:
                self.assertEqual(
                    len(senders[target]), 1,
                    "%s was opped by multiple bots: %s"
                    % (target, senders[target]))


if __name__ == "__main__":
    unittest.main()
