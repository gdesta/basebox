/*
 * cfib.cc
 *
 *  Created on: 15.07.2013
 *      Author: andreas
 */

#include <cfib.h>

using namespace ethercore;

std::map<uint64_t, std::map<uint16_t, cfib*> > cfib::fibs;


/*static*/
cfib&
cfib::get_fib(uint64_t dpid, uint16_t vid)
{
	if (cfib::fibs[dpid].find(vid) == cfib::fibs[dpid].end()) {
		throw eFibNotFound();
	}
	return *(cfib::fibs[dpid][vid]);
}


/*static*/
void
cfib::destroy_fibs(uint64_t dpid)
{
	while (not cfib::fibs[dpid].empty()) {
		delete (cfib::fibs[dpid].begin()->second);
	}
}


cfib::cfib(
		cfib_owner *fibowner,
		uint64_t dpid,
		uint16_t vid,
		uint8_t src_stage_table_id,
		uint8_t dst_stage_table_id) :
		fibowner(fibowner),
		dpid(dpid),
		vid(vid),
		src_stage_table_id(src_stage_table_id),
		dst_stage_table_id(dst_stage_table_id)
{
	if (NULL == fibowner) {
		throw eFibInval();
	}
	if (cfib::fibs[dpid].find(vid) != cfib::fibs[dpid].end()) {
		throw eFibExists();
	}
	cfib::fibs[dpid][vid] = this;

	flood_group_id = fibowner->get_idle_group_id();

	add_group_entry_flood(/*modify=*/false);

	add_flow_mod_flood();
}



cfib::~cfib()
{
	cfib::fibs[dpid].erase(vid);

	drop_flow_mod_flood();

	drop_group_entry_flood();

	fibowner->release_group_id(flood_group_id);
}


void
cfib::add_port(
		uint32_t portno,
		bool tagged)
{
	try {
		sport& port = sport::get_sport(dpid, portno);

		/* notify sport about new VLAN membership => setup of associated FlowTable and GroupTable entries */
		port.add_membership(vid, tagged);

		ports.insert(portno);

		add_group_entry_flood(/*modify=*/true);

	} catch (eSportNotFound& e) {
		logging::error << "cfib::add_port() sport instance for OF portno:" << (int)portno << " not found" << std::endl;
	} catch (eSportFailed& e) {
		logging::error << "cfib::add_port() adding membership to sport instance failed" << std::endl;
	}
}


void
cfib::drop_port(
		uint32_t portno)
{
	try {
		sport& port = sport::get_sport(dpid, portno);

		/* notify sport about new VLAN membership => setup of associated FlowTable and GroupTable entries */
		port.drop_membership(vid);

		ports.erase(portno);

		add_group_entry_flood(/*modify=*/true);

	} catch (eSportNotFound& e) {
		logging::error << "cfib::drop_port() sport instance for OF portno:" << (int)portno << " not found" << std::endl;
	} catch (eSportFailed& e) {
		logging::error << "cfib::drop_port() dropping membership from sport instance failed" << std::endl;
	}
}


void
cfib::reset()
{
	for (std::map<rofl::cmacaddr, cfibentry*>::iterator
			it = fibtable.begin(); it != fibtable.end(); ++it) {
		delete (it->second);
	}
	fibtable.clear();
}



void
cfib::fib_timer_expired(cfibentry *entry)
{
	if (fibtable.find(entry->get_lladdr()) != fibtable.end()) {
		fibtable.erase(entry->get_lladdr());
		delete entry;
	}

	std::cerr << "EXPIRED: " << *this << std::endl;
}



uint32_t
cfib::get_flood_group_id(uint16_t vid)
{
	return 0;
}



void
cfib::add_flow_mod_flood()
{
	rofl::crofbase *rofbase = fibowner->get_rofbase();
	rofl::cofdpt *dpt = rofbase->dpt_find(dpid);

	rofl::cflowentry fe(dpt->get_version());

	fe.set_command(OFPFC_ADD);
	fe.set_table_id(dst_stage_table_id);
	fe.set_priority(0x4000);
	fe.set_idle_timeout(0);
	fe.set_hard_timeout(0);

	fe.match.set_vlan_vid(rofl::coxmatch_ofb_vlan_vid::VLAN_TAG_MODE_NORMAL, vid);
	fe.instructions.next() = rofl::cofinst_apply_actions(dpt->get_version());
	fe.instructions.back().actions.next() = rofl::cofaction_group(dpt->get_version(), flood_group_id);

	rofbase->send_flow_mod_message(dpt, fe);
}



void
cfib::drop_flow_mod_flood()
{
	rofl::crofbase *rofbase = fibowner->get_rofbase();
	rofl::cofdpt *dpt = rofbase->dpt_find(dpid);

	rofl::cflowentry fe(dpt->get_version());

	fe.set_command(OFPFC_DELETE_STRICT);
	fe.set_table_id(dst_stage_table_id);
	fe.set_priority(0x4000);
	fe.set_idle_timeout(0);
	fe.set_hard_timeout(0);

	fe.match.set_vlan_vid(rofl::coxmatch_ofb_vlan_vid::VLAN_TAG_MODE_NORMAL, vid);
	fe.instructions.next() = rofl::cofinst_apply_actions(dpt->get_version());
	fe.instructions.back().actions.next() = rofl::cofaction_group(dpt->get_version(), flood_group_id);

	rofbase->send_flow_mod_message(dpt, fe);
}



void
cfib::add_group_entry_flood(
		bool modify)
{
	try {
		rofl::crofbase *rofbase = fibowner->get_rofbase();
		rofl::cofdpt *dpt = rofbase->dpt_find(dpid);

		rofl::cgroupentry ge(dpt->get_version());

		if (modify)
			ge.set_command(OFPGC_MODIFY);
		else
			ge.set_command(OFPGC_ADD);

		ge.set_type(OFPGT_ALL);
		ge.set_group_id(flood_group_id);

		/* what about group chaining here? this would be useful ... */
		for (std::set<uint32_t>::iterator
				it = ports.begin(); it != ports.end(); ++it) {
			sport& port = sport::get_sport(dpid, *it);


			ge.buckets.next() = rofl::cofbucket(dpt->get_version(), 0, 0, 0);
			// only pop vlan, when vid is used untagged on this port, i.e. vid == port.pvid
			try {
				if (port.get_pvid() == vid) {
					ge.buckets.back().actions.next() = rofl::cofaction_pop_vlan(dpt->get_version());
				}
			} catch (eSportNoPvid& e) {}
			ge.buckets.back().actions.next() = rofl::cofaction_output(dpt->get_version(), port.get_portno());
		}

		rofbase->send_group_mod_message(dpt, ge);

	} catch (...) {

	}
}



void
cfib::drop_group_entry_flood()
{
	try {
		rofl::crofbase *rofbase = fibowner->get_rofbase();
		rofl::cofdpt *dpt = rofbase->dpt_find(dpid);

		rofl::cgroupentry ge(dpt->get_version());

		ge.set_command(OFPGC_DELETE);
		ge.set_type(OFPGT_ALL);	// necessary for OFPGC_DELETE?
		ge.set_group_id(flood_group_id);

		rofbase->send_group_mod_message(dpt, ge);

	} catch (...) {

	}
}



void
cfib::handle_packet_in(rofl::cofmsg_packet_in& msg)
{
	try {
		/* get sport instance for in-port */
		uint32_t inport = msg.get_match().get_in_port();
		sport& port = sport::get_sport(dpid, inport);

		/* get eth-src and eth-dst */
		rofl::cmacaddr eth_src = msg.get_packet().ether()->get_dl_src();
		rofl::cmacaddr eth_dst = msg.get_packet().ether()->get_dl_dst();


		/* sanity check: if source mac is multicast => invalid frame */
		if (eth_src.is_multicast()) {
			return;
		}


		/* Packet-In generated by table containing eth-src checks => i.e. source host is unknown so far */
		if (msg.get_table_id() == src_stage_table_id) {

			/* no FIB entry so far, create new one */
			if (fibtable.find(eth_src) == fibtable.end()) {

				fibtable[eth_src] = new cfibentry(this,
						src_stage_table_id,
						dst_stage_table_id,
						eth_src,
						dpid,
						vid, inport, port.get_is_tagged(vid));

				logging::info << "[ethcore - fib] - creating new FIB entry: " << *(fibtable[eth_src]) << std::endl;



			/* either inport has changed or the FlowMod entry was removed => in either case, refresh entry */
			} else {

				fibtable[eth_src]->set_portno(inport);

				logging::info << "[ethcore - fib] - updating FIB entry: " << (*fibtable[eth_src]) << std::endl;
			}
		}
		/* Packet-In generated by table containing eth-dst entries => i.e. destination is not known yet (or entry has expired), flood it */
		else if (msg.get_table_id() == (dst_stage_table_id)) {
			// flood packet

			rofl::crofbase *rofbase = fibowner->get_rofbase();
			rofl::cofdpt *dpt = rofbase->dpt_find(dpid);

			rofl::cofaclist actions(dpt->get_version());
			actions.next() = rofl::cofaction_group(dpt->get_version(), flood_group_id);
		}


#if 0
		/*
		 * block mac address 01:80:c2:00:00:00
		 */
		if (msg.get_packet().ether()->get_dl_dst() == rofl::cmacaddr("01:80:c2:00:00:00") ||
			msg.get_packet().ether()->get_dl_dst() == rofl::cmacaddr("01:00:5e:00:00:fb")) {
			rofl::cflowentry fe(dpt->get_version());

			fe.set_command(OFPFC_ADD);
			fe.set_buffer_id(msg->get_buffer_id());
			fe.set_idle_timeout(15);
			fe.set_table_id(msg->get_table_id());

			fe.match.set_in_port(msg->get_match().get_in_port());
			fe.match.set_eth_dst(msg->get_packet().ether()->get_dl_dst());
			fe.instructions.next() = cofinst_apply_actions(dpt->get_version());

			logging::info << "ethercore: installing FLOW-MOD with entry: " << fe << std::endl;

			send_flow_mod_message(dpt, fe);

			delete msg; return;
		}
#endif

	} catch (rofl::eOFmatchNotFound& e) {

	}
}


