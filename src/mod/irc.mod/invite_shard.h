#ifndef _EGG_MOD_IRC_INVITE_SHARD_H
#define _EGG_MOD_IRC_INVITE_SHARD_H

#include <string>
#include <vector>

namespace invite_shard {

struct Bot {
  std::string handle;
  std::vector<std::string> groups;
};

struct Assignment {
  std::string target;
  std::string inviter;
};

std::string canonical_group(const std::vector<std::string>& channel_groups,
                            const std::vector<std::string>& bot_groups);

std::vector<Assignment> assign(const std::vector<std::string>& channel_groups,
                               const std::vector<Bot>& inviters,
                               const std::vector<Bot>& targets);

}

#endif
