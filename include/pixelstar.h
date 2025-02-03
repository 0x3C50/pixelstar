#pragma once
#include <seastar/core/future.hh>
#include <seastar/core/gate.hh>
#include <seastar/net/api.hh>

namespace pixelstar {
	seastar::future<> main();

	struct global_state {
		uint32_t* buffer;
		size_t width, height;
	};

	class service_shard {
		const global_state* state;
		// uint32_t* bufferStart;
		// size_t responsibleRegionSize;
		seastar::gate gate = seastar::gate();
		seastar::server_socket currentSocket;
	public:
		explicit service_shard(const global_state* state) : state(state) {}
		// important: DO NOT free bufferStart; it is not malloc()d. state is shared, so no free either
		~service_shard() = default;

		seastar::future<> handle_connection(seastar::connected_socket s);

		seastar::future<> run();

		seastar::future<> stop();

		seastar::future<> handle_packet(size_t abs_index, seastar::output_stream<char> &out,
		                                const seastar::temporary_buffer<char> &read_packet) const;
	};
}
