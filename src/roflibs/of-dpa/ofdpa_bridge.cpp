/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ofdpa_bridge.hpp"

#include "roflibs/netlink/clogging.hpp"

#include <cassert>
#include <map>
#include <cstring>

#include <rofl/common/openflow/cofport.h>

namespace basebox {

ofdpa_bridge::ofdpa_bridge(rofl::rofl_ofdpa_fm_driver &fm_driver)
    : ingress_vlan_filtered(true), egress_vlan_filtered(false),
      fm_driver(fm_driver) {}

ofdpa_bridge::~ofdpa_bridge() {}

void ofdpa_bridge::set_bridge_interface(const rofcore::crtlink &rtl) {
  if (AF_BRIDGE != rtl.get_family() || 0 != rtl.get_master()) {
    rofcore::logging::error << __PRETTY_FUNCTION__
                            << " not a bridge master: " << rtl << std::endl;
    return;
  }

  this->bridge = rtl;
}

void ofdpa_bridge::apply_default_rules(rofl::crofdpt &dpt) {
  using rofcore::logging;

  if (0 == bridge.get_ifindex()) {
    logging::error << __PRETTY_FUNCTION__ << " no bridge interface set"
                   << std::endl;
    return;
  }

  fm_driver.enable_policy_arp(dpt, 1, 1);
  fm_driver.enable_policy_dhcp(dpt);
  fm_driver.enable_policy_vrrp(dpt);
}

static int find_next_bit(int i, uint32_t x) {
  int j;

  if (i >= 32)
    return -1;

  /* find first bit */
  if (i < 0)
    return __builtin_ffs(x);

  /* mask off prior finds to get next */
  j = __builtin_ffs(x >> i);
  return j ? j + i : 0;
}

void ofdpa_bridge::add_interface(rofl::crofdpt &dpt,
                                 const rofcore::crtlink &rtl) {

  using rofcore::logging;

  // sanity checks
  if (0 == bridge.get_ifindex()) {
    logging::error << __PRETTY_FUNCTION__
                   << " cannot attach interface without bridge: " << rtl
                   << std::endl;
    return;
  }
  if (AF_BRIDGE != rtl.get_family()) {
    logging::error << __PRETTY_FUNCTION__ << rtl
                   << " is not a bridge interface " << std::endl;
    return;
  }
  if (bridge.get_ifindex() != rtl.get_master()) {
    logging::error << __PRETTY_FUNCTION__ << rtl
                   << " is not a slave of this bridge interface " << std::endl;
    return;
  }

  if (not ingress_vlan_filtered) {
    // ingress
    fm_driver.enable_port_vid_allow_all(dpt, rtl.get_devname());
  }

  if (not egress_vlan_filtered) {
    // egress
    uint32_t group =
        fm_driver.enable_port_unfiltered_egress(dpt, rtl.get_devname());
    l2_domain[0].push_back(group);
  }

  if (not ingress_vlan_filtered && not egress_vlan_filtered) {
    return;
  }

  const struct rtnl_link_bridge_vlan *br_vlan = rtl.get_br_vlan();

  for (int k = 0; k < RTNL_LINK_BRIDGE_VLAN_BITMAP_LEN; k++) {
    int base_bit;
    uint32_t a = br_vlan->vlan_bitmap[k];

    base_bit = k * 32;
    int i = -1;
    int done = 0;
    while (!done) {
      int j = find_next_bit(i, a);
      if (j > 0) {
        int vid = j - 1 + base_bit;
        bool egress_untagged = false;

        // check if egress is untagged
        if (br_vlan->untagged_bitmap[k] & 1 << (j - 1)) {
          egress_untagged = true;
        }

        if (egress_vlan_filtered) {
          uint32_t group = fm_driver.enable_port_vid_egress(
              dpt, rtl.get_devname(), vid, egress_untagged);
          assert(group && "invalid group identifier");
          if (rofl::openflow::OFPG_MAX == group) {
            logging::error << __PRETTY_FUNCTION__
                           << " failed to set vid on egress " << std::endl;
            i = j;
            continue;
          }
          l2_domain[vid].push_back(group);
        }

        if (ingress_vlan_filtered) {
          if (br_vlan->pvid == vid) {
            fm_driver.enable_port_pvid_ingress(dpt, rtl.get_devname(), vid);
          } else {
            fm_driver.enable_port_vid_ingress(dpt, rtl.get_devname(), vid);
          }
        }

        // // todo check if vid is okay as an id as well
        // group = fm_driver.enable_group_l2_multicast(vid, vid,
        //                                             l2_domain[vid],
        //                                             1 !=
        //                                             l2_domain[vid].size());
        //
        // if (1 == l2_domain[vid].size()) { // todo maybe unnecessary
        //   fm_driver.enable_policy_arp(vid, group);
        // }

        i = j;
      } else {
        done = 1;
      }
    }
  }
}

void ofdpa_bridge::update_vlans(rofl::crofdpt &dpt, const std::string &devname,
                                const rtnl_link_bridge_vlan *old_br_vlan,
                                const rtnl_link_bridge_vlan *new_br_vlan) {
  using rofcore::logging;
  for (int k = 0; k < RTNL_LINK_BRIDGE_VLAN_BITMAP_LEN; k++) {
    int base_bit;
    uint32_t a = old_br_vlan->vlan_bitmap[k];
    uint32_t b = new_br_vlan->vlan_bitmap[k];

    uint32_t c = old_br_vlan->untagged_bitmap[k];
    uint32_t d = new_br_vlan->untagged_bitmap[k];

    uint32_t vlan_diff = a ^ b;
    uint32_t untagged_diff = c ^ d;

    base_bit = k * 32;
    int i = -1;
    int done = 0;
    while (!done) {
      int j = find_next_bit(i, vlan_diff);
      if (j > 0) {
        // vlan added or removed
        int vid = j - 1 + base_bit;
        bool egress_untagged = false;

        // check if egress is untagged
        if (new_br_vlan->untagged_bitmap[k] & 1 << (j - 1)) {
          egress_untagged = true;

          // clear untagged_diff bit
          untagged_diff &= ~((uint32_t)1 << (j - 1));
        }

        if (new_br_vlan->vlan_bitmap[k] & 1 << (j - 1)) {
          // vlan added

          if (egress_vlan_filtered) {
            try {
              uint32_t group = fm_driver.enable_port_vid_egress(
                  dpt, devname, vid, egress_untagged);
              assert(group && "invalid group identifier");
              if (rofl::openflow::OFPG_MAX == group) {
                logging::error << __PRETTY_FUNCTION__
                               << " failed to set vid on egress" << std::endl;
                i = j;
                continue;
              }
              l2_domain[vid].push_back(group);
            } catch (std::exception &e) {
              logging::error << __PRETTY_FUNCTION__
                             << " caught error1:" << e.what() << std::endl;
            }
          }

          if (ingress_vlan_filtered) {
            try {
              if (new_br_vlan->pvid == vid) {
                // todo check for existing pvid?
                fm_driver.enable_port_pvid_ingress(dpt, devname, vid);
              } else {
                fm_driver.enable_port_vid_ingress(dpt, devname, vid);
              }
            } catch (std::exception &e) {
              logging::error << __PRETTY_FUNCTION__
                             << " caught error2:" << e.what() << std::endl;
            }
          }

          // todo check if vid is okay as an id as well
          // group = fm_driver.enable_group_l2_multicast(vid, vid,
          //                                             l2_domain[vid],
          //                                             1 !=
          //                                             l2_domain[vid].size());
          // // enable arp flooding as well
          // #if DISABLED_TO_TEST
          // if (1 == l2_domain[vid].size()) { // todo maybe unnecessary
          //   fm_driver.enable_policy_arp(vid, group);
          // }
          // #endif

        } else {
          // vlan removed

          if (ingress_vlan_filtered) {
            try {
              if (old_br_vlan->pvid == vid) {
                fm_driver.disable_port_pvid_ingress(dpt, devname, vid);
              } else {
                fm_driver.disable_port_vid_ingress(dpt, devname, vid);
              }
            } catch (std::exception &e) {
              logging::error << __PRETTY_FUNCTION__
                             << " caught error3:" << e.what() << std::endl;
            }
          }

          if (egress_vlan_filtered) {
            try {
              // delete all FM pointing to this group first
              fm_driver.remove_bridging_unicast_vlan_all(dpt, devname, vid);
              // make sure they are deleted
              fm_driver.send_barrier(dpt);
              uint32_t group = fm_driver.disable_port_vid_egress(
                  dpt, devname, vid, egress_untagged);
              assert(group && "invalid group identifier");
              if (rofl::openflow::OFPG_MAX == group) {
                logging::error << __PRETTY_FUNCTION__
                               << " failed to remove vid on egress"
                               << std::endl;
                i = j;
                continue;
              }
              l2_domain[vid].remove(group);
            } catch (std::exception &e) {
              logging::error << __PRETTY_FUNCTION__
                             << " caught error4:" << e.what() << std::endl;
            }
          }
        }

        i = j;
      } else {
        done = 1;
      }
    }

#if 0 // not yet implemented the update
		done = 0;
		i = -1;
		while (!done) {
			// vlan is existing, but swapping egress tagged/untagged
			int j = find_next_bit(i, untagged_diff);
			if (j > 0) {
				// egress untagged changed
				int vid = j - 1 + base_bit;
				bool egress_untagged = false;

				// check if egress is untagged
				if (new_br_vlan->untagged_bitmap[k] & 1 << (j-1)) {
					egress_untagged = true;
				}

				// XXX implement update
				fm_driver.update_port_vid_egress(devname, vid, egress_untagged);


				i = j;
			} else {
				done = 1;
			}
		}
#endif
  }
}

void ofdpa_bridge::update_interface(rofl::crofdpt &dpt,
                                    const rofcore::crtlink &oldlink,
                                    const rofcore::crtlink &newlink) {
  using rofcore::crtlink;
  using rofcore::logging;

  // sanity checks
  if (0 == bridge.get_ifindex()) {
    logging::error << __PRETTY_FUNCTION__
                   << " cannot update interface without bridge" << std::endl;
    return;
  }
  if (AF_BRIDGE != newlink.get_family()) {
    logging::error << __PRETTY_FUNCTION__ << newlink
                   << " is not a bridge interface" << std::endl;
    return;
  }
  // if (AF_BRIDGE != oldlink.get_family()) {
  //   logging::error << __PRETTY_FUNCTION__ << oldlink << " is
  //                     not a bridge interface" << std::endl;
  //   return;
  // }
  if (bridge.get_ifindex() != newlink.get_master()) {
    logging::error << __PRETTY_FUNCTION__ << newlink
                   << " is not a slave of this bridge interface" << std::endl;
    return;
  }
  if (bridge.get_ifindex() != oldlink.get_master()) {
    logging::error << __PRETTY_FUNCTION__ << newlink
                   << " is not a slave of this bridge interface" << std::endl;
    return;
  }

  if (newlink.get_devname().compare(oldlink.get_devname())) {
    logging::info << __PRETTY_FUNCTION__
                  << " interface rename currently ignored " << std::endl;
    // FIXME this has to be handled differently
    return;
  }

  // handle VLANs
  if (not ingress_vlan_filtered && not egress_vlan_filtered) {
    return;
  }

  if (not crtlink::are_br_vlan_equal(newlink.get_br_vlan(),
                                     oldlink.get_br_vlan())) {
    // vlan updated
    update_vlans(dpt, oldlink.get_devname(), oldlink.get_br_vlan(),
                 newlink.get_br_vlan());
  }
}

void ofdpa_bridge::delete_interface(rofl::crofdpt &dpt,
                                    const rofcore::crtlink &rtl) {
  using rofcore::crtlink;
  using rofcore::logging;

  // sanity checks
  if (0 == bridge.get_ifindex()) {
    logging::error << __PRETTY_FUNCTION__
                   << " cannot attach interface without bridge: " << rtl
                   << std::endl;
    return;
  }
  if (AF_BRIDGE != rtl.get_family()) {
    logging::error << __PRETTY_FUNCTION__ << rtl
                   << " is not a bridge interface " << std::endl;
    return;
  }
  if (bridge.get_ifindex() != rtl.get_master()) {
    logging::error << __PRETTY_FUNCTION__ << rtl
                   << " is not a slave of this bridge interface " << std::endl;
    return;
  }

  if (not ingress_vlan_filtered) {
    // ingress
    fm_driver.disable_port_vid_allow_all(dpt, rtl.get_devname());
  }

  if (not egress_vlan_filtered) {
    // egress
    // delete all FM pointing to this group first
    fm_driver.remove_bridging_unicast_vlan_all(dpt, rtl.get_devname(), -1);
    // make sure they are deleted
    fm_driver.send_barrier(dpt);
    uint32_t group =
        fm_driver.disable_port_unfiltered_egress(dpt, rtl.get_devname());
    l2_domain[0].remove(group);
  }

  if (not ingress_vlan_filtered && not egress_vlan_filtered) {
    return;
  }

  const struct rtnl_link_bridge_vlan *br_vlan = rtl.get_br_vlan();
  struct rtnl_link_bridge_vlan br_vlan_empty;
  memset(&br_vlan_empty, 0, sizeof(struct rtnl_link_bridge_vlan));

  if (not crtlink::are_br_vlan_equal(br_vlan, &br_vlan_empty)) {
    // vlan updated
    update_vlans(dpt, rtl.get_devname(), br_vlan, &br_vlan_empty);
  }
}

void ofdpa_bridge::add_mac_to_fdb(rofl::crofdpt &dpt, const uint32_t of_port_no,
                                  const uint16_t vlan,
                                  const rofl::cmacaddr &mac, bool permanent) {
  fm_driver.add_bridging_unicast_vlan(dpt, mac, vlan, of_port_no, permanent,
                                      egress_vlan_filtered);
}

void ofdpa_bridge::remove_mac_from_fdb(rofl::crofdpt &dpt,
                                       const uint32_t of_port_no, uint16_t vid,
                                       const rofl::cmacaddr &mac) {
  fm_driver.remove_bridging_unicast_vlan(dpt, mac, vid, of_port_no);
}

} /* namespace basebox */
