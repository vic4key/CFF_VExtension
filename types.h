#pragma once

#include <json.hpp>
#include <fifo_map.hpp>

template<class K, class V, class dummy_compare, class A>
using unordered_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;
using json = nlohmann::basic_json<unordered_fifo_map>;

template <typename value_t>
value_t json_get(const json& jobject, const std::string& name, const value_t def)
{
  return jobject.contains(name) ? jobject[name].get<value_t>() : def;
}

template <typename value_t>
value_t json_get_option(json& jobject, const std::string& name, const value_t def)
{
  if (not jobject.contains("options"))
  {
    jobject["options"] = json::object();
  }

  auto& joptions = jobject["options"];

  return joptions.contains(name) ? joptions[name].get<value_t>() : def;
}

template <typename value_t>
void json_set_option(json& jobject, const std::string& name, const value_t def)
{
  if (not jobject.contains("options"))
  {
    jobject["options"] = json::object();
  }

  auto& joptions = jobject["options"];

  joptions[name] = def;
}
