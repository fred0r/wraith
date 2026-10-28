#include "invite_shard.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace {

std::string lower_ascii(const std::string& value)
{
  std::string result(value);

  for (char& ch : result)
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return result;
}

std::string trim(const std::string& value)
{
  size_t first = 0;
  size_t last = value.length();

  while (first < last && std::isspace(static_cast<unsigned char>(value[first])))
    ++first;
  while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
    --last;
  return value.substr(first, last - first);
}

bool has_group(const std::vector<std::string>& groups, const std::string& group)
{
  const std::string wanted = lower_ascii(trim(group));

  for (const std::string& candidate : groups) {
    if (lower_ascii(trim(candidate)) == wanted)
      return true;
  }
  return false;
}

bool same_handle(const std::string& first, const std::string& second)
{
  return lower_ascii(first) == lower_ascii(second);
}

std::vector<invite_shard::Bot> normalized(std::vector<invite_shard::Bot> bots)
{
  std::sort(bots.begin(), bots.end(), [](const invite_shard::Bot& first,
                                         const invite_shard::Bot& second) {
    return lower_ascii(first.handle) < lower_ascii(second.handle);
  });
  bots.erase(std::unique(bots.begin(), bots.end(),
      [](const invite_shard::Bot& first, const invite_shard::Bot& second) {
        return same_handle(first.handle, second.handle);
      }), bots.end());
  return bots;
}

}

namespace invite_shard {

std::string canonical_group(const std::vector<std::string>& channel_groups,
                            const std::vector<std::string>& bot_groups)
{
  for (const std::string& group : channel_groups) {
    if (has_group(bot_groups, group))
      return trim(group);
  }
  return std::string();
}

std::vector<Assignment> assign(const std::vector<std::string>& channel_groups,
                               const std::vector<Bot>& inviters,
                               const std::vector<Bot>& targets)
{
  std::vector<Bot> inviter_list = normalized(inviters);
  std::vector<Bot> target_list = normalized(targets);
  std::map<std::string, std::vector<const Bot*> > buckets;
  std::vector<Assignment> result;

  for (const Bot& target : target_list) {
    const std::string group = canonical_group(channel_groups, target.groups);
    buckets[lower_ascii(group)].push_back(&target);
  }

  for (auto& bucket : buckets) {
    size_t next = 0;

    for (const Bot* target : bucket.second) {
      if (inviter_list.empty())
        break;

      for (size_t attempts = 0; attempts < inviter_list.size(); ++attempts) {
        const Bot& inviter = inviter_list[next % inviter_list.size()];

        ++next;
        if (same_handle(inviter.handle, target->handle))
          continue;

        Assignment assignment;
        assignment.target = target->handle;
        assignment.inviter = inviter.handle;
        result.push_back(assignment);
        break;
      }
    }
  }

  return result;
}

}
