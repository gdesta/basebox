/*
 * cethendpnt.hpp
 *
 *  Created on: 04.08.2014
 *      Author: andreas
 */

#ifndef CETHENDPNT_HPP_
#define CETHENDPNT_HPP_

#include <inttypes.h>
#include <exception>
#include <iostream>

#include <rofl/common/crofdpt.h>
#include <rofl/common/cdptid.h>
#include <rofl/common/protocols/fvlanframe.h>

#include "roflibs/netlink/clogging.hpp"
#include "roflibs/netlink/ccookiebox.hpp"

namespace roflibs {
namespace eth {

class eEthEndpntBase : public std::runtime_error {
public:
  eEthEndpntBase(const std::string &__arg) : std::runtime_error(__arg){};
};
class eEthEndpntNotFound : public eEthEndpntBase {
public:
  eEthEndpntNotFound(const std::string &__arg) : eEthEndpntBase(__arg){};
};

class cethendpnt : public roflibs::common::openflow::ccookie_owner {
public:
  /**
   *
   */
  cethendpnt()
      : dpt_state(STATE_IDLE),
        cookie_endpoint(
            roflibs::common::openflow::ccookie_owner::acquire_cookie()),
        vid(0xffff), tagged(false), table_id_eth_local(0){};

  /**
   *
   */
  cethendpnt(const rofl::cdptid &dptid, uint8_t local_stage_table_id,
             const rofl::cmacaddr &hwaddr, uint16_t vid = 0xffff,
             bool tagged = true)
      : dpt_state(STATE_IDLE), dptid(dptid),
        cookie_endpoint(
            roflibs::common::openflow::ccookie_owner::acquire_cookie()),
        vid(vid), tagged(tagged), table_id_eth_local(local_stage_table_id),
        hwaddr(hwaddr){};

  /**
   *
   */
  virtual ~cethendpnt() {
    try {
      if (STATE_ATTACHED == dpt_state) {
        handle_dpt_close(rofl::crofdpt::get_dpt(dptid));
      }
    } catch (rofl::eRofDptNotFound &e) {
    };
  };

  /**
   *
   */
  cethendpnt(const cethendpnt &port)
      : cookie_endpoint(
            roflibs::common::openflow::ccookie_owner::acquire_cookie()) {
    *this = port;
  };

  /**
   *
   */
  cethendpnt &operator=(const cethendpnt &port) {
    if (this == &port)
      return *this;
    dpt_state = port.dpt_state;
    dptid = port.dptid;
    // do not copy cookies here!
    vid = port.vid;
    tagged = port.tagged;
    table_id_eth_local = port.table_id_eth_local;
    hwaddr = port.hwaddr;
    return *this;
  };

public:
  /**
   *
   */
  const rofl::cmacaddr &get_hwaddr() const { return hwaddr; };

  /**
   *
   */
  bool get_tagged() const { return tagged; };

  /**
   *
   */
  uint16_t get_vid() const { return vid; };

public:
  /**
   *
   */
  void handle_dpt_open(rofl::crofdpt &dpt);

  /**
   *
   */
  void handle_dpt_close(rofl::crofdpt &dpt);

  /**
   *
   */
  virtual void handle_packet_in(rofl::crofdpt &dpt, const rofl::cauxid &auxid,
                                rofl::openflow::cofmsg_packet_in &msg);

  /**
   *
   */
  virtual void handle_flow_removed(rofl::crofdpt &dpt,
                                   const rofl::cauxid &auxid,
                                   rofl::openflow::cofmsg_flow_removed &msg);

  /**
   *
   */
  void handle_port_status(rofl::crofdpt &dpt, const rofl::cauxid &auxid,
                          rofl::openflow::cofmsg_port_status &msg);

  /**
   *
   */
  void handle_error_message(rofl::crofdpt &dpt, const rofl::cauxid &auxid,
                            rofl::openflow::cofmsg_error &msg);

public:
  friend std::ostream &operator<<(std::ostream &os, const cethendpnt &port) {
    os << rofcore::indent(0) << "<cethendpnt "
       << "hwaddr: " << port.get_hwaddr().str() << " "
       << "tagged: " << (int)port.get_tagged() << " "
       << ">" << std::endl;
    return os;
  };

private:
  enum dpt_state_t {
    STATE_IDLE = 1,
    STATE_DETACHED = 2,
    STATE_ATTACHED = 3,
  };

  dpt_state_t dpt_state;

  rofl::cdptid dptid;
  uint64_t cookie_endpoint;
  uint16_t vid;
  bool tagged;
  uint8_t table_id_eth_local; // MAC addresses assigned to local host
  rofl::cmacaddr hwaddr;
};

}; // end of namespace ethernet
}; // end of namespace roflibs

#endif /* CETHENDPNT_HPP_ */
