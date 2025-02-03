#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <iostream>

#include "pixelstar.h"

// seastar::future<> handle_connection(seastar::connected_socket s) {
// 	auto out = s.output();
// 	auto in = s.input();
// 	return do_with(std::move(s), std::move(out), std::move(in),
// 			[] (auto& s, seastar::output_stream<char>& out, seastar::input_stream<char>& in) {
// 		return seastar::repeat([&out, &in] {
// 			return in.read().then([&out] (seastar::temporary_buffer<char> buf) {
// 				if (buf) {
// 					return out.write(std::move(buf))
// 					.then([&out] { std::stringstream ss; ss << "delivered by " << seastar::this_shard_id(); return out.write(seastar::net::packet(ss.str().c_str(), ss.str().length())); })
// 					.then([&out] {
// 						return out.flush();
// 					}).then([] {
// 						return seastar::stop_iteration::no;
// 					});
// 				} else {
// 					return seastar::make_ready_future<seastar::stop_iteration>(
// 							seastar::stop_iteration::yes);
// 				}
// 			});
// 		}).then([&out] {
// 			return out.close();
// 		});
// 	});
// }
//
// seastar::future<> service_loop() {
// 	seastar::listen_options lo;
// 	lo.reuse_address = true;
// 	return seastar::do_with(seastar::listen(seastar::make_ipv4_address({1234}), lo),
// 			[] (auto& listener) {
// 		return seastar::keep_doing([&listener] () {
// 			return listener.accept().then(
// 					[] (seastar::accept_result res) {
// 				// Note we ignore, not return, the future returned by
// 				// handle_connection(), so we do not wait for one
// 				// connection to be handled before accepting the next one.
				// (void)handle_connection(std::move(res.connection)).handle_exception(
				// 		[] (std::exception_ptr ep) {
				// 	fmt::print(stderr, "Could not handle connection: {}\n", ep);
				// });
// 			});
// 		});
// 	});
// }
//
// seastar::future<> f() {
// 	return seastar::parallel_for_each(std::views::iota(0u, seastar::smp::count),
// 			[] (unsigned c) {
// 		return seastar::smp::submit_to(c, service_loop);
// 	});
// }



int main(const int argc, char** argv) {
	try {
		seastar::app_template app;
		app.run(argc, argv, pixelstar::main);
		// app.run(argc, argv, f);
	} catch(...) {
		std::cerr << "Couldn't start application: "
				  << std::current_exception() << "\n";
		return 1;
	}
	return 0;
}