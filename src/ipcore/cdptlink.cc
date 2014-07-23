/*
 * cdptport.cc
 *
 *  Created on: 28.06.2013
 *      Author: andreas
 */

#include <dptlink.h>

using namespace ipcore;




cdptlink::cdptlink() :
				ofp_port_no(0),
				ifindex(0),
				tapdev(0),
				table_id(0)
{

}



cdptlink::~cdptlink()
{
	if (ifindex > 0) {
		tap_close();
	}
}



void
cdptlink::tap_open()
{
	if (NULL != tapdev) {
		// tap device already exists, ignoring
		return;
	}

	tapdev = new rofcore::ctapdev(this, get_devname(), get_hwaddr());

	ifindex = tapdev->get_ifindex();

	flags.set(FLAG_TAP_DEVICE_ACTIVE);
}



void
cdptlink::tap_close()
{
	if (tapdev) delete tapdev;

	tapdev = NULL;

	ifindex = -1;

	flags.reset(FLAG_TAP_DEVICE_ACTIVE);
}



void
cdptlink::install()
{
	for (std::map<uint16_t, cdptaddr_in4>::iterator
			it = dpt4addrs.begin(); it != dpt4addrs.end(); ++it) {
		it->second.install();
	}
	for (std::map<uint16_t, cdptaddr_in6>::iterator
			it = dpt6addrs.begin(); it != dpt6addrs.end(); ++it) {
		it->second.install();
	}
	for (std::map<uint16_t, cdptneigh_in4>::iterator
			it = dpt4neighs.begin(); it != dpt4neighs.end(); ++it) {
		it->second.install();
	}
	for (std::map<uint16_t, cdptneigh_in6>::iterator
			it = dpt6neighs.begin(); it != dpt6neighs.end(); ++it) {
		it->second.install();
	}
	flags.set(FLAG_FLOW_MOD_INSTALLED);
}



void
cdptlink::uninstall()
{
	for (std::map<uint16_t, cdptaddr_in4>::iterator
			it = dpt4addrs.begin(); it != dpt4addrs.end(); ++it) {
		it->second.uninstall();
	}
	for (std::map<uint16_t, cdptaddr_in6>::iterator
			it = dpt6addrs.begin(); it != dpt6addrs.end(); ++it) {
		it->second.uninstall();
	}
	for (std::map<uint16_t, cdptneigh_in4>::iterator
			it = dpt4neighs.begin(); it != dpt4neighs.end(); ++it) {
		it->second.uninstall();
	}
	for (std::map<uint16_t, cdptneigh_in6>::iterator
			it = dpt6neighs.begin(); it != dpt6neighs.end(); ++it) {
		it->second.uninstall();
	}
	flags.reset(FLAG_FLOW_MOD_INSTALLED);
}



void
cdptlink::delete_all_addrs()
{
	for (std::map<uint16_t, cdptaddr_in4>::iterator
			it = dpt4addrs.begin(); it != dpt4addrs.end(); ++it) {
		it->second.uninstall();
	}
	dpt4addrs.clear();
	for (std::map<uint16_t, cdptaddr_in6>::iterator
			it = dpt6addrs.begin(); it != dpt6addrs.end(); ++it) {
		it->second.uninstall();
	}
	dpt6addrs.clear();
}



void
cdptlink::delete_all_neighs()
{
	for (std::map<uint16_t, cdptneigh_in4>::iterator
			it = dpt4neighs.begin(); it != dpt4neighs.end(); ++it) {
		it->second.uninstall();
	}
	dpt4neighs.clear();
	for (std::map<uint16_t, cdptneigh_in6>::iterator
			it = dpt6neighs.begin(); it != dpt6neighs.end(); ++it) {
		it->second.uninstall();
	}
	dpt6neighs.clear();
}



void
cdptlink::enqueue(rofcore::cnetdev *netdev, rofl::cpacket* pkt)
{
	try {
		if (not rofl::crofdpt::get_dpt(dptid).get_channel().is_established())
			throw eDptLinkNoDptAttached();
		}

		rofcore::ctapdev* tapdev = dynamic_cast<rofcore::ctapdev*>( netdev );
		if (0 == tapdev) {
			throw eDptLinkTapDevNotFound();
		}

		rofl::openflow::cofactions actions(rofl::crofdpt::get_dpt(dptid).get_version());
		actions.set_action_output(rofl::cindex(0)).set_port_no(ofp_port_no);

		rofl::crofdpt::get_dpt(dptid).send_packet_out_message(
				rofl::cauxid(0),
				rofl::openflow::base::get_ofp_no_buffer(rofl::crofdpt::get_dpt(dptid).get_version()),
				rofl::openflow::base::get_ofpp_controller_port(rofl::crofdpt::get_dpt(dptid).get_version()),
				actions,
				pkt->soframe(),
				pkt->framelen());

	} catch (eDptLinkNoDptAttached& e) {
		rofcore::logging::warn << "[ipcore][dptlink][enqueue] no data path attached, dropping outgoing packet" << std::endl;

	} catch (eDptLinkTapDevNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][enqueue] unable to find tap device" << std::endl;
	}

	rofcore::cpacketpool::get_instance().release_pkt(pkt);
}



void
cdptlink::enqueue(rofcore::cnetdev *netdev, std::vector<rofl::cpacket*> pkts)
{
	for (std::vector<rofl::cpacket*>::iterator
			it = pkts.begin(); it != pkts.end(); ++it) {
		enqueue(netdev, *it);
	}
}



void
cdptlink::handle_packet_in(rofl::cpacket const& pack)
{
	if (0 == tapdev)
		return;

	rofl::cpacket *pkt = rofcore::cpacketpool::get_instance().acquire_pkt();

	*pkt = pack;

	tapdev->enqueue(pkt);
}



void
cdptlink::handle_port_status()
{
	try {
		if (0 == tapdev)
			return;

		if (get_ofp_port().link_state_is_link_down() || get_ofp_port().config_is_port_down()) {
			tapdev->disable_interface();
		} else {
			tapdev->enable_interface();
		}

	} catch (rofl::openflow::ePortNotFound& e) {

	}
}



void
cdptlink::clear()
{
	delete_all_neighs();

	delete_all_addrs();
}



cdptaddr_in4&
cdptlink::add_addr_in4(uint16_t adindex)
{
	if (dpt4addrs.find(adindex) != dpt4addrs.end()) {
		dpt4addrs.erase(adindex);
	}
	return dpt4addrs[adindex];
}



cdptaddr_in4&
cdptlink::set_addr_in4(uint16_t adindex)
{
	if (dpt4addrs.find(adindex) == dpt4addrs.end()) {
		(void)dpt4addrs[adindex];
	}
	return dpt4addrs[adindex];
}



const cdptaddr_in4&
cdptlink::get_addr_in4(uint16_t adindex) const
{
	if (dpt4addrs.find(adindex) == dpt4addrs.end()) {
		throw eDptLinkNotFound();
	}
	return dpt4addrs.at(adindex);
}



void
cdptlink::drop_addr_in4(uint16_t adindex)
{
	if (dpt4addrs.find(adindex) == dpt4addrs.end()) {
		return;
	}
	dpt4addrs.erase(adindex);
}



bool
cdptlink::has_addr_in4(uint16_t adindex) const
{
	return (not (dpt4addrs.find(adindex) == dpt4addrs.end()));
}



cdptaddr_in6&
cdptlink::add_addr_in6(uint16_t adindex)
{
	if (dpt6addrs.find(adindex) != dpt6addrs.end()) {
		dpt6addrs.erase(adindex);
	}
	return dpt6addrs[adindex];
}



cdptaddr_in6&
cdptlink::set_addr_in6(uint16_t adindex)
{
	if (dpt6addrs.find(adindex) == dpt6addrs.end()) {
		(void)dpt6addrs[adindex];
	}
	return dpt6addrs[adindex];
}



const cdptaddr_in6&
cdptlink::get_addr_in6(uint16_t adindex) const
{
	if (dpt6addrs.find(adindex) == dpt6addrs.end()) {
		throw eDptLinkNotFound();
	}
	return dpt6addrs.at(adindex);
}



void
cdptlink::drop_addr_in6(uint16_t adindex)
{
	if (dpt6addrs.find(adindex) == dpt6addrs.end()) {
		return;
	}
	dpt6addrs.erase(adindex);
}



bool
cdptlink::has_addr_in6(uint16_t adindex) const
{
	return (not (dpt6addrs.find(adindex) == dpt6addrs.end()));
}



cdptneigh_in4&
cdptlink::add_neigh_in4(uint16_t adindex)
{
	if (dpt4neighs.find(adindex) != dpt4neighs.end()) {
		dpt4neighs.erase(adindex);
	}
	return dpt4neighs[adindex];
}



cdptneigh_in4&
cdptlink::set_neigh_in4(uint16_t adindex)
{
	if (dpt4neighs.find(adindex) == dpt4neighs.end()) {
		(void)dpt4neighs[adindex];
	}
	return dpt4neighs[adindex];
}



const cdptneigh_in4&
cdptlink::get_neigh_in4(uint16_t adindex) const
{
	if (dpt4neighs.find(adindex) == dpt4neighs.end()) {
		throw eDptLinkNotFound();
	}
	return dpt4neighs.at(adindex);
}



void
cdptlink::drop_neigh_in4(uint16_t adindex)
{
	if (dpt4neighs.find(adindex) == dpt4neighs.end()) {
		return;
	}
	dpt4neighs.erase(adindex);
}



bool
cdptlink::has_neigh_in4(uint16_t adindex) const
{
	return (not (dpt4neighs.find(adindex) == dpt4neighs.end()));
}



cdptneigh_in6&
cdptlink::add_neigh_in6(uint16_t adindex)
{
	if (dpt6neighs.find(adindex) != dpt6neighs.end()) {
		dpt6neighs.erase(adindex);
	}
	return dpt6neighs[adindex];
}



cdptneigh_in6&
cdptlink::set_neigh_in6(uint16_t adindex)
{
	if (dpt6neighs.find(adindex) == dpt6neighs.end()) {
		(void)dpt6neighs[adindex];
	}
	return dpt6neighs[adindex];
}



const cdptneigh_in6&
cdptlink::get_neigh_in6(uint16_t adindex) const
{
	if (dpt6neighs.find(adindex) == dpt6neighs.end()) {
		throw eDptLinkNotFound();
	}
	return dpt6neighs.at(adindex);
}



void
cdptlink::drop_neigh_in6(uint16_t adindex)
{
	if (dpt6neighs.find(adindex) == dpt6neighs.end()) {
		return;
	}
	dpt6neighs.erase(adindex);
}



bool
cdptlink::has_neigh_in6(uint16_t adindex) const
{
	return (not (dpt6neighs.find(adindex) == dpt6neighs.end()));
}




void
cdptlink::addr_in4_created(unsigned int ifindex, uint16_t adindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex) {
			return;
		}

		add_addr_in4(adindex).set_dptid(dptid);	// reset any existing instance
		set_addr_in4(adindex).set_ifindex(ifindex);
		set_addr_in4(adindex).set_adindex(adindex);
		set_addr_in4(adindex).set_table_id(table_id);
		set_addr_in4(adindex).install();

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in4_created] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in4_created] address not found, "
				"adindex:" << adindex << std::endl

	}
}



void
cdptlink::addr_in4_updated(unsigned int ifindex, uint16_t adindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex)
			return;

		if (not has_addr_in4(adindex)) {
			return;
		}

		// check status changes and notify dptaddr instance
		set_addr_in4(adindex).update();
		set_addr_in4(adindex).reinstall();

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in4_updated] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in4_updated] address not found, "
				"adindex:" << adindex << std::endl

	}
}



void
cdptlink::addr_in4_deleted(unsigned int ifindex, uint16_t adindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex)
			return;

		if (not has_addr_in4(adindex)) {
			return;
		}

		set_addr_in4(adindex).uninstall();
		drop_addr_in4(adindex);

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in4_deleted] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in4_deleted] address not found, "
				"adindex:" << adindex << std::endl

	}
}





void
cdptlink::addr_in6_created(unsigned int ifindex, uint16_t adindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex) {
			return;
		}

		add_addr_in6(adindex).set_dptid(dptid);	// reset any existing instance
		set_addr_in6(adindex).set_ifindex(ifindex);
		set_addr_in6(adindex).set_adindex(adindex);
		set_addr_in6(adindex).set_table_id(table_id);
		set_addr_in6(adindex).install();

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in6_created] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in6_created] address not found, "
				"adindex:" << adindex << std::endl

	}
}



void
cdptlink::addr_in6_updated(unsigned int ifindex, uint16_t adindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex)
			return;

		if (not has_addr_in6(adindex)) {
			return;
		}

		// check status changes and notify dptaddr instance
		set_addr_in6(adindex).update();
		set_addr_in6(adindex).reinstall();

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in6_updated] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in6_updated] address not found, "
				"adindex:" << adindex << std::endl

	}

}



void
cdptlink::addr_in6_deleted(unsigned int ifindex, uint16_t adindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex)
			return;

		if (not has_addr_in6(adindex)) {
			return;
		}

		set_addr_in6(adindex).uninstall();
		drop_addr_in6(adindex);

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in6_deleted] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][addr_in6_deleted] address not found, "
				"adindex:" << adindex << std::endl

	}
}




void
cdptlink::neigh_in4_created(unsigned int ifindex, uint16_t nbindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex) {
			return;
		}

		add_neigh_in4(nbindex).set_dptid(dptid);	// reset any existing instance
		set_neigh_in4(nbindex).set_ofp_port_no(ofp_port_no);
		set_neigh_in4(nbindex).set_ifindex(ifindex);
		set_neigh_in4(nbindex).set_nbindex(nbindex);
		set_neigh_in4(nbindex).set_table_id(2); // table-id: 2
		set_neigh_in4(nbindex).install();

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in4_created] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in4_created] address not found, "
				"nbindex:" << nbindex << std::endl

	}
}



void
cdptlink::neigh_in4_updated(unsigned int ifindex, uint16_t nbindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex)
			return;

		rofcore::crtneigh_in4& rtn = rofcore::cnetlink::get_instance().get_link(ifindex).get_neigh_in4(nbindex);
		// check status changes and notify dptneigh instance
		switch (rtn.get_state()) {
		case NUD_INCOMPLETE:
		case NUD_DELAY:
		case NUD_PROBE:
		case NUD_FAILED: {

			if (not has_neigh_in4(nbindex)) {
				return;
			}

			rofl::logging::info << "[ipcore][dptlink] crtneigh UPDATE (scheduled for removal):" << std::endl
					<< rofcore::cnetlink::get_instance().get_link(ifindex).get_neigh_in4(nbindex);

			set_neigh_in4(nbindex).uninstall();
			drop_neigh_in4(nbindex);

		} break;

		case NUD_STALE:
		case NUD_NOARP:
		case NUD_REACHABLE:
		case NUD_PERMANENT: {

			rofl::logging::info << "[ipcore][dptlink] crtneigh UPDATE (refresh):" << std::endl
					<< rofcore::cnetlink::get_instance().get_link(ifindex).get_neigh_in4(nbindex);

			set_neigh_in4(nbindex).update();
			set_neigh_in4(nbindex).reinstall();

		} break;
		}

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in4_updated] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in4_updated] neigh address not found, "
				"nbindex:" << nbindex << std::endl

	}
}



void
cdptlink::neigh_in4_deleted(unsigned int ifindex, uint16_t nbindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex)
			return;

		if (not has_neigh_in4(nbindex)) {
			return;
		}

		set_neigh_in4(nbindex).uninstall();
		drop_neigh_in4(nbindex);

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in4_deleted] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in4_deleted] address not found, "
				"nbindex:" << nbindex << std::endl

	}
}





void
cdptlink::neigh_in6_created(unsigned int ifindex, uint16_t nbindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex) {
			return;
		}

		add_neigh_in6(nbindex).set_dptid(dptid);	// reset any existing instance
		set_neigh_in6(nbindex).set_ofp_port_no(ofp_port_no);
		set_neigh_in6(nbindex).set_ifindex(ifindex);
		set_neigh_in6(nbindex).set_nbindex(nbindex);
		set_neigh_in6(nbindex).set_table_id(2); // table-id: 2
		set_neigh_in6(nbindex).install();

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in6_created] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in6_created] address not found, "
				"nbindex:" << nbindex << std::endl

	}
}



void
cdptlink::neigh_in6_updated(unsigned int ifindex, uint16_t nbindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex)
			return;

		rofcore::crtneigh_in6& rtn = rofcore::cnetlink::get_instance().get_link(ifindex).get_neigh_in6(nbindex);
		// check status changes and notify dptneigh instance
		switch (rtn.get_state()) {
		case NUD_INCOMPLETE:
		case NUD_DELAY:
		case NUD_PROBE:
		case NUD_FAILED: {

			if (not has_neigh_in6(nbindex)) {
				return;
			}

			rofl::logging::info << "[ipcore][dptlink] crtneigh UPDATE (scheduled for removal):" << std::endl
					<< rofcore::cnetlink::get_instance().get_link(ifindex).get_neigh_in6(nbindex);

			set_neigh_in6(nbindex).uninstall();
			drop_neigh_in6(nbindex);

		} break;

		case NUD_STALE:
		case NUD_NOARP:
		case NUD_REACHABLE:
		case NUD_PERMANENT: {

			rofl::logging::info << "[ipcore][dptlink] crtneigh UPDATE (refresh):" << std::endl
					<< rofcore::cnetlink::get_instance().get_link(ifindex).get_neigh_in6(nbindex);

			set_neigh_in6(nbindex).update();
			set_neigh_in6(nbindex).reinstall();

		} break;
		}

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in6_updated] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in6_updated] neigh address not found, "
				"nbindex:" << nbindex << std::endl

	}
}



void
cdptlink::neigh_in6_deleted(unsigned int ifindex, uint16_t nbindex)
{
	try {
		// filter out any events not related to our port
		if (ifindex != this->ifindex)
			return;

		if (not has_neigh_in6(nbindex)) {
			return;
		}

		set_neigh_in6(nbindex).uninstall();
		drop_neigh_in6(nbindex);

	} catch (rofcore::eNetLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in6_deleted] link not found, "
				"ifindex:" << ifindex << std::endl

	} catch (rofcore::eRtLinkNotFound& e) {
		rofcore::logging::warn << "[ipcore][dptlink][neigh_in6_deleted] address not found, "
				"nbindex:" << nbindex << std::endl

	}
}



