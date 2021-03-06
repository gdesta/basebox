/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <deque>
#include <string>
#include <vector>
#include <map>

#include <rofl/common/cpacket.h>

#include "roflibs/netlink/ctapdev.hpp"

namespace rofcore {

class tap_callback {
public:
  virtual ~tap_callback(){};
  virtual int enqueue(rofcore::ctapdev *, rofl::cpacket *) = 0;
};

class tap_manager final {

public:
  tap_manager(){};
  ~tap_manager();

  /**
   * create tap devices for each unique name in queue.
   *
   * @return: pairs of ID,dev_name of the created devices
   */
  std::deque<std::pair<int, std::string>>
  register_tapdevs(std::deque<std::string> &, tap_callback &);

  void start();

  void stop();

  void destroy_tapdevs();

  ctapdev &get_dev(int i) { return *devs[i]; }

private:
  tap_manager(const tap_manager &other) = delete; // non construction-copyable
  tap_manager &operator=(const tap_manager &) = delete; // non copyable

  int create_tapdev(const std::string &, tap_callback &);

  std::vector<ctapdev *> devs;
  std::map<std::string, int> devname_to_spot;
};

} // namespace rofcore
