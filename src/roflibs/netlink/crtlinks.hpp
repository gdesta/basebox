/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CRTLINKS_H_
#define CRTLINKS_H_

#include <iostream>
#include <list>
#include <map>

#include <glog/logging.h>
#include <roflibs/netlink/crtlink.hpp>

namespace rofcore {

class crtlinks {
public:
  /**
   *
   */
  crtlinks(){};

  /**
   *
   */
  virtual ~crtlinks(){};

  /**
   *
   */
  crtlinks(const crtlinks &rtlinks) { *this = rtlinks; };

  /**
   *
   */
  crtlinks &operator=(const crtlinks &rtlinks) {
    if (this == &rtlinks)
      return *this;

    clear();
    for (std::map<unsigned int, crtlink>::const_iterator it =
             rtlinks.rtlinks.begin();
         it != rtlinks.rtlinks.end(); ++it) {
      add_link(it->first) = it->second;
    }

    return *this;
  };

public:
  /**
   *
   */
  void clear() { rtlinks.clear(); };

  size_t size() const { return rtlinks.size(); }

  /**
   *
   */
  crtlink &add_link(const crtlink &rtlink) {
    return (rtlinks[rtlink.get_ifindex()] = rtlink);
  };

  /**
   *
   */
  crtlink &set_link(const crtlink &rtlink) {
    return (rtlinks[rtlink.get_ifindex()] = rtlink);
  };

  /**
   *
   */
  crtlink &add_link(unsigned int ifindex) {
    if (rtlinks.find(ifindex) != rtlinks.end()) {
      rtlinks.erase(ifindex);
    }
    return rtlinks[ifindex];
  };

  /**
   *
   */
  crtlink &set_link(unsigned int ifindex) {
    if (rtlinks.find(ifindex) == rtlinks.end()) {
      rtlinks[ifindex];
    }
    return rtlinks[ifindex];
  };

  /**
   *
   */
  const crtlink &get_link(unsigned int ifindex) const {
    if (rtlinks.find(ifindex) == rtlinks.end()) {
      throw crtlink::eRtLinkNotFound(
          "crtlinks::get_link() / error: ifindex not found");
    }
    return rtlinks.at(ifindex);
  };

  /**
   *
   */
  void drop_link(unsigned int ifindex) {
    if (rtlinks.find(ifindex) == rtlinks.end()) {
      return;
    }
    rtlinks.erase(ifindex);
  };

  /**
   *
   */
  bool has_link(unsigned int ifindex) const {
    return (not(rtlinks.find(ifindex) == rtlinks.end()));
  };

  /**
   *
   */
  crtlink &set_link(const std::string &devname) {
    std::map<unsigned int, crtlink>::const_iterator it;
    if ((it = find_if(rtlinks.begin(), rtlinks.end(),
                      crtlink::crtlink_find_by_devname(devname))) ==
        rtlinks.end()) {
      throw crtlink::eRtLinkNotFound(
          "crtlinks::set_link() / error: devname not found");
    }
    return rtlinks[it->first];
  };

  /**
   *
   */
  const crtlink &get_link(const std::string &devname) const {
    std::map<unsigned int, crtlink>::const_iterator it;
    if ((it = find_if(rtlinks.begin(), rtlinks.end(),
                      crtlink::crtlink_find_by_devname(devname))) ==
        rtlinks.end()) {
      throw crtlink::eRtLinkNotFound(
          "crtlinks::get_link() / error: devname not found");
    }
    return rtlinks.at(it->first);
  };

  /**
   *
   */
  void drop_link(const std::string &devname) {
    std::map<unsigned int, crtlink>::const_iterator it;
    if ((it = find_if(rtlinks.begin(), rtlinks.end(),
                      crtlink::crtlink_find_by_devname(devname))) ==
        rtlinks.end()) {
      return;
    }
    rtlinks.erase(it->first);
  };

  /**
   *
   */
  bool has_link(const std::string &devname) const {
    std::map<unsigned int, crtlink>::const_iterator it;
    if ((it = find_if(rtlinks.begin(), rtlinks.end(),
                      crtlink::crtlink_find_by_devname(devname))) ==
        rtlinks.end()) {
      return false;
    }
    return true;
  }

  std::list<unsigned int> keys() const {
    std::list<unsigned int> keys;

    for (const auto &i : rtlinks) {
      keys.push_back(i.first);
    }

    return keys;
  }

public:
  friend std::ostream &operator<<(std::ostream &os, const crtlinks &rtlinks) {
    os << "<crtlinks #rtlinks: " << rtlinks.rtlinks.size() << " >" << std::endl;
    for (std::map<unsigned int, crtlink>::const_iterator it =
             rtlinks.rtlinks.begin();
         it != rtlinks.rtlinks.end(); ++it) {
      os << it->second;
    }
    return os;
  };

  std::string str() const {
    std::stringstream ss;
    for (std::map<unsigned int, crtlink>::const_iterator it = rtlinks.begin();
         it != rtlinks.end(); ++it) {
      ss << it->second.str() << std::endl;
    }
    return ss.str();
  };

  const std::map<unsigned int, crtlink> &get_all_links() { return rtlinks; }

private:
  std::map<unsigned int, crtlink> rtlinks;
};

}; // end of namespace

#endif /* CRTLINKS_H_ */
