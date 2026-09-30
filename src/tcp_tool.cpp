// Standalone Windows TCP Network Stress & Throughput Tool

// Usage: tcp_stress.exe --host <ip:port> [options]



#include <winsock2.h>

#include <ws2tcpip.h>

#include <windows.h>



#include <atomic>

#include <chrono>

#include <cstdint>

#include <cstdio>

#include <iostream>

#include <mutex>

#include <random>

#include <string>

#include <thread>

#include <vector>



#pragma comment( lib, "Ws2_32.lib" )



namespace

{

	std::atomic< bool > g_stop{ false };

	std::atomic< std::uint64_t > g_packets{ 0 };

	std::atomic< std::uint64_t > g_bytes{ 0 };

	std::atomic< std::uint64_t > g_connects{ 0 };

	std::atomic< std::uint64_t > g_errors{ 0 };

	std::mutex g_output_mutex;



	bool g_quiet = false;



	void say( const std::string& text )

	{

		std::lock_guard< std::mutex > guard( g_output_mutex );

		if ( g_quiet )

			return;

		std::cout << text << '\n';

		std::cout.flush();

	}



	void always( const std::string& text )

	{

		std::lock_guard< std::mutex > guard( g_output_mutex );

		std::cout << text << '\n';

		std::cout.flush();

	}



	std::string human_rate( double bytes_per_second )

	{

		char text[ 64 ]{};

		if ( bytes_per_second >= 1024.0 * 1024.0 )

			sprintf_s( text, "%.2f MiB/s", bytes_per_second / ( 1024.0 * 1024.0 ) );

		else if ( bytes_per_second >= 1024.0 )

			sprintf_s( text, "%.1f KiB/s", bytes_per_second / 1024.0 );

		else

			sprintf_s( text, "%.0f B/s", bytes_per_second );

		return text;

	}



	BOOL WINAPI console_handler( DWORD type )

	{

		if ( type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT )

		{

			g_stop.store( true, std::memory_order_relaxed );

			return TRUE;

		}

		return FALSE;

	}



	struct Options

	{

		std::string host;

		std::string port;

		int threads = 4;

		int size = 4096;

		int delay_ms = 0;

		int duration_sec = 0;

		std::string mode = "null";

	};



	// One connection's fixed-size payload is built once per thread, not per packet: on a saturated

	// link the allocation cost would dwarf the send() itself.

	void run_worker( const Options& options, int worker_index )

	{

		char host_buffer[ 256 ]{};

		strncpy_s( host_buffer, options.host.c_str(), _TRUNCATE );



		sockaddr_storage address{};

		int address_length = 0;

		{

			addrinfo hints{};

			hints.ai_family = AF_UNSPEC;

			hints.ai_socktype = SOCK_STREAM;

			hints.ai_protocol = IPPROTO_TCP;

			addrinfo* result = nullptr;

			if ( getaddrinfo( host_buffer, options.port.c_str(), &hints, &result ) != 0 || !result )

			{

				g_errors.fetch_add( 1, std::memory_order_relaxed );

				say( "[!!] thread " + std::to_string( worker_index ) + ": could not resolve " + options.host );

				return;

			}

			memcpy( &address, result->ai_addr, result->ai_addrlen );

			address_length = static_cast< int >( result->ai_addrlen );

			freeaddrinfo( result );

		}



		std::vector< char > payload( static_cast< size_t >( options.size ), 0 );

		if ( options.mode == "rand" )

		{

			std::mt19937 generator( std::random_device{}() + static_cast< unsigned >( worker_index ) );

			std::uniform_int_distribution< int > distribution( 0, 255 );

			for ( auto& byte : payload )

				byte = static_cast< char >( distribution( generator ) );

		}



		while ( !g_stop.load( std::memory_order_relaxed ) )

		{

			const SOCKET socket_handle = socket( address.ss_family, SOCK_STREAM, IPPROTO_TCP );

			if ( socket_handle == INVALID_SOCKET )

			{

				g_errors.fetch_add( 1, std::memory_order_relaxed );

				std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

				continue;

			}



			// socket timeout configuration

			DWORD timeout_ms = 4000;

			setsockopt( socket_handle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast< const char* >( &timeout_ms ),

				sizeof( timeout_ms ) );



			if ( connect( socket_handle, reinterpret_cast< sockaddr* >( &address ), address_length ) == SOCKET_ERROR )

			{

				g_errors.fetch_add( 1, std::memory_order_relaxed );

				closesocket( socket_handle );

				std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

				continue;

			}



			g_connects.fetch_add( 1, std::memory_order_relaxed );



			if ( options.mode == "connect" )

			{

				closesocket( socket_handle );

				if ( options.delay_ms > 0 )

					std::this_thread::sleep_for( std::chrono::milliseconds( options.delay_ms ) );

				continue;

			}



			while ( !g_stop.load( std::memory_order_relaxed ) )

			{

				const int sent = send( socket_handle, payload.data(), static_cast< int >( payload.size() ), 0 );

				if ( sent == SOCKET_ERROR || sent == 0 )

				{

					g_errors.fetch_add( 1, std::memory_order_relaxed );

					break;

				}

				g_packets.fetch_add( 1, std::memory_order_relaxed );

				g_bytes.fetch_add( static_cast< std::uint64_t >( sent ), std::memory_order_relaxed );

				if ( options.delay_ms > 0 )

					std::this_thread::sleep_for( std::chrono::milliseconds( options.delay_ms ) );

			}



			closesocket( socket_handle );

		}

	}



	int run_probe( const Options& options )

	{

		SOCKET socket_handle = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

		if ( socket_handle == INVALID_SOCKET )

		{

			always( "[!!] could not create a socket" );

			return 1;

		}



		sockaddr_in address{};

		address.sin_family = AF_INET;

		address.sin_port = htons( static_cast< u_short >( atoi( options.port.c_str() ) ) );

		if ( inet_pton( AF_INET, options.host.c_str(), &address.sin_addr ) != 1 )

		{

			addrinfo hints{};

			hints.ai_family = AF_INET;

			hints.ai_socktype = SOCK_STREAM;

			addrinfo* result = nullptr;

			if ( getaddrinfo( options.host.c_str(), options.port.c_str(), &hints, &result ) != 0 || !result )

			{

				always( "[!!] could not resolve " + options.host );

				closesocket( socket_handle );

				return 1;

			}

			memcpy( &address, result->ai_addr, sizeof( sockaddr_in ) );

			freeaddrinfo( result );

		}



		const auto started = std::chrono::steady_clock::now();

		const int result = connect( socket_handle, reinterpret_cast< sockaddr* >( &address ), sizeof( address ) );

		const int last_error = result == 0 ? 0 : WSAGetLastError();

		const auto elapsed = std::chrono::duration_cast< std::chrono::milliseconds >(

			std::chrono::steady_clock::now() - started ).count();

		closesocket( socket_handle );



		if ( result == 0 )

		{

			always( "[OK] reachable: " + options.host + ":" + options.port + " answered in " +

				std::to_string( elapsed ) + " ms" );

			return 0;

		}

		always( "[!!] unreachable: " + options.host + ":" + options.port + " (error " +

			std::to_string( last_error ) + ")" );

		return 1;

	}



	void usage()

	{

		always(

			"femboi tcp tool\n"

			"  tcp_tool.exe --host <ip:port> [--threads N] [--size N] [--delay ms] [--duration sec]\n"

			"               [--mode null|rand|connect|probe] [--quiet]\n"

			"  probe mode answers whether one connection succeeds, then exits.\n"

			"Only target hosts you are explicitly allowed to test." );

	}

} // namespace



int main( int argc, char** argv )

{

	SetConsoleTitleA( "Femboi Tools - TCP Stress" );

	SetConsoleCtrlHandler( console_handler, TRUE );



	Options options;

	for ( int i = 1; i < argc; ++i )

	{

		const std::string argument = argv[ i ];

		auto next = [ & ]( const char* name ) -> std::string

		{

			if ( i + 1 >= argc )

			{

				always( std::string( "[!!] " ) + name + " needs a value" );

				exit( 2 );

			}

			return argv[ ++i ];

		};



		if ( argument == "--host" )

		{

			const auto value = next( "--host" );

			const auto colon = value.find( ':' );

			if ( colon == std::string::npos )

			{

				options.host = value;

			}

			else

			{

				options.host = value.substr( 0, colon );

				options.port = value.substr( colon + 1 );

			}

		}

		else if ( argument == "--port" )

			options.port = next( "--port" );

		else if ( argument == "--threads" )

			options.threads = atoi( next( "--threads" ).c_str() );

		else if ( argument == "--size" )

			options.size = atoi( next( "--size" ).c_str() );

		else if ( argument == "--delay" )

			options.delay_ms = atoi( next( "--delay" ).c_str() );

		else if ( argument == "--duration" )

			options.duration_sec = atoi( next( "--duration" ).c_str() );

		else if ( argument == "--mode" )

			options.mode = next( "--mode" );

		else if ( argument == "--quiet" )

			g_quiet = true;

		else if ( argument == "--help" || argument == "-h" )

		{

			usage();

			return 0;

		}

	}



	if ( options.host.empty() || options.port.empty() )

	{

		usage();

		return 2;

	}



	if ( options.threads < 1 )

		options.threads = 1;

	if ( options.threads > 64 )

		options.threads = 64;

	if ( options.size < 1 )

		options.size = 1;

	if ( options.size > 65536 )

		options.size = 65536;



	WSADATA wsa{};

	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsa ) != 0 )

	{

		always( "[!!] WSAStartup failed" );

		return 1;

	}



	say( "[ii] target " + options.host + ":" + options.port + " mode=" + options.mode +

		" threads=" + std::to_string( options.threads ) + " size=" + std::to_string( options.size ) +

		" delay=" + std::to_string( options.delay_ms ) + "ms" );



	if ( options.mode == "probe" )

	{

		const int result = run_probe( options );

		WSACleanup();

		return result;

	}



	std::vector< std::thread > workers;

	workers.reserve( static_cast< size_t >( options.threads ) );

	for ( int i = 0; i < options.threads; ++i )

		workers.emplace_back( run_worker, options, i );



	const auto started = std::chrono::steady_clock::now();

	auto last = started;

	std::uint64_t last_bytes = 0;



	while ( !g_stop.load( std::memory_order_relaxed ) )

	{

		std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );

		const auto now = std::chrono::steady_clock::now();

		const double seconds = std::chrono::duration< double >( now - last ).count();

		const auto bytes = g_bytes.load( std::memory_order_relaxed );

		const double rate = seconds > 0.0 ? static_cast< double >( bytes - last_bytes ) / seconds : 0.0;

		last = now;

		last_bytes = bytes;



		say( "[ii] t=" + std::to_string( static_cast< long long >(

				std::chrono::duration_cast< std::chrono::seconds >( now - started ).count() ) ) +

			"s  packets=" + std::to_string( g_packets.load( std::memory_order_relaxed ) ) +

			"  connects=" + std::to_string( g_connects.load( std::memory_order_relaxed ) ) +

			"  errors=" + std::to_string( g_errors.load( std::memory_order_relaxed ) ) +

			"  " + human_rate( rate ) );



		if ( options.duration_sec > 0 &&

			std::chrono::duration_cast< std::chrono::seconds >( now - started ).count() >= options.duration_sec )

			g_stop.store( true, std::memory_order_relaxed );

	}



	for ( auto& worker : workers )

	{

		if ( worker.joinable() )

			worker.join();

	}



	const auto elapsed = std::chrono::duration< double >( std::chrono::steady_clock::now() - started ).count();

	const auto bytes = g_bytes.load( std::memory_order_relaxed );

	always( "--- finished: " + std::to_string( g_packets.load( std::memory_order_relaxed ) ) + " packet(s), " +

		std::to_string( bytes / 1024 ) + " KiB, " + std::to_string( g_connects.load( std::memory_order_relaxed ) ) +

		" connection(s), " + std::to_string( g_errors.load( std::memory_order_relaxed ) ) + " error(s) in " +

		std::to_string( static_cast< long long >( elapsed ) ) + "s ---" );



	WSACleanup();

	return 0;

}

