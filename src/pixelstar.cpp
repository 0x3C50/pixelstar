//
// Created by x150 on 01 Feb. 2025.
//

#include "pixelstar.h"
#include <seastar/core/sleep.hh>

#include <seastar/core/loop.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/sharded.hh>
#include <seastar/core/thread.hh>

// #define DO_PXS_DBG

#ifdef DO_PXS_DBG
#define PXS_DBG(what) what;
#else
#define PXS_DBG(what)
#endif
// ceil(x/y) for x,y > 0
#define POSITIVE_CEILDIV_NZ(x, y) ((x+y - 1) / y)
#define PXS_SBTOUB(x) static_cast<unsigned char>(x)
#define PXS_UNPACK_2B_UINT16(from, i) static_cast<uint16_t>((PXS_SBTOUB(from[i]) << 8) | (PXS_SBTOUB(from[i+1])))
#define PXS_UNPACK_4B_UINT32(from, i) static_cast<uint32_t>((PXS_UNPACK_2B_UINT16(from, i) << 16) | PXS_UNPACK_2B_UINT16(from, i+2))

seastar::sharded<pixelstar::service_shard> service;

seastar::logger pxslog("main");

inline uint8_t lerpInt(const uint8_t a, const uint8_t b, const float delta) noexcept {
	const uint8_t diff = b - a;
	return a + diff * delta;
}

seastar::future<> pixelstar::main() {
	constexpr size_t width = 64;
	constexpr size_t height = 64;
	// constexpr size_t bufferSize = width * height;
	uint32_t *buffer = new uint32_t[width * height];
	global_state gs{buffer, width, height};
	return seastar::async([gs] {
		seastar::promise<> closeFuture;
		seastar::engine().handle_signal(SIGINT, [&closeFuture] {
			pxslog.info("received sigint");
			closeFuture.set_value();
		});
		seastar::engine().handle_signal(SIGTERM, [&closeFuture] {
			pxslog.info("received sigterm");
			closeFuture.set_value();
		});
		service.start(&gs).wait();

		// unsigned int nCores = seastar::smp::count;
		// size_t bufferSizeOfOne = POSITIVE_CEILDIV_NZ(bufferSize, nCores);
		// uint32_t *currentRegionStart = gs.buffer;
		// const uint32_t *bufferEnd = gs.buffer + bufferSize;
		// pxslog.info("we have {} cores, one core will manage at most {} pixels starting at {} ending at {}", nCores,
		// 			bufferSizeOfOne, static_cast<void *>(currentRegionStart), static_cast<const void *>(bufferEnd));
		// for (unsigned int thisCore = 0; thisCore < nCores; thisCore++) {
		// 	size_t thisRegionSize = std::min(bufferSizeOfOne, static_cast<size_t>(bufferEnd - currentRegionStart));
		// 	service.invoke_on(thisCore, [currentRegionStart, thisRegionSize](service_shard &s) {
		// 		pxslog.info("core {} region starts at {} ends at {}, contains {} pixels", seastar::this_shard_id(),
		// 					static_cast<void *>(currentRegionStart),
		// 					static_cast<const void *>(currentRegionStart + thisRegionSize), thisRegionSize);
		// 		s.init_state(currentRegionStart, thisRegionSize);
		// 		return seastar::make_ready_future();
		// 	}).wait();
		// 	currentRegionStart += thisRegionSize;
		// }

		seastar::future<> listenDone = service.invoke_on_all([](service_shard &shard) {
			return shard.run();
		});

		pxslog.info("init done, waiting for signal to stop");
		auto closeFutReal = closeFuture.get_future();
		closeFutReal.wait();
		// alright we got a sigint, lets end it
		service.stop().wait();
		// we're done
		listenDone.wait();
	}).finally([buffer] {
		pxslog.info("freeing buffer");
		delete[] buffer;
	});
}

seastar::socket_address sa = seastar::make_ipv4_address({1235});


// static const char* ERR_OOB = "Coordinate out of bounds";

seastar::future<> pixelstar::service_shard::handle_connection(seastar::connected_socket s) {
	seastar::output_stream<char> t_out = s.output();
	seastar::input_stream<char> t_in = s.input();
	return do_with(std::move(s), std::move(t_out), std::move(t_in),
					[this](seastar::connected_socket &p_s, seastar::output_stream<char> &out, seastar::input_stream<char> &in) {
						return seastar::repeat([this, &out, &in] {
							return with_gate(this->gate, [this, &out, &in] -> seastar::future<seastar::stop_iteration> {
								// see protocol.txt
								return in.read_exactly(2 + 2 + 1 + 4).then([this, &out, &in] (const seastar::temporary_buffer<char> &read_packet) -> seastar::future<seastar::stop_iteration> {
									// if no data or less data than wanted (read_exactly returns less data than expected if EOF)
									if (!read_packet || read_packet.size() != (2+2+1+4)) return seastar::make_ready_future<seastar::stop_iteration>(seastar::stop_iteration::yes);
									const uint16_t x = PXS_UNPACK_2B_UINT16(read_packet, 0);
									const uint16_t y = PXS_UNPACK_2B_UINT16(read_packet, 2);
									PXS_DBG(pxslog.info("packet talks about: {} {}", x, y))
									if (x >= this->state->width || y >= this->state->height) {
										PXS_DBG(pxslog.info("which is oob"))
										return seastar::make_ready_future<seastar::stop_iteration>(seastar::stop_iteration::no);
									}
									size_t index = this->state->width * y + x;
									PXS_DBG(pxslog.info("idx: {}", index))
									// rearranged version of index / length * coreCount, so we dont have to do float math.
									// if len = 64, index = 31, coreCount = 8, then this returns 3, being the core responsible for that index.
									// index = 32 returns 4, 7 returns 0, 8 returns 1, as expected
									// unsigned int core_index = index * seastar::smp::count / (this->state->width * this->state->height);
									// PXS_DBG(pxslog.info("core responsible: {}", core_index))
									return handle_packet(index, out, read_packet).then([] {return seastar::stop_iteration::no;});
								});
							});
						}).then([&out] {
							return out.close();
						});
					});
}

seastar::future<> pixelstar::service_shard::handle_packet(size_t abs_index, seastar::output_stream<char> &out,
                                                          const seastar::temporary_buffer<char> &read_packet) const {
	size_t our_index = abs_index;
	PXS_DBG(pxslog.info("relative index: {}", our_index))
	switch (read_packet[4]) {
		case '\x00': {
			// read pixel
			const uint32_t existing_color = this->state->buffer[our_index];
			PXS_DBG(pxslog.info("read: {}", existing_color))
			unsigned char buffer[4];
			buffer[0] = 0x80;
			buffer[1] = (existing_color >> 24) & 0xFF;
			buffer[2] = (existing_color >> 16) & 0xFF;
			buffer[3] = (existing_color >> 8) & 0xFF;
			// dogshit assumption to prevent ANOTHER move: outside waits for the call to this function to complete before freeing out
			return out.write(reinterpret_cast<char*>(buffer), 4).then([&out] { return out.flush(); });
		}
		case '\x01': {
			// write pixel
			uint32_t to_write = PXS_UNPACK_4B_UINT32(read_packet, 5);
			PXS_DBG(pxslog.info("write: {}", to_write))
			if (const unsigned char alpha = to_write & 0xFF; alpha == 0) {
				// edge case: no transparency; nothing to write since this pixel is fully transparent
				// return out.write("\x81", 1).then([&out] { return out.flush(); });
				return seastar::make_ready_future();
			} else if (alpha != 255) {
				const uint32_t existing1 = this->state->buffer[our_index];
				// normal case: transparency between 1 and 254 inclusive
				const float falpha = alpha / 255.f;
				to_write =
					lerpInt(existing1 >> 24 & 0xFF, to_write >> 24 & 0xFF, falpha) << 24 |
					lerpInt(existing1 >> 16 & 0xFF, to_write >> 16 & 0xFF, falpha) << 16 |
					lerpInt(existing1 >> 8 & 0xFF, to_write >> 8 & 0xFF, falpha) << 8;
			} // else edge case: full transparency; no modification to color
			PXS_DBG(pxslog.info("write actual: {}", to_write))
			this->state->buffer[our_index] = to_write;
			// return out.write("\x81", 1).then([&out] { return out.flush(); });
			return seastar::make_ready_future();
		}
		default:
			break;
	}

	return seastar::make_ready_future();
}

seastar::future<> pixelstar::service_shard::run() {
	seastar::listen_options lo;
	lo.reuse_address = true;
	this->currentSocket = seastar::listen(sa, lo);
	pxslog.info("{} start listening", seastar::this_shard_id());
	return seastar::keep_doing([this] {
		return with_gate(this->gate, [this] {
			pxslog.info("{} accept()", seastar::this_shard_id());
			return this->currentSocket.accept().then([this](seastar::accept_result res) {
				(void) this->handle_connection(std::move(res.connection)).handle_exception(
					[](std::exception_ptr ep) {
						pxslog.error("Could not handle connection: {}\n", ep);
					});
			});
		});
	}).handle_exception_type([](const seastar::gate_closed_exception &e) {
	});
}

seastar::future<> pixelstar::service_shard::stop() {
	pxslog.info("{} stop()", seastar::this_shard_id());
	this->currentSocket.abort_accept();
	pxslog.info("{} abort_accept()", seastar::this_shard_id());
	return this->gate.close();
}
